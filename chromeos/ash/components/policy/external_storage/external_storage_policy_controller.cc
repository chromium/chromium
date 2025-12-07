// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/policy/external_storage/external_storage_policy_controller.h"

#include "base/values.h"
#include "chromeos/ash/components/policy/external_storage/device_id.h"
#include "chromeos/components/disks/disks_prefs.h"
#include "components/prefs/pref_service.h"

using ::disks::prefs::kExternalStorageAllowlist;
using ::disks::prefs::kExternalStorageDisabled;
using ::disks::prefs::kExternalStorageReadOnly;

namespace policy {

// static
bool ExternalStoragePolicyController::IsExternalStorageDisabled(
    const PrefService& pref_service) {
  return pref_service.GetBoolean(kExternalStorageDisabled);
}

// static
bool ExternalStoragePolicyController::IsExternalStorageReadOnly(
    const PrefService& pref_service) {
  return pref_service.GetBoolean(kExternalStorageReadOnly);
}

// static
bool ExternalStoragePolicyController::IsDeviceAllowlisted(
    const PrefService& pref_service,
    std::optional<DeviceId> device_id) {
  if (!device_id.has_value()) {
    return false;
  }

  for (const base::Value& entry :
       pref_service.GetList(kExternalStorageAllowlist)) {
    if (device_id == DeviceId::FromDict(entry)) {
      return true;
    }
  }

  return false;
}

// static
bool ExternalStoragePolicyController::IsDeviceDisabled(
    const PrefService& pref_service,
    std::optional<DeviceId> device_id) {
  return IsExternalStorageDisabled(pref_service) &&
         !IsDeviceAllowlisted(pref_service, device_id);
}

// static
bool ExternalStoragePolicyController::IsDeviceReadOnly(
    const PrefService& pref_service,
    std::optional<DeviceId> device_id) {
  return pref_service.GetBoolean(kExternalStorageReadOnly) &&
         !IsDeviceAllowlisted(pref_service, device_id);
}

// static
bool ExternalStoragePolicyController::IsDeviceWriteable(
    const PrefService& pref_service,
    std::optional<DeviceId> device_id) {
  return (!pref_service.GetBoolean(kExternalStorageDisabled) &&
          !pref_service.GetBoolean(kExternalStorageReadOnly)) ||
         IsDeviceAllowlisted(pref_service, device_id);
}

}  // namespace policy
