// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/built_in_backend_to_android_backend_migrator.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_service.h"

namespace password_manager {

BuiltInBackendToAndroidBackendMigrator::BuiltInBackendToAndroidBackendMigrator(
    PrefService* prefs)
    : prefs_(prefs) {}

BuiltInBackendToAndroidBackendMigrator::
    ~BuiltInBackendToAndroidBackendMigrator() = default;

void BuiltInBackendToAndroidBackendMigrator::StartMigrationIfNecessary() {
  if (features::kMigrationVersion.Get() >
      prefs_->GetInteger(
          prefs::kCurrentMigrationVersionToGoogleMobileServices)) {
    // TODO:(crbug.com/1252443) Implement actual migration.
    UpdateMigrationVersionInPref();
  }
}

void BuiltInBackendToAndroidBackendMigrator::UpdateMigrationVersionInPref() {
  prefs_->SetInteger(prefs::kCurrentMigrationVersionToGoogleMobileServices,
                     features::kMigrationVersion.Get());
}

}  // namespace password_manager
