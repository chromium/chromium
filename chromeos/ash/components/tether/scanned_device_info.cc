// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/tether/scanned_device_info.h"

namespace ash::tether {

ScannedDeviceInfo::ScannedDeviceInfo(const std::string& device_id,
                                     const std::string& device_name,
                                     std::optional<DeviceStatus> device_status,
                                     bool setup_required,
                                     bool notifications_enabled)
    : device_id(device_id),
      device_name(device_name),
      device_status(device_status),
      notifications_enabled(notifications_enabled),
      setup_required(setup_required) {}

ScannedDeviceInfo::ScannedDeviceInfo(const ScannedDeviceInfo& other)
    : device_id(other.device_id),
      device_name(other.device_name),
      device_status(other.device_status),
      notifications_enabled(other.notifications_enabled),
      setup_required(other.setup_required) {}

ScannedDeviceInfo::~ScannedDeviceInfo() = default;

bool operator==(const ScannedDeviceInfo& first,
                const ScannedDeviceInfo& second) {
  if (first.device_status.has_value()) {
    if (!second.device_status.has_value() ||
        first.device_status->SerializeAsString() !=
            second.device_status->SerializeAsString()) {
      return false;
    }
  } else if (second.device_status.has_value()) {
    return false;
  }

  return first.device_id == second.device_id &&
         first.notifications_enabled == second.notifications_enabled &&
         first.device_name == second.device_name &&
         first.setup_required == second.setup_required;
}

}  // namespace ash::tether
