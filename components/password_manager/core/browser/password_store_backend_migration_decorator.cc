// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_store_backend_migration_decorator.h"

#include "base/bind.h"
#include "base/task/sequenced_task_runner.h"
#include "components/password_manager/core/browser/built_in_backend_to_android_backend_migrator.h"
#include "components/password_manager/core/browser/field_info_table.h"
#include "components/password_manager/core/browser/password_store_proxy_backend.h"
#include "components/password_manager/core/browser/password_sync_util.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/sync/driver/sync_service.h"
#include "components/sync/model/proxy_model_type_controller_delegate.h"

namespace password_manager {

namespace {

// Time in seconds by which the passwords migration from the built-in backend to
// the Android backend is delayed.
constexpr int kMigrationToAndroidBackendDelay = 30;

}  // namespace

PasswordStoreBackendMigrationDecorator::PasswordStoreBackendMigrationDecorator(
    std::unique_ptr<PasswordStoreBackend> built_in_backend,
    std::unique_ptr<PasswordStoreBackend> android_backend,
    PrefService* prefs,
    SyncDelegate* sync_delegate)
    : built_in_backend_(std::move(built_in_backend)),
      android_backend_(std::move(android_backend)),
      prefs_(prefs),
      sync_delegate_(sync_delegate),
      sync_settings_helper_(prefs) {
  DCHECK(built_in_backend_);
  DCHECK(android_backend_);
  active_backend_ = std::make_unique<PasswordStoreProxyBackend>(
      built_in_backend_.get(), android_backend_.get(), prefs_,
      sync_delegate_.get());
}

PasswordStoreBackendMigrationDecorator::
    ~PasswordStoreBackendMigrationDecorator() = default;

PasswordStoreBackendMigrationDecorator::PasswordSyncSettingsHelper::
    PasswordSyncSettingsHelper(PrefService* prefs)
    : prefs_(prefs) {}

void PasswordStoreBackendMigrationDecorator::PasswordSyncSettingsHelper::
    CachePasswordSyncSettingOnStartup(syncer::SyncService* sync) {
  sync_service_ = sync;
  password_sync_configured_setting_ = sync_util::IsPasswordSyncEnabled(sync);
  password_sync_applied_setting_ = password_sync_configured_setting_;
}

void PasswordStoreBackendMigrationDecorator::PasswordSyncSettingsHelper::
    SyncStatusChangeApplied() {
  DCHECK(sync_service_);
  password_sync_applied_setting_ =
      sync_util::IsPasswordSyncEnabled(sync_service_);
  // Previously cached prefs are not needed anymore.
  last_migration_version_setting_.reset();
  last_migration_time_setting_.reset();
}

void PasswordStoreBackendMigrationDecorator::PasswordSyncSettingsHelper::
    OnStateChanged(syncer::SyncService* sync) {
  DCHECK(sync_service_ == sync);

  // Return early if the setting didn't change.
  if (sync_util::IsPasswordSyncEnabled(sync) ==
      password_sync_configured_setting_) {
    return;
  }
  password_sync_configured_setting_ = sync_util::IsPasswordSyncEnabled(sync);

  if (password_sync_configured_setting_ != password_sync_applied_setting_) {
    UpdatePrefsToTriggerMigration();
  } else {
    RestoreMigrationPrefsFromCacheIfNeeded();
  }
}

void PasswordStoreBackendMigrationDecorator::PasswordSyncSettingsHelper::
    UpdatePrefsToTriggerMigration() {
  // Cache old values.
  last_migration_version_setting_ =
      prefs_->GetInteger(prefs::kCurrentMigrationVersionToGoogleMobileServices);
  last_migration_time_setting_ =
      prefs_->GetDouble(prefs::kTimeOfLastMigrationAttempt);

  // Set updated value.
  prefs_->SetInteger(prefs::kCurrentMigrationVersionToGoogleMobileServices, 0);
  prefs_->SetDouble(prefs::kTimeOfLastMigrationAttempt, 0.0);
  prefs_->SetBoolean(prefs::kRequiresMigrationAfterSyncStatusChange, true);
}

void PasswordStoreBackendMigrationDecorator::PasswordSyncSettingsHelper::
    RestoreMigrationPrefsFromCacheIfNeeded() {
  // It's not possible to restore prefs if nothing is cached.
  if (!last_migration_version_setting_.has_value() ||
      !last_migration_time_setting_.has_value()) {
    return;
  }

  prefs_->SetInteger(prefs::kCurrentMigrationVersionToGoogleMobileServices,
                     last_migration_version_setting_.value());
  prefs_->SetDouble(prefs::kTimeOfLastMigrationAttempt,
                    last_migration_time_setting_.value());
  prefs_->SetBoolean(prefs::kRequiresMigrationAfterSyncStatusChange, false);
}

void PasswordStoreBackendMigrationDecorator::InitBackend(
    RemoteChangesReceived remote_form_changes_received,
    base::RepeatingClosure sync_enabled_or_disabled_cb,
    base::OnceCallback<void(bool)> completion) {
  base::RepeatingClosure handle_sync_status_change = base::BindRepeating(
      &PasswordStoreBackendMigrationDecorator::SyncStatusChanged,
      weak_ptr_factory_.GetWeakPtr());

  // |sync_enabled_or_disabled_cb| is called on a background sequence so it
  // should be posted to the main sequence before invoking
  // PasswordStoreBackendMigrationDecorator::SyncStatusChanged().
  base::RepeatingClosure handle_sync_status_change_on_main_thread =
      base::BindRepeating(
          base::IgnoreResult(&base::SequencedTaskRunner::PostTask),
          base::SequencedTaskRunnerHandle::Get(), FROM_HERE,
          std::move(handle_sync_status_change));

  // Inject nested callback to listen for sync status changes.
  sync_enabled_or_disabled_cb =
      std::move(handle_sync_status_change_on_main_thread)
          .Then(std::move(sync_enabled_or_disabled_cb));

  active_backend_->InitBackend(std::move(remote_form_changes_received),
                               std::move(sync_enabled_or_disabled_cb),
                               std::move(completion));

  // Only start the migration when launching the UPM which needs chrome-local
  // data in the remote store. For shadow traffic, this doesn't matter.
  if (features::RequiresMigrationForUnifiedPasswordManager()) {
    migrator_ = std::make_unique<BuiltInBackendToAndroidBackendMigrator>(
        built_in_backend_.get(), android_backend_.get(), prefs_,
        sync_delegate_.get());
    base::SequencedTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(
            &PasswordStoreBackendMigrationDecorator::StartMigrationAfterInit,
            weak_ptr_factory_.GetWeakPtr()),
        base::Seconds(kMigrationToAndroidBackendDelay));
  }
}

void PasswordStoreBackendMigrationDecorator::Shutdown(
    base::OnceClosure shutdown_completed) {
  // Calling Shutdown() on active_backend_ will take care of calling
  // Shutdown() on relevant backends.
  active_backend_->Shutdown(
      base::BindOnce(
          [](std::unique_ptr<PasswordStoreBackend> built_in_backend,
             std::unique_ptr<PasswordStoreBackend> android_backend,
             std::unique_ptr<PasswordStoreBackend> combined_backend) {
            // All the backends must be destroyed only after |active_backend_|
            // signals that Shutdown is over. It can be done asynchronously and
            // after PasswordStoreBackendMigrationDecorator destruction.
            built_in_backend.reset();
            android_backend.reset();
            combined_backend.reset();
          },
          std::move(built_in_backend_), std::move(android_backend_),
          std::move(active_backend_))
          .Then(std::move(shutdown_completed)));
}

void PasswordStoreBackendMigrationDecorator::GetAllLoginsAsync(
    LoginsOrErrorReply callback) {
  active_backend_->GetAllLoginsAsync(std::move(callback));
}

void PasswordStoreBackendMigrationDecorator::GetAutofillableLoginsAsync(
    LoginsOrErrorReply callback) {
  active_backend_->GetAutofillableLoginsAsync(std::move(callback));
}

void PasswordStoreBackendMigrationDecorator::GetAllLoginsForAccountAsync(
    absl::optional<std::string> account,
    LoginsOrErrorReply callback) {
  NOTREACHED();
}

void PasswordStoreBackendMigrationDecorator::FillMatchingLoginsAsync(
    LoginsReply callback,
    bool include_psl,
    const std::vector<PasswordFormDigest>& forms) {
  active_backend_->FillMatchingLoginsAsync(std::move(callback), include_psl,
                                           forms);
}

void PasswordStoreBackendMigrationDecorator::AddLoginAsync(
    const PasswordForm& form,
    PasswordStoreChangeListReply callback) {
  active_backend_->AddLoginAsync(form, std::move(callback));
}

void PasswordStoreBackendMigrationDecorator::UpdateLoginAsync(
    const PasswordForm& form,
    PasswordStoreChangeListReply callback) {
  active_backend_->UpdateLoginAsync(form, std::move(callback));
}

void PasswordStoreBackendMigrationDecorator::RemoveLoginAsync(
    const PasswordForm& form,
    PasswordStoreChangeListReply callback) {
  active_backend_->RemoveLoginAsync(form, std::move(callback));
}

void PasswordStoreBackendMigrationDecorator::RemoveLoginsByURLAndTimeAsync(
    const base::RepeatingCallback<bool(const GURL&)>& url_filter,
    base::Time delete_begin,
    base::Time delete_end,
    base::OnceCallback<void(bool)> sync_completion,
    PasswordStoreChangeListReply callback) {
  active_backend_->RemoveLoginsByURLAndTimeAsync(
      url_filter, std::move(delete_begin), std::move(delete_end),
      std::move(sync_completion), std::move(callback));
}

void PasswordStoreBackendMigrationDecorator::RemoveLoginsCreatedBetweenAsync(
    base::Time delete_begin,
    base::Time delete_end,
    PasswordStoreChangeListReply callback) {
  active_backend_->RemoveLoginsCreatedBetweenAsync(
      std::move(delete_begin), std::move(delete_end), std::move(callback));
}

void PasswordStoreBackendMigrationDecorator::DisableAutoSignInForOriginsAsync(
    const base::RepeatingCallback<bool(const GURL&)>& origin_filter,
    base::OnceClosure completion) {
  active_backend_->DisableAutoSignInForOriginsAsync(origin_filter,
                                                    std::move(completion));
}

SmartBubbleStatsStore*
PasswordStoreBackendMigrationDecorator::GetSmartBubbleStatsStore() {
  return active_backend_->GetSmartBubbleStatsStore();
}

FieldInfoStore* PasswordStoreBackendMigrationDecorator::GetFieldInfoStore() {
  return active_backend_->GetFieldInfoStore();
}

std::unique_ptr<syncer::ProxyModelTypeControllerDelegate>
PasswordStoreBackendMigrationDecorator::CreateSyncControllerDelegate() {
  if (base::FeatureList::IsEnabled(
          features::kUnifiedPasswordManagerSyncUsingAndroidBackendOnly)) {
    // The android backend (PasswordStoreAndroidBackend) creates a controller
    // delegate that prevents sync from actually communicating with the sync
    // server using the built in SyncEngine.
    return android_backend_->CreateSyncControllerDelegate();
  }

  return built_in_backend_->CreateSyncControllerDelegate();
}

void PasswordStoreBackendMigrationDecorator::ClearAllLocalPasswords() {
  NOTIMPLEMENTED();
}

void PasswordStoreBackendMigrationDecorator::OnSyncServiceInitialized(
    syncer::SyncService* sync_service) {
  sync_settings_helper_.CachePasswordSyncSettingOnStartup(sync_service);
  sync_service->AddObserver(&sync_settings_helper_);
  active_backend_->OnSyncServiceInitialized(sync_service);
}

void PasswordStoreBackendMigrationDecorator::StartMigrationAfterInit() {
  DCHECK(migrator_);
  if (prefs_->GetBoolean(prefs::kRequiresMigrationAfterSyncStatusChange) &&
      !sync_delegate_->IsSyncingPasswordsEnabled()) {
    // Sync was disabled at the end of the last session, but migration from
    // the android backend to the built-in backend didn't happen. It's not
    // safe to attempt to call the android backend to migrate logins. Disable
    // autosignin for all logins to avoid using outdated settings.
    built_in_backend_->DisableAutoSignInForOriginsAsync(
        base::BindRepeating([](const GURL& url) { return true; }),
        base::DoNothing());
    prefs_->SetBoolean(prefs::kRequiresMigrationAfterSyncStatusChange, false);
    return;
  }

  migrator_->StartMigrationIfNecessary();
}

void PasswordStoreBackendMigrationDecorator::SyncStatusChanged() {
  if (!features::RequiresMigrationForUnifiedPasswordManager())
    return;

  sync_settings_helper_.SyncStatusChangeApplied();
  // Non-syncable data needs to be migrated to the new active backend.
  migrator_->StartMigrationIfNecessary();

  // TODO(crbug.com/1312387): Delete all the passwords from GMS Core
  // local storage if password sync was enabled.
}

}  // namespace password_manager
