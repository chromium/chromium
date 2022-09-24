// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_BUILT_IN_BACKEND_TO_ANDROID_BACKEND_MIGRATOR_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_BUILT_IN_BACKEND_TO_ANDROID_BACKEND_MIGRATOR_H_

#include "base/memory/raw_ptr.h"
#include "components/password_manager/core/browser/password_store_backend.h"

class PrefService;

namespace password_manager {

// Instantiate this object to migrate all stored passwords between the built-in
// and the Android backends. Migration is potentially an expensive operation
// and shouldn't start during the hot phase of Chrome start.
class BuiltInBackendToAndroidBackendMigrator {
 public:
  // The type of the migration that should be executed next.
  enum class MigrationType {
    // Migration is not needed.
    kNone,
    // When the sync user is first enrolled into the UPM experiment, initial
    // migration to the android backend should happen once. Because the user is
    // syncing, the android backend already has all credentials, so only the
    // non-syncable data should be moved.
    kInitialForSyncUsers,
    // Migration to the android backend after enabling password sync. Can happen
    // multiple times. While password sync was disabled, logins were saved to
    // the built-in backend, after enabling sync logins can be synced, but
    // non-syncable data still needs to be migrated.
    kNonSyncableToAndroidBackend,
    // Migration to the built-in backend after disabling password sync. Can
    // happen multiple times. While password sync was on, logins were saved to
    // the android backend and the built in backend was updated via sync and
    // logins should be the same, but non-syncable data still needs to be
    // migrated.
    kNonSyncableToBuiltInBackend,
    // When Unified Password Manager is enabled for non-syncing
    // users, the migration to keep both backend in sync is needed. That
    // includes both the initial migration (to the android backend) and the
    // rolling migration (to the built-in backend).
    kForLocalUsers,
    // When the sync user is unenrolled into the UPM experiment, but the sync
    // functions without errors on the device, automatic UPM reenrollment
    // attempts will be made. Because the user is syncing, the android backend
    // already has all credentials, so only the non-syncable data should be
    // moved.
    kReenrollmentAttempt,
  };

  // |built_in_backend| and |android_backend| must not be null and must outlive
  // the migrator.
  BuiltInBackendToAndroidBackendMigrator(PasswordStoreBackend* built_in_backend,
                                         PasswordStoreBackend* android_backend,
                                         PrefService* prefs);

  BuiltInBackendToAndroidBackendMigrator(
      const BuiltInBackendToAndroidBackendMigrator&) = delete;
  BuiltInBackendToAndroidBackendMigrator& operator=(
      const BuiltInBackendToAndroidBackendMigrator&) = delete;
  BuiltInBackendToAndroidBackendMigrator(
      BuiltInBackendToAndroidBackendMigrator&&) = delete;
  BuiltInBackendToAndroidBackendMigrator& operator=(
      BuiltInBackendToAndroidBackendMigrator&&) = delete;
  ~BuiltInBackendToAndroidBackendMigrator();

  void StartMigrationIfNecessary(bool should_attempt_upm_reenrollment);

  void OnSyncServiceInitialized(syncer::SyncService* sync_service);

  base::WeakPtr<BuiltInBackendToAndroidBackendMigrator> GetWeakPtr();

 private:
  struct IsPasswordLess;
  struct BackendAndLoginsResults;
  class MigrationMetricsReporter;

  using PasswordFormPtrFlatSet =
      base::flat_set<const PasswordForm*, IsPasswordLess>;

  // Saves current migration version in |prefs_|.
  void UpdateMigrationVersionInPref();

  // Returns the type of migration that should happen next.
  MigrationType GetMigrationType(bool should_attempt_upm_reenrollment) const;

  // Schedules async call(s) to read passwords with a callback to migrate
  // passwords once they are retrieved.
  void PrepareForMigration(MigrationType migration_type);

  // Migrates all non-syncable data contained in |logins_or_error| to the
  // |target_backend|. This is implemented by issuing update requests for
  // all retrieved credentials.
  void MigrateNonSyncableData(PasswordStoreBackend* target_backend,
                              LoginsResultOrError logins_or_error);

  // Performs the migration that synchronises entries between
  // |built_in_backend_| and |android_backend_| to keep them in consistent
  // state. Calls |MigratePasswordsBetweenAndroidAndBuiltInBackends| internally
  // to perform initial & rolling migration for local users.
  void RunMigrationForLocalUsers();

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
  void RunCallbackOrAbortMigration(base::OnceClosure callback,
                                   PasswordChangesOrError changelist);

  // Reports metrics and deletes |metrics_reporter_|
  void MigrationFinished(bool is_success);

  // Removes blocklisted forms with non-empty |username_value| or
  // |password_value| from |backend|.
  // |result_callback| is called with the |LoginsResult| containing valid forms
  // only or |PasswordStoreBackendError| if it contained in |logins_or_error|.
  // |logins_or_error| is modified in place.
  void RemoveBlacklistedFormsWithValues(PasswordStoreBackend* backend,
                                        LoginsOrErrorReply result_callback,
                                        LoginsResultOrError logins_or_error);

  const raw_ptr<PasswordStoreBackend> built_in_backend_;
  const raw_ptr<PasswordStoreBackend> android_backend_;

  const raw_ptr<PrefService> prefs_ = nullptr;

  std::unique_ptr<MigrationMetricsReporter> metrics_reporter_;

  raw_ptr<const syncer::SyncService> sync_service_ = nullptr;

  MigrationType migration_in_progress_type_ = MigrationType::kNone;

  base::WeakPtrFactory<BuiltInBackendToAndroidBackendMigrator>
      weak_ptr_factory_{this};
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_BUILT_IN_BACKEND_TO_ANDROID_BACKEND_MIGRATOR_H_
