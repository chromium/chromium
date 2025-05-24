// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_POLICY_EXTERNAL_STORAGE_EXTERNAL_STORAGE_POLICY_CONTROLLER_H_
#define CHROMEOS_ASH_COMPONENTS_POLICY_EXTERNAL_STORAGE_EXTERNAL_STORAGE_POLICY_CONTROLLER_H_

#include <optional>

#include "base/component_export.h"
#include "chromeos/ash/components/policy/external_storage/device_id.h"

class PrefService;

namespace policy {

// Handles external storage policies and does any reconciliation between them.
// Design document: go/gb-external-storage-allowlist.
class COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_POLICY)
    ExternalStoragePolicyController {
 public:
  ExternalStoragePolicyController() = delete;

  static bool IsExternalStorageDisabled(const PrefService& pref_service);
  static bool IsExternalStorageReadOnly(const PrefService& pref_service);

  static bool IsDeviceAllowlisted(const PrefService& pref_service,
                                  std::optional<DeviceId> device_id);
  static bool IsDeviceDisabled(const PrefService& pref_service,
                               std::optional<DeviceId> device_id);
  static bool IsDeviceReadOnly(const PrefService& pref_service,
                               std::optional<DeviceId> device_id);
  static bool IsDeviceWriteable(const PrefService& pref_service,
                                std::optional<DeviceId> device_id);
};

}  // namespace policy

#endif  // CHROMEOS_ASH_COMPONENTS_POLICY_EXTERNAL_STORAGE_EXTERNAL_STORAGE_POLICY_CONTROLLER_H_
