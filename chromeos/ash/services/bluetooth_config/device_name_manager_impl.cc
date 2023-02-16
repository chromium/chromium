// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/bluetooth_config/device_name_manager_impl.h"

#include "chromeos/ash/services/bluetooth_config/public/mojom/cros_bluetooth_config.mojom.h"
#include "components/device_event_log/device_event_log.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "device/bluetooth/chromeos/bluetooth_utils.h"

namespace ash::bluetooth_config {

namespace {

const char kDeviceIdToNicknameMapPrefName[] =
    "bluetooth.device_id_to_nickname_map";

bool IsNicknameValid(const std::string& nickname) {
  if (nickname.empty())
    return false;

  return nickname.size() <= mojom::kDeviceNicknameCharacterLimit;
}

}  // namespace

// static
void DeviceNameManagerImpl::RegisterLocalStatePrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterDictionaryPref(kDeviceIdToNicknameMapPrefName,
                                   base::Value::Dict());
}

DeviceNameManagerImpl::DeviceNameManagerImpl(
    scoped_refptr<device::BluetoothAdapter> bluetooth_adapter)
    : bluetooth_adapter_(std::move(bluetooth_adapter)) {}

DeviceNameManagerImpl::~DeviceNameManagerImpl() = default;

absl::optional<std::string> DeviceNameManagerImpl::GetDeviceNickname(
    const std::string& device_id) {
  if (!local_state_) {
    BLUETOOTH_LOG(ERROR) << "Cannot get device nickname for " << device_id
                         << " because local_state_ is null";
    return absl::nullopt;
  }

  const std::string* nickname =
      local_state_->GetDict(kDeviceIdToNicknameMapPrefName)
          .FindString(device_id);
  if (!nickname)
    return absl::nullopt;

  return *nickname;
}

void DeviceNameManagerImpl::SetDeviceNickname(const std::string& device_id,
                                              const std::string& nickname) {
  if (!IsNicknameValid(nickname)) {
    BLUETOOTH_LOG(ERROR) << "SetDeviceNickname for device with id " << device_id
                         << " failed because nickname is invalid, nickname: "
                         << nickname;
    device::RecordSetDeviceNickName(
        device::SetNicknameResult::kInvalidNicknameFormat);
    return;
  }

  if (!DoesDeviceExist(device_id)) {
    BLUETOOTH_LOG(ERROR) << "SetDeviceNickname for device failed because "
                            "device_id was not found, device_id: "
                         << device_id;
    device::RecordSetDeviceNickName(device::SetNicknameResult::kDeviceNotFound);
    return;
  }

  if (!local_state_) {
    BLUETOOTH_LOG(ERROR) << "SetDeviceNickname for device failed because "
                            "no local_state_ was set, device_id: "
                         << device_id;
    device::RecordSetDeviceNickName(
        device::SetNicknameResult::kPrefsUnavailable);
    return;
  }

  ScopedDictPrefUpdate update(local_state_, kDeviceIdToNicknameMapPrefName);

  BLUETOOTH_LOG(USER) << "Setting device nickname for " << device_id << " to "
                      << nickname;
  update->Set(device_id, nickname);

  NotifyDeviceNicknameChanged(device_id, nickname);
  device::RecordSetDeviceNickName(device::SetNicknameResult::kSuccess);
}

void DeviceNameManagerImpl::RemoveDeviceNickname(const std::string& device_id) {
  if (!local_state_) {
    BLUETOOTH_LOG(ERROR) << "RemoveDeviceNickname for device failed because "
                         << "no local_state_ was set, device_id: " << device_id;
    return;
  }

  ScopedDictPrefUpdate update(local_state_, kDeviceIdToNicknameMapPrefName);

  // Do nothing if no nickname exists for |device_id|.
  if (!update->Find(device_id)) {
    BLUETOOTH_LOG(ERROR) << "RemoveDeviceNickname for device failed because no "
                         << "nickname exists for " << device_id;
    return;
  }

  BLUETOOTH_LOG(EVENT) << "Removing device nickname for " << device_id;
  update->Remove(device_id);
  NotifyDeviceNicknameChanged(device_id, /*nickname=*/absl::nullopt);
}

void DeviceNameManagerImpl::SetPrefs(PrefService* local_state) {
  local_state_ = local_state;
}

bool DeviceNameManagerImpl::DoesDeviceExist(
    const std::string& device_id) const {
  for (auto* device : bluetooth_adapter_->GetDevices()) {
    if (device->GetIdentifier() == device_id)
      return true;
  }
  return false;
}

}  // namespace ash::bluetooth_config
