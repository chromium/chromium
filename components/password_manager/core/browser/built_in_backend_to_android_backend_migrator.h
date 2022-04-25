// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_BUILT_IN_BACKEND_TO_ANDROID_BACKEND_MIGRATOR_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_BUILT_IN_BACKEND_TO_ANDROID_BACKEND_MIGRATOR_H_

#include "base/memory/raw_ptr.h"
#include "components/password_manager/core/browser/password_store_backend.h"

class PrefService;

namespace password_manager {

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
      PasswordStoreBackend::SyncDelegate* sync_delegate);

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
  struct IsPasswordLess;
  struct BackendAndLoginsResults;
  class MigrationMetricsReporter;

  using PasswordFormPtrFlatSet =
      base::flat_set<const PasswordForm*, IsPasswordLess>;

  // Saves current migration version in |prefs_|.
  void UpdateMigrationVersionInPref();

  // Schedules async calls to read of all passwords from both backends.
  void PrepareForMigration();

  // Migrates all non-syncable data contained in |logins_or_error| to the
  // |target_backend|. This is implemented by issuing update requests for
  // all retrieved credentials.
  void MigrateNonSyncableData(PasswordStoreBackend* target_backend,
                              LoginsResultOrError logins_or_error);

  // Migrates password between |built_in_backend_| and |android_backend_|.
  // |result| consists of passwords from the |built_in_backend_| let's call them
  // |A| and passwords from the |android_backend_| - |B|. If initial migration
  // needed this function will update both backends with |A|U|B| otherwise it
  // will replace passwords from the |built_in_backend_| with |B|.
  void MigratePasswordsBetweenAndroidAndBuiltInBackends(
      std::vector<BackendAndLoginsResults> result);

  // Updates both |built_in_backend_| and |android_backend_| such that both
  // contain the same set of passwords without deleting any password. In
  // addition, it marks the initial migration as completed.
  void MergeAndroidBackendAndBuiltInBackend(
      PasswordFormPtrFlatSet built_in_backend_logins,
      PasswordFormPtrFlatSet android_logins);

  // Updates |built_in_backend_| such that it contains the same set of passwords
  // as in |android_backend_|.
  void MirrorAndroidBackendToBuiltInBackend(
      PasswordFormPtrFlatSet built_in_backend_logins,
      PasswordFormPtrFlatSet android_logins);

  // Helper methods to {Add,Update,Remove} |form| in |backend|. This is used to
  // ensure that all the operations are happening inside
  // BuiltInBackendToAndroidBackendMigrator life-scope.
  void AddLoginToBackend(PasswordStoreBackend* backend,
                         const PasswordForm& form,
                         base::OnceClosure callback);
  void UpdateLoginInBackend(PasswordStoreBackend* backend,
                            const PasswordForm& form,
                            base::OnceClosure callback);
  void RemoveLoginFromBackend(PasswordStoreBackend* backend,
                              const PasswordForm& form,
                              base::OnceClosure callback);

  // If |changelist| is an empty changelist, migration is aborted by calling
  // MigrationFinished() indicating the migration is *not* successful.
  // Otherwise, |callback| is invoked.
  void RunCallbackOrAbortMigration(
      base::OnceClosure callback,
      absl::optional<PasswordStoreChangeList> changelist);

  // Reports metrics and deletes |metrics_reporter_|
  void MigrationFinished(bool is_success);

  // Returns true if prefs and enabled features allow non-syncable data
  // migration.
  bool ShouldMigrateNonSyncableData();

  const raw_ptr<PasswordStoreBackend> built_in_backend_;
  const raw_ptr<PasswordStoreBackend> android_backend_;

  const raw_ptr<PrefService> prefs_ = nullptr;

  std::unique_ptr<MigrationMetricsReporter> metrics_reporter_;

  const raw_ptr<PasswordStoreBackend::SyncDelegate> sync_delegate_;

  bool non_syncable_data_migration_in_progress_ = false;

  base::WeakPtrFactory<BuiltInBackendToAndroidBackendMigrator>
      weak_ptr_factory_{this};
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_BUILT_IN_BACKEND_TO_ANDROID_BACKEND_MIGRATOR_H_
