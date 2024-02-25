// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/bluetooth_config/fake_device_name_manager.h"

namespace ash::bluetooth_config {

FakeDeviceNameManager::FakeDeviceNameManager() = default;

FakeDeviceNameManager::~FakeDeviceNameManager() = default;

std::optional<std::string> FakeDeviceNameManager::GetDeviceNickname(
    const std::string& device_id) {
  base::flat_map<std::string, std::string>::iterator it =
      device_id_to_nickname_map_.find(device_id);
  if (it == device_id_to_nickname_map_.end())
    return std::nullopt;

  return it->second;
}

void FakeDeviceNameManager::SetDeviceNickname(const std::string& device_id,
                                              const std::string& nickname) {
  device_id_to_nickname_map_[device_id] = nickname;
  NotifyDeviceNicknameChanged(device_id, nickname);
}

void FakeDeviceNameManager::RemoveDeviceNickname(const std::string& device_id) {
  device_id_to_nickname_map_.erase(device_id);
  NotifyDeviceNicknameChanged(device_id, /*nickname=*/std::nullopt);
}

}  // namespace ash::bluetooth_config
