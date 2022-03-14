// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_STORE_BACKEND_MIGRATION_DECORATOR_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_STORE_BACKEND_MIGRATION_DECORATOR_H_

#include <memory>

#include "base/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/password_manager/core/browser/password_store_backend.h"

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
      PrefService* prefs,
      SyncDelegate* sync_delegate);
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
  // Implements PasswordStoreBackend interface.
  void InitBackend(RemoteChangesReceived remote_form_changes_received,
                   base::RepeatingClosure sync_enabled_or_disabled_cb,
                   base::OnceCallback<void(bool)> completion) override;
  void Shutdown(base::OnceClosure shutdown_completed) override;
  void GetAllLoginsAsync(LoginsOrErrorReply callback) override;
  void GetAutofillableLoginsAsync(LoginsOrErrorReply callback) override;
  void FillMatchingLoginsAsync(
      LoginsReply callback,
      bool include_psl,
      const std::vector<PasswordFormDigest>& forms) override;
  void AddLoginAsync(const PasswordForm& form,
                     PasswordStoreChangeListReply callback) override;
  void UpdateLoginAsync(const PasswordForm& form,
                        PasswordStoreChangeListReply callback) override;
  void RemoveLoginAsync(const PasswordForm& form,
                        PasswordStoreChangeListReply callback) override;
  void RemoveLoginsByURLAndTimeAsync(
      const base::RepeatingCallback<bool(const GURL&)>& url_filter,
      base::Time delete_begin,
      base::Time delete_end,
      base::OnceCallback<void(bool)> sync_completion,
      PasswordStoreChangeListReply callback) override;
  void RemoveLoginsCreatedBetweenAsync(
      base::Time delete_begin,
      base::Time delete_end,
      PasswordStoreChangeListReply callback) override;
  void DisableAutoSignInForOriginsAsync(
      const base::RepeatingCallback<bool(const GURL&)>& origin_filter,
      base::OnceClosure completion) override;
  SmartBubbleStatsStore* GetSmartBubbleStatsStore() override;
  FieldInfoStore* GetFieldInfoStore() override;
  std::unique_ptr<syncer::ProxyModelTypeControllerDelegate>
  CreateSyncControllerDelegate() override;
  void ClearAllLocalPasswords() override;
  void OnSyncServiceInitialized(syncer::SyncService* sync_service) override;

  // Creates 'migrator_' and starts migration process.
  void StartMigration();

  // React on sync changes to keep GMS Core local storage up-to-date.
  // TODO(https://crbug.com/) Remove this method when no longer needed.
  void SyncStatusChanged();

  std::unique_ptr<PasswordStoreBackend> built_in_backend_;
  std::unique_ptr<PasswordStoreBackend> android_backend_;

  // Proxy backend to which all responsibilities are being delegated.
  std::unique_ptr<PasswordStoreBackend> active_backend_;

  const raw_ptr<PrefService> prefs_ = nullptr;

  // |sync_delegate| lives inside |android_backend|. So it should always be
  // destroyed before |android_backend_|.
  const raw_ptr<SyncDelegate> sync_delegate_;

  std::unique_ptr<BuiltInBackendToAndroidBackendMigrator> migrator_;

  base::WeakPtrFactory<PasswordStoreBackendMigrationDecorator>
      weak_ptr_factory_{this};
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_STORE_BACKEND_MIGRATION_DECORATOR_H_
