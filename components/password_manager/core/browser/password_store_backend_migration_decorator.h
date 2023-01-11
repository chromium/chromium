// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_STORE_BACKEND_MIGRATION_DECORATOR_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_STORE_BACKEND_MIGRATION_DECORATOR_H_

#include <memory>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/password_manager/core/browser/password_store_backend.h"
#include "components/sync/driver/sync_service_observer.h"

class PrefService;

namespace password_manager {

class BuiltInBackendToAndroidBackendMigrator;

// This is the backend that should be used on Android platform until the full
// migration to the Android backend is launched. Internally, this backend
// owns two backends: the built-in and the Android backend. In addition
// to delegating all backend responsibilities, it is responsible for migrating
// credentials between both backends as well as instantiating any proxy backends
// that are used for shadowing the traffic.
class PasswordStoreBackendMigrationDecorator : public PasswordStoreBackend {
 public:
  PasswordStoreBackendMigrationDecorator(
      std::unique_ptr<PasswordStoreBackend> built_in_backend,
      std::unique_ptr<PasswordStoreBackend> android_backend,
      PrefService* prefs);
  PasswordStoreBackendMigrationDecorator(
      const PasswordStoreBackendMigrationDecorator&) = delete;
  PasswordStoreBackendMigrationDecorator(
      PasswordStoreBackendMigrationDecorator&&) = delete;
  PasswordStoreBackendMigrationDecorator& operator=(
      const PasswordStoreBackendMigrationDecorator&) = delete;
  PasswordStoreBackendMigrationDecorator& operator=(
      PasswordStoreBackendMigrationDecorator&&) = delete;
  ~PasswordStoreBackendMigrationDecorator() override;

 private:
  class PasswordSyncSettingsHelper : public syncer::SyncServiceObserver {
   public:
    explicit PasswordSyncSettingsHelper(PrefService* prefs);

    // Remembers the initial sync setting to track its changes later.
    // Should be called after SyncService is initialized.
    void CachePasswordSyncSettingOnStartup(syncer::SyncService* sync);

    // Called when sync settings were applied to confirm change of state.
    void SyncStatusChangeApplied();

    // Clears cached prefs when they are not needed anymore.
    void ResetCachedPrefs();

    void set_migrator(BuiltInBackendToAndroidBackendMigrator* migrator) {
      migrator_ = migrator;
    }

   private:
    // syncer::SyncServiceObserver implementation.
    void OnStateChanged(syncer::SyncService* sync) override;
    void OnSyncCycleCompleted(syncer::SyncService* sync) override;

    // Pref service.
    const raw_ptr<PrefService> prefs_ = nullptr;

    // Set when sync_service is already initialized and can be interacted with.
    raw_ptr<const syncer::SyncService> sync_service_ = nullptr;

    // Migrator object to use in case observed sync service events should
    // trigger migration.
    raw_ptr<BuiltInBackendToAndroidBackendMigrator> migrator_ = nullptr;

    // Cached value of the configured password sync setting. Updated when the
    // user is changing sync settings, and may from
    // |password_sync_applied_setting_| at that moment.
    bool password_sync_configured_setting_ = false;

    // Cached value of the password sync runtime state. May differ from
    // |password_sync_configured_setting_| at the moment when the user is
    // changing sync settings. Updated when new settings take action.
    bool password_sync_applied_setting_ = false;

    // If the first sync cycle after the startup has completed.
    bool is_waiting_for_the_first_sync_cycle_ = true;
  };

  // Implements PasswordStoreBackend interface.
  void InitBackend(RemoteChangesReceived remote_form_changes_received,
                   base::RepeatingClosure sync_enabled_or_disabled_cb,
                   base::OnceCallback<void(bool)> completion) override;
  void Shutdown(base::OnceClosure shutdown_completed) override;
  void GetAllLoginsAsync(LoginsOrErrorReply callback) override;
  void GetAutofillableLoginsAsync(LoginsOrErrorReply callback) override;
  void GetAllLoginsForAccountAsync(absl::optional<std::string> account,
                                   LoginsOrErrorReply callback) override;
  void FillMatchingLoginsAsync(
      LoginsOrErrorReply callback,
      bool include_psl,
      const std::vector<PasswordFormDigest>& forms) override;
  void AddLoginAsync(const PasswordForm& form,
                     PasswordChangesOrErrorReply callback) override;
  void UpdateLoginAsync(const PasswordForm& form,
                        PasswordChangesOrErrorReply callback) override;
  void RemoveLoginAsync(const PasswordForm& form,
                        PasswordChangesOrErrorReply callback) override;
  void RemoveLoginsByURLAndTimeAsync(
      const base::RepeatingCallback<bool(const GURL&)>& url_filter,
      base::Time delete_begin,
      base::Time delete_end,
      base::OnceCallback<void(bool)> sync_completion,
      PasswordChangesOrErrorReply callback) override;
  void RemoveLoginsCreatedBetweenAsync(
      base::Time delete_begin,
      base::Time delete_end,
      PasswordChangesOrErrorReply callback) override;
  void DisableAutoSignInForOriginsAsync(
      const base::RepeatingCallback<bool(const GURL&)>& origin_filter,
      base::OnceClosure completion) override;
  SmartBubbleStatsStore* GetSmartBubbleStatsStore() override;
  FieldInfoStore* GetFieldInfoStore() override;
  std::unique_ptr<syncer::ProxyModelTypeControllerDelegate>
  CreateSyncControllerDelegate() override;
  void ClearAllLocalPasswords() override;
  void OnSyncServiceInitialized(syncer::SyncService* sync_service) override;

  // Starts migration process.
  void StartMigrationAfterInit();

  // React on sync changes to keep GMS Core local storage up-to-date.
  // Called when the changed setting is applied.
  // TODO(https://crbug.com/) Remove this method when no longer needed.
  void SyncStatusChanged();

  std::unique_ptr<PasswordStoreBackend> built_in_backend_;
  std::unique_ptr<PasswordStoreBackend> android_backend_;

  // Proxy backend to which all responsibilities are being delegated.
  std::unique_ptr<PasswordStoreBackend> active_backend_;

  const raw_ptr<PrefService> prefs_ = nullptr;

  raw_ptr<const syncer::SyncService> sync_service_ = nullptr;

  std::unique_ptr<BuiltInBackendToAndroidBackendMigrator> migrator_;

  // Listener for sync settings changes.
  PasswordSyncSettingsHelper sync_settings_helper_;

  base::WeakPtrFactory<PasswordStoreBackendMigrationDecorator>
      weak_ptr_factory_{this};
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_STORE_BACKEND_MIGRATION_DECORATOR_H_
