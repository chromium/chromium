// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/bluetooth_config/device_name_manager_impl.h"

#include "chromeos/services/bluetooth_config/public/mojom/cros_bluetooth_config.mojom.h"
#include "components/device_event_log/device_event_log.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"

namespace chromeos {
namespace bluetooth_config {
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
void DeviceNameManagerImpl::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterDictionaryPref(kDeviceIdToNicknameMapPrefName,
                                   base::Value(base::Value::Type::DICTIONARY));
}

DeviceNameManagerImpl::DeviceNameManagerImpl(
    scoped_refptr<device::BluetoothAdapter> bluetooth_adapter,
    PrefService* pref_service)
    : bluetooth_adapter_(std::move(bluetooth_adapter)),
      pref_service_(pref_service) {}

DeviceNameManagerImpl::~DeviceNameManagerImpl() = default;

absl::optional<std::string> DeviceNameManagerImpl::GetDeviceNickname(
    const std::string& device_id) {
  const std::string* nickname =
      pref_service_->GetDictionary(kDeviceIdToNicknameMapPrefName)
          ->FindStringKey(device_id);
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
    return;
  }

  if (!DoesDeviceExist(device_id)) {
    BLUETOOTH_LOG(ERROR) << "SetDeviceNickname for device failed because "
                            "device_id was not found, device_id: "
                         << device_id;
    return;
  }

  base::DictionaryValue* device_id_to_nickname_map =
      DictionaryPrefUpdate(pref_service_, kDeviceIdToNicknameMapPrefName).Get();
  DCHECK(device_id_to_nickname_map)
      << "Device ID to nickname map pref is unregistered.";
  device_id_to_nickname_map->SetStringKey(device_id, nickname);
}

bool DeviceNameManagerImpl::DoesDeviceExist(
    const std::string& device_id) const {
  for (auto* device : bluetooth_adapter_->GetDevices()) {
    if (device->GetIdentifier() == device_id)
      return true;
  }
  return false;
}

}  // namespace bluetooth_config
}  // namespace chromeos
