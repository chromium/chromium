// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/bluetooth_config/device_name_manager.h"

namespace chromeos {
namespace bluetooth_config {

DeviceNameManager::DeviceNameManager() = default;

DeviceNameManager::~DeviceNameManager() = default;

void DeviceNameManager::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void DeviceNameManager::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void DeviceNameManager::NotifyDeviceNicknameChanged(
    const std::string& device_id,
    const absl::optional<std::string>& nickname) {
  for (auto& observer : observers_)
    observer.OnDeviceNicknameChanged(device_id, nickname);
}

}  // namespace bluetooth_config
}  // namespace chromeos
