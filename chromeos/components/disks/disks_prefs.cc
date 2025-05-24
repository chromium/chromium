// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/disks/disks_prefs.h"

#include "components/prefs/pref_registry_simple.h"

namespace disks::prefs {

// A pref holding the value of the policy used to disable mounting of external
// storage for the user.
const char kExternalStorageDisabled[] = "hardware.external_storage_disabled";

// A pref holding the value of the policy used to limit mounting of external
// storage to read-only mode for the user.
const char kExternalStorageReadOnly[] = "hardware.external_storage_read_only";

// A list of VID:PID pairs which specify USB mass storage devices which are
// exempt from `kExternalStorageDisabled` and `kExternalStorageReadOnly`.
// Example content:
// [
//   {
//     "vendor_id": 0x1234,
//     "product_id": 0x5678
//   },
//   {
//     "vendor_id": 0x9abc,
//     "product_id": 0xdef0
//   }
// ]
const char kExternalStorageAllowlist[] = "hardware.external_storage_allowlist";

void RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(kExternalStorageDisabled, false);
  registry->RegisterBooleanPref(kExternalStorageReadOnly, false);
  registry->RegisterListPref(kExternalStorageAllowlist);
}

}  // namespace disks::prefs
