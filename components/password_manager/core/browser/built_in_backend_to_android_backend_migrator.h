// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_BUILT_IN_BACKEND_TO_ANDROID_BACKEND_MIGRATOR_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_BUILT_IN_BACKEND_TO_ANDROID_BACKEND_MIGRATOR_H_

#include "base/callback.h"
#include "base/callback_forward.h"

class PrefService;

namespace password_manager {
// Instantiate this object to migrate all password stored in the built-in
// backend to the Android backend. Migration is potentially an expensive
// operation and shouldn't start during the hot phase of Chrome start.
class BuiltInBackendToAndroidBackendMigrator {
 public:
  explicit BuiltInBackendToAndroidBackendMigrator(PrefService* prefs);
  BuiltInBackendToAndroidBackendMigrator(
      const BuiltInBackendToAndroidBackendMigrator&) = delete;
  BuiltInBackendToAndroidBackendMigrator& operator=(
      const BuiltInBackendToAndroidBackendMigrator&) = delete;
  BuiltInBackendToAndroidBackendMigrator(
      BuiltInBackendToAndroidBackendMigrator&&) = delete;
  BuiltInBackendToAndroidBackendMigrator& operator=(
      BuiltInBackendToAndroidBackendMigrator&&) = delete;
  ~BuiltInBackendToAndroidBackendMigrator();

  void StartMigrationIfNecessary();

 private:
  // Saves current migration version in 'pref_'.
  void UpdateMigrationVersionInPref();

  PrefService* prefs_ = nullptr;
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_BUILT_IN_BACKEND_TO_ANDROID_BACKEND_MIGRATOR_H_
