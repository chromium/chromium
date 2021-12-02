// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_BUILT_IN_BACKEND_TO_ANDROID_BACKEND_MIGRATOR_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_BUILT_IN_BACKEND_TO_ANDROID_BACKEND_MIGRATOR_H_

#include "base/memory/raw_ptr.h"
#include "components/password_manager/core/browser/password_store_backend.h"

class PrefService;

namespace password_manager {

class PasswordStoreBackend;

// Instantiate this object to migrate all password stored in the built-in
// backend to the Android backend. Migration is potentially an expensive
// operation and shouldn't start during the hot phase of Chrome start.
class BuiltInBackendToAndroidBackendMigrator {
 public:
  // |built_in_backend| and |android_backend| must not be null and must outlive
  // the migrator.
  BuiltInBackendToAndroidBackendMigrator(
      PasswordStoreBackend* built_in_backend,
      PasswordStoreBackend* android_backend,
      PrefService* prefs,
      base::RepeatingCallback<bool()> is_syncing_passwords_callback);

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
  struct BackendAndLoginsResults;

  // Saves current migration version in |prefs_|.
  void UpdateMigrationVersionInPref();

  // Schedules async calls to read of all passwords from both backends.
  void PrepareForMigration();

  // Migrates password between |built_in_backend_| and |android_backend_|.
  // |result| consists of passwords from the |built_in_backend_| let's call them
  // |A| and passwords from the |android_backend_| - |B|. If initial migration
  // needed this function will update both backends with |A|U|B| otherwise it
  // will replace passwords from the |built_in_backend_| with |B|.
  void MigratePasswordsBetweenAndroidAndBuiltInBackends(
      std::vector<BackendAndLoginsResults> result);

  const raw_ptr<PasswordStoreBackend> built_in_backend_;
  const raw_ptr<PasswordStoreBackend> android_backend_;

  const raw_ptr<PrefService> prefs_ = nullptr;

  base::RepeatingCallback<bool()> is_syncing_passwords_callback_;

  base::WeakPtrFactory<BuiltInBackendToAndroidBackendMigrator>
      weak_ptr_factory_{this};
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_BUILT_IN_BACKEND_TO_ANDROID_BACKEND_MIGRATOR_H_
