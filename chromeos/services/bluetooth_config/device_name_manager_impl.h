// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_BLUETOOTH_CONFIG_DEVICE_NAME_MANAGER_IMPL_H_
#define CHROMEOS_SERVICES_BLUETOOTH_CONFIG_DEVICE_NAME_MANAGER_IMPL_H_

#include "chromeos/services/bluetooth_config/device_name_manager.h"

class PrefRegistrySimple;
class PrefService;

namespace chromeos {
namespace bluetooth_config {

// Concrete DeviceNameManager implementation that saves entries into Prefs.
class DeviceNameManagerImpl : public DeviceNameManager {
 public:
  static void RegisterPrefs(PrefRegistrySimple* registry);

  explicit DeviceNameManagerImpl(PrefService* pref_service);
  ~DeviceNameManagerImpl() override;

  // DeviceNameManager:
  absl::optional<std::string> GetDeviceNickname(
      const std::string& device_id) override;
  void SetDeviceNickname(const std::string& device_id,
                         const std::string& nickname) override;

 private:
  PrefService* pref_service_;
};

}  // namespace bluetooth_config
}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_BLUETOOTH_CONFIG_DEVICE_NAME_MANAGER_IMPL_H_
