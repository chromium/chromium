// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/bluetooth_config/device_name_manager.h"

namespace ash::bluetooth_config {

const char DeviceNameManager::kDeviceIdToNicknameMapPrefName[] =
    "bluetooth.floss_device_id_to_nickname_map";

const char DeviceNameManager::kDeviceIdToNicknameMapPrefNameLegacy[] =
    "bluetooth.device_id_to_nickname_map";

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
    const std::optional<std::string>& nickname) {
  for (auto& observer : observers_)
    observer.OnDeviceNicknameChanged(device_id, nickname);
}

}  // namespace ash::bluetooth_config
