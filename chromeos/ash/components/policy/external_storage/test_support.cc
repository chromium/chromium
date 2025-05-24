// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/policy/external_storage/test_support.h"

#include "base/values.h"
#include "chromeos/ash/components/policy/external_storage/device_id.h"
#include "chromeos/components/disks/disks_prefs.h"
#include "components/prefs/pref_service.h"

using ::disks::prefs::kExternalStorageAllowlist;
using ::disks::prefs::kExternalStorageDisabled;
using ::disks::prefs::kExternalStorageReadOnly;

namespace policy::external_storage {

void SetDisabled(PrefService& pref_service, bool disabled) {
  pref_service.SetBoolean(kExternalStorageDisabled, disabled);
}

void SetReadOnly(PrefService& pref_service, bool read_only) {
  pref_service.SetBoolean(kExternalStorageReadOnly, read_only);
}

void SetAllowlist(PrefService& pref_service,
                  std::initializer_list<DeviceId> allowlist) {
  base::Value::List list;

  for (const DeviceId& device_id : allowlist) {
    list.Append(device_id.ToDict());
  }

  pref_service.SetList(kExternalStorageAllowlist, std::move(list));
}

void SetAllowlist(PrefService& pref_service, DeviceId device_id) {
  SetAllowlist(pref_service, {device_id});
}

}  // namespace policy::external_storage
