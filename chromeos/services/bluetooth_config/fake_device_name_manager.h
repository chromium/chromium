// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_BLUETOOTH_CONFIG_FAKE_DEVICE_NAME_MANAGER_H_
#define CHROMEOS_SERVICES_BLUETOOTH_CONFIG_FAKE_DEVICE_NAME_MANAGER_H_

#include "chromeos/services/bluetooth_config/device_name_manager.h"

#include "base/containers/flat_map.h"

class PrefService;

namespace chromeos {
namespace bluetooth_config {

class FakeDeviceNameManager : public DeviceNameManager {
 public:
  FakeDeviceNameManager();
  ~FakeDeviceNameManager() override;

  // DeviceNameManager:
  absl::optional<std::string> GetDeviceNickname(
      const std::string& device_id) override;
  void SetDeviceNickname(const std::string& device_id,
                         const std::string& nickname) override;
  void RemoveDeviceNickname(const std::string& device_id) override;
  void SetPrefs(PrefService* local_state) override {}

 private:
  base::flat_map<std::string, std::string> device_id_to_nickname_map_;
};

}  // namespace bluetooth_config
}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_BLUETOOTH_CONFIG_FAKE_DEVICE_NAME_MANAGER_H_
