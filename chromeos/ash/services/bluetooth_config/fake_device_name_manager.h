// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_BLUETOOTH_CONFIG_FAKE_DEVICE_NAME_MANAGER_H_
#define CHROMEOS_ASH_SERVICES_BLUETOOTH_CONFIG_FAKE_DEVICE_NAME_MANAGER_H_

#include "chromeos/ash/services/bluetooth_config/device_name_manager.h"

#include "base/containers/flat_map.h"

class PrefService;

namespace ash::bluetooth_config {

class FakeDeviceNameManager : public DeviceNameManager {
 public:
  FakeDeviceNameManager();
  ~FakeDeviceNameManager() override;

  // DeviceNameManager:
  std::optional<std::string> GetDeviceNickname(
      const std::string& device_id) override;
  void SetDeviceNickname(const std::string& device_id,
                         const std::string& nickname) override;
  void RemoveDeviceNickname(const std::string& device_id) override;
  void SetPrefs(PrefService* local_state) override {}

 private:
  base::flat_map<std::string, std::string> device_id_to_nickname_map_;
};

}  // namespace ash::bluetooth_config

#endif  // CHROMEOS_ASH_SERVICES_BLUETOOTH_CONFIG_FAKE_DEVICE_NAME_MANAGER_H_
