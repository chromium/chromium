// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_POLICY_EXTERNAL_STORAGE_EXTERNAL_STORAGE_POLICY_CONTROLLER_H_
#define CHROMEOS_ASH_COMPONENTS_POLICY_EXTERNAL_STORAGE_EXTERNAL_STORAGE_POLICY_CONTROLLER_H_

#include "base/component_export.h"

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
};

}  // namespace policy

#endif  // CHROMEOS_ASH_COMPONENTS_POLICY_EXTERNAL_STORAGE_EXTERNAL_STORAGE_POLICY_CONTROLLER_H_
