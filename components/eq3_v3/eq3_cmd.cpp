#include "eq3.h"
#include "eq3_const.h"
#include "esphome/core/log.h"

using namespace esphome;

static const char *TAG = "eq3_cmd";

static uint8_t temp_to_dev(const float &value) {
  if (value < EQ3BT_MIN_TEMP)
    return uint8_t(EQ3BT_MIN_TEMP * 2);
  else if (value > EQ3BT_MAX_TEMP)
    return uint8_t(EQ3BT_MAX_TEMP * 2);
  else
    return uint8_t(value * 2);
}

bool EQ3Climate::with_connection(std::function<bool()> handler) {
  bool success = true;

  success = success && connect();
  success = success && handler();
  success = success && wait_for_notify();
  disconnect();

  return success;
}

bool EQ3Climate::connect() {

  ESP_LOGV(TAG, "Connecting to %10llx...", address);
  
  return true;
}

void EQ3Climate::disconnect() {
  ESP_LOGV(TAG, "Disconnected from %10llx.", address);
}

bool EQ3Climate::send_command(void *command, uint16_t length) {
    ESP_LOGV(TAG, "Sent of `%s` to %10llx to handle %04x.",
      format_hex_pretty((const uint8_t*)command, length).c_str(), address, command_handle);
  
  return true;
}

bool EQ3Climate::wait_for_notify(int timeout_ms) {
  ESP_LOGV(TAG, "Received notification for %10llx.", address);
  return true;
}

bool EQ3Climate::query_id() {
  uint8_t command[] = {PROP_ID_QUERY};
  return send_command(command, sizeof(command));
}

bool EQ3Climate::query_state() {
  if (!time_clock || !time_clock->now().is_valid()) {
    ESP_LOGE(TAG, "Clock source for %10llx is not valid.", address);
    return false;
  }

  auto now = time_clock->now();

  uint8_t command[] = {
    PROP_INFO_QUERY,
    uint8_t(now.year % 100),
    now.month,
    now.day_of_month,
    now.hour,
    now.minute,
    now.second
  };

  return send_command(command, sizeof(command));
}

bool EQ3Climate::query_schedule(EQ3Day day) {
  if (day > EQ3_LastDay) {
    return false;
  }

  uint8_t command[] = {PROP_SCHEDULE_QUERY, day};
  return send_command(command, sizeof(command));
}

bool EQ3Climate::set_temperature(float temperature) {
  uint8_t command[2];

  if (temperature <= EQ3BT_OFF_TEMP) {
    command[0] = PROP_MODE_WRITE;
    command[1] = uint8_t(EQ3BT_OFF_TEMP * 2) | 0x40;
  } else if (temperature >= EQ3BT_ON_TEMP) {
    command[0] = PROP_MODE_WRITE;
    command[1] = uint8_t(EQ3BT_ON_TEMP * 2) | 0x40;
  } else {
    command[0] = PROP_TEMPERATURE_WRITE;
    command[1] = uint8_t(temperature * 2);
  }

  return send_command(command, sizeof(command));
}

bool EQ3Climate::set_auto_mode() {
  uint8_t command[] = {
    PROP_MODE_WRITE,
    0
  };

  return send_command(command, sizeof(command));
}

bool EQ3Climate::set_manual_mode() {
  uint8_t command[] = {
    PROP_MODE_WRITE,
    0x40
  };

  return send_command(command, sizeof(command));
}

bool EQ3Climate::set_boost_mode(bool enabled) {
  uint8_t command[] = {
    PROP_BOOST,
    uint8_t(enabled ? 1 : 0)
  };

  return send_command(command, sizeof(command));
}

bool EQ3Climate::set_temperature_offset(float offset) {
  // [-3,5 .. 0  .. 3,5 ]
  // [00   .. 07 .. 0e ]

  if (offset < -3.5 || offset > 3.5) {
    return false;
  }

  uint8_t command[] = {
    PROP_OFFSET,
    uint8_t((offset + 3.5) * 2)
  };

  return send_command(command, sizeof(command));
}

bool EQ3Climate::set_temperature_type(int eco) {
  uint8_t command[] = {
    eco ? PROP_ECO : PROP_COMFORT
  };

  return send_command(command, sizeof(command));
}

bool EQ3Climate::set_temperature_presets(float comfort, float eco) {
  uint8_t command[] = {
    PROP_COMFORT_ECO_CONFIG,
    temp_to_dev(comfort),
    temp_to_dev(eco)
  };

  return send_command(command, sizeof(command));
}

bool EQ3Climate::set_locked(bool locked) {
  uint8_t command[] = {
    PROP_LOCK,
    uint8_t(locked ? 1 : 0)
  };

  return send_command(command, sizeof(command));
}

bool EQ3Climate::set_window_config(int seconds, float temperature) {
  if (seconds < 0 || seconds > 3600) {
    return false;
  }

  uint8_t command[] = {
    PROP_WINDOW_OPEN_CONFIG,
    temp_to_dev(temperature),
    uint8_t(seconds / 300)
  };

  return send_command(command, sizeof(command));
}

void EQ3Climate::parse_client_notify(std::string data) {
  if (data.size() >= 2 && data[0] == PROP_INFO_RETURN && data[1] == 0x1) {
    defer("parse_notify", [this, data]() {
      parse_state(data);
    });
  } else if (data.size() >= 1 && data[0] == PROP_SCHEDULE_RETURN) {
    defer("parse_schedule_" + to_string(data[1]), [this, data]() {
      parse_schedule(data);
    });
  } else if (data.size() >= 1 && data[0] == PROP_ID_RETURN) {
    defer("parse_id", [this, data]() {
      parse_id(data);
    });
  } else {
    ESP_LOGW(TAG, "Received unknown characteristic from %10llx: %s.",
      address,
      format_hex_pretty((const uint8_t*)data.c_str(), data.size()).c_str());
  }
}
