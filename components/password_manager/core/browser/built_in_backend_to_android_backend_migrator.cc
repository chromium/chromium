// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/built_in_backend_to_android_backend_migrator.h"

namespace password_manager {

BuiltInBackendToAndroidBackendMigrator::
    BuiltInBackendToAndroidBackendMigrator() = default;

BuiltInBackendToAndroidBackendMigrator::
    ~BuiltInBackendToAndroidBackendMigrator() = default;

void BuiltInBackendToAndroidBackendMigrator::StartMigrationIfNecessary() {
  // TODO:(crbug.com/1252443) Check current migration version and version
  // saved in pref. If current version is higher, start migration.
}

void BuiltInBackendToAndroidBackendMigrator::UpdateMigrationVersionInPref() {
  // TODO:(crbug.com/1252443) Save current migration version in pref.
}

}  // namespace password_manager
