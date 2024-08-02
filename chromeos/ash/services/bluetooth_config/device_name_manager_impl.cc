// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chromeos/ash/services/bluetooth_config/device_name_manager_impl.h"

#include "base/containers/span.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "chromeos/ash/services/bluetooth_config/public/mojom/cros_bluetooth_config.mojom.h"
#include "components/device_event_log/device_event_log.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "device/bluetooth/chromeos/bluetooth_utils.h"
#include "device/bluetooth/floss/floss_features.h"
#include "third_party/re2/src/re2/re2.h"

namespace ash::bluetooth_config {

namespace {

bool IsNicknameValid(const std::string& nickname) {
  if (nickname.empty())
    return false;

  return nickname.size() <= mojom::kDeviceNicknameCharacterLimit;
}

}  // namespace

// static
void DeviceNameManagerImpl::RegisterLocalStatePrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterDictionaryPref(kDeviceIdToNicknameMapPrefNameLegacy);
  registry->RegisterDictionaryPref(kDeviceIdToNicknameMapPrefName);
}

DeviceNameManagerImpl::DeviceNameManagerImpl(
    scoped_refptr<device::BluetoothAdapter> bluetooth_adapter)
    : bluetooth_adapter_(std::move(bluetooth_adapter)) {}

DeviceNameManagerImpl::~DeviceNameManagerImpl() = default;

std::optional<std::string> DeviceNameManagerImpl::GetDeviceNickname(
    const std::string& device_id) {
  if (!local_state_) {
    BLUETOOTH_LOG(ERROR) << "Cannot get device nickname for " << device_id
                         << " because local_state_ is null";
    return std::nullopt;
  }

  const std::string* nickname =
      local_state_
          ->GetDict(floss::features::IsFlossEnabled()
                        ? kDeviceIdToNicknameMapPrefName
                        : kDeviceIdToNicknameMapPrefNameLegacy)
          .FindString(device_id);
  if (!nickname)
    return std::nullopt;

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

  ScopedDictPrefUpdate update(local_state_,
                              floss::features::IsFlossEnabled()
                                  ? kDeviceIdToNicknameMapPrefName
                                  : kDeviceIdToNicknameMapPrefNameLegacy);

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

  ScopedDictPrefUpdate update(local_state_,
                              floss::features::IsFlossEnabled()
                                  ? kDeviceIdToNicknameMapPrefName
                                  : kDeviceIdToNicknameMapPrefNameLegacy);

  // Do nothing if no nickname exists for |device_id|.
  if (!update->Find(device_id)) {
    BLUETOOTH_LOG(ERROR) << "RemoveDeviceNickname for device failed because no "
                         << "nickname exists for " << device_id;
    return;
  }

  BLUETOOTH_LOG(EVENT) << "Removing device nickname for " << device_id;
  update->Remove(device_id);
  NotifyDeviceNicknameChanged(device_id, /*nickname=*/std::nullopt);
}

void DeviceNameManagerImpl::SetPrefs(PrefService* local_state) {
  local_state_ = local_state;

  if (!local_state_) {
    return;
  }
  const bool hasPref =
      local_state_->HasPrefPath(kDeviceIdToNicknameMapPrefName);
  if (!floss::features::IsFlossEnabled()) {
    if (hasPref) {
      local_state_->ClearPref(kDeviceIdToNicknameMapPrefName);
    }
    return;
  }
  if (!hasPref) {
    MigrateExistingNicknames();
  }
}

void DeviceNameManagerImpl::MigrateExistingNicknames() {
  DCHECK(floss::features::IsFlossEnabled());
  DCHECK(local_state_);

  BLUETOOTH_LOG(EVENT) << "Starting migration of Bluetooth nickname prefs";

  const re2::RE2 kFlossIdRegex("^([0-9A-Fa-f]{2}:){5}[0-9A-Fa-f]{2}$");

  const base::Value::Dict& existing_prefs =
      local_state_->GetDict(kDeviceIdToNicknameMapPrefNameLegacy);

  for (const auto [id, nickname] : existing_prefs) {
    // Check whether the ID already matches the Floss format. If so, simply copy
    // the entry into the migrated prefs.
    if (re2::RE2::FullMatch(id, kFlossIdRegex)) {
      BLUETOOTH_LOG(EVENT)
          << "Device ID format already matches Floss format; overwriting entry";
      ScopedDictPrefUpdate update(local_state_, kDeviceIdToNicknameMapPrefName);
      update->Set(id, nickname.Clone());
      continue;
    }

    // Since BlueZ uses the DBus object path of the device as the ID, which
    // includes the parts of the address of the device, we are able to extract
    // these parts and use them to generate an ID for the device that follows
    // the format expected of a Floss device ID. Using the following ID as an
    // example:
    //
    //   /org/bluez/hci0/dev_7C_96_D2_8B_FB_17  ==>  7C:96:D2:8B:FB:17
    //
    // We start by splitting the ID using underscores as the delimiter and then
    // concatenating the last six parts, which correspond to the parts of the
    // Bluetooth address of the device, using colons to delineate.
    const std::vector<std::string> parts =
        base::SplitString(id, "_", base::WhitespaceHandling::TRIM_WHITESPACE,
                          base::SplitResult::SPLIT_WANT_NONEMPTY);

    // We expect at least seven different parts; six for the device address, and
    // at least one additional part for the non-address prefix.
    if (parts.size() < 7) {
      BLUETOOTH_LOG(ERROR) << "Failed to migrate Bluetooth nickname, device ID "
                           << "has an unexpected format: " << id;
      continue;
    }

    auto floss_id =
        base::JoinString(base::make_span(parts.end() - 6, parts.end()), ":");

    // Avoid overwriting an existing entry with a BlueZ nickname. This allows us
    // to guarantee that Floss nicknames are migrated and outdate BlueZ
    // nicknames are effectively forgotten.
    if (local_state_->GetDict(kDeviceIdToNicknameMapPrefName)
            .contains(floss_id)) {
      BLUETOOTH_LOG(EVENT) << "Device ID already exists; skipping entry";
      continue;
    }

    ScopedDictPrefUpdate update(local_state_, kDeviceIdToNicknameMapPrefName);
    update->Set(floss_id, nickname.Clone());

    BLUETOOTH_LOG(EVENT) << "Successfully migrated Bluetooth nickname pref";
  }
  BLUETOOTH_LOG(EVENT) << "Finished migration of Bluetooth nickname prefs";
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
