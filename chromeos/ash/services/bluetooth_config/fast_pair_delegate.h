// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_BLUETOOTH_CONFIG_FAST_PAIR_DELEGATE_H_
#define CHROMEOS_ASH_SERVICES_BLUETOOTH_CONFIG_FAST_PAIR_DELEGATE_H_

#include "third_party/abseil-cpp/absl/types/optional.h"

namespace ash::bluetooth_config {

class AdapterStateController;
class DeviceImageInfo;
class DeviceNameManager;

// Delegate class used to connect the bluetooth_config and Fast Pair systems,
// which live in different parts of the dependency tree and cannot directly
// call each other.
class FastPairDelegate {
 public:
  virtual ~FastPairDelegate() = default;

  virtual absl::optional<DeviceImageInfo> GetDeviceImageInfo(
      const std::string& mac_address) = 0;
  virtual void ForgetDevice(const std::string& mac_address) = 0;
  virtual void UpdateDeviceNickname(const std::string& mac_address,
                                    const std::string& nickname) = 0;
  virtual void SetAdapterStateController(
      AdapterStateController* adapter_state_controller) = 0;
  virtual void SetDeviceNameManager(DeviceNameManager* device_name_manager) = 0;
};

}  // namespace ash::bluetooth_config

#endif  // CHROMEOS_ASH_SERVICES_BLUETOOTH_CONFIG_FAST_PAIR_DELEGATE_H_
