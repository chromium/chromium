// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_POLICY_EXTERNAL_STORAGE_TEST_SUPPORT_H_
#define CHROMEOS_ASH_COMPONENTS_POLICY_EXTERNAL_STORAGE_TEST_SUPPORT_H_

#include <initializer_list>

#include "chromeos/ash/components/policy/external_storage/device_id.h"

class PrefService;

namespace policy::external_storage {

// Sets the value of the ExternalStorageDisabled policy.
void SetDisabled(PrefService& pref_service, bool disabled);

// Sets the value of the ExternalStorageReadOnly policy.
void SetReadOnly(PrefService& pref_service, bool read_only);

// Sets the value of the ExternalStorageAllowlist policy.
void SetAllowlist(PrefService& pref_service,
                  std::initializer_list<DeviceId> allowlist);
void SetAllowlist(PrefService& pref_service, DeviceId device_id);

}  // namespace policy::external_storage

#endif  // CHROMEOS_ASH_COMPONENTS_POLICY_EXTERNAL_STORAGE_TEST_SUPPORT_H_
