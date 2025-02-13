// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/policy/external_storage/external_storage_policy_controller.h"

#include "chromeos/components/disks/disks_prefs.h"
#include "components/prefs/pref_service.h"

namespace policy {

// static
bool ExternalStoragePolicyController::IsExternalStorageDisabled(
    const PrefService& pref_service) {
  return pref_service.GetBoolean(disks::prefs::kExternalStorageDisabled);
}

// static
bool ExternalStoragePolicyController::IsExternalStorageReadOnly(
    const PrefService& pref_service) {
  return pref_service.GetBoolean(disks::prefs::kExternalStorageReadOnly);
}

}  // namespace policy
