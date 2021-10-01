// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_BLUETOOTH_CONFIG_DEVICE_NAME_MANAGER_H_
#define CHROMEOS_SERVICES_BLUETOOTH_CONFIG_DEVICE_NAME_MANAGER_H_

#include <string>

#include "base/callback.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace chromeos {
namespace bluetooth_config {

// Manages saving and retrieving nicknames for Bluetooth devices. This nickname
// is local to only the device and is visible to all users of the device.
class DeviceNameManager {
 public:
  virtual ~DeviceNameManager() = default;

  // Retrieves the nickname of the Bluetooth device with ID |device_id| or
  // abs::nullopt if not found.
  virtual absl::optional<std::string> GetDeviceNickname(
      const std::string& device_id) = 0;

  // Sets the nickname of the Bluetooth device with ID |device_id| for all users
  // of the current device, if |nickname| is valid.
  virtual void SetDeviceNickname(const std::string& device_id,
                                 const std::string& nickname) = 0;

 protected:
  DeviceNameManager() = default;
};

}  // namespace bluetooth_config
}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_BLUETOOTH_CONFIG_DEVICE_NAME_MANAGER_H_
