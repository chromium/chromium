// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_store/login_database_async_helper.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/task/sequenced_task_runner.h"
#include "build/buildflag.h"
#include "components/os_crypt/async/common/encryptor.h"
#include "components/os_crypt/sync/os_crypt.h"
#include "components/password_manager/core/browser/password_manager_buildflags.h"
#include "components/password_manager/core/browser/password_store/login_database.h"
#include "components/password_manager/core/browser/sync/password_proto_utils.h"
#include "components/password_manager/core/browser/sync/password_sync_bridge.h"
#include "components/sync/base/data_type.h"
#include "components/sync/model/client_tag_based_data_type_processor.h"
#include "components/sync/model/data_type_controller_delegate.h"

#if !BUILDFLAG(USE_LOGIN_DATABASE_AS_BACKEND)
#include "components/password_manager/core/browser/features/password_features.h"
#endif

namespace password_manager {

namespace {

constexpr base::TimeDelta kSyncTaskTimeout = base::Seconds(30);

}  // namespace

LoginDatabaseAsyncHelper::LoginDatabaseAsyncHelper(
    std::unique_ptr<LoginDatabase> login_db,
    UnsyncedCredentialsDeletionNotifier notifier,
    scoped_refptr<base::SequencedTaskRunner> main_task_runner,
    syncer::WipeModelUponSyncDisabledBehavior
        wipe_model_upon_sync_disabled_behavior)
    : login_db_(std::move(login_db)),
      wipe_model_upon_sync_disabled_behavior_(
          wipe_model_upon_sync_disabled_behavior),
      deletion_notifier_(std::move(notifier)),
      main_task_runner_(std::move(main_task_runner)) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
  DCHECK(login_db_);
  DCHECK(main_task_runner_);
}

LoginDatabaseAsyncHelper::~LoginDatabaseAsyncHelper() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void LoginDatabaseAsyncHelper::CreateSyncBackend() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

#if BUILDFLAG(USE_LOGIN_DATABASE_AS_BACKEND)
  // Sync bridge must be constructed immediately to accommodate
  // GetSyncControllerDelegate() call.
  password_sync_bridge_ =
          std::make_unique<PasswordSyncBridge>(
              std::make_unique<syncer::ClientTagBasedDataTypeProcessor>(
                  syncer::PASSWORDS, base::DoNothing()),
              wipe_model_upon_sync_disabled_behavior_);
#endif  // BUILDFLAG(USE_LOGIN_DATABASE_AS_BACKEND)
}

bool LoginDatabaseAsyncHelper::Initialize(
    base::RepeatingCallback<void(std::optional<PasswordStoreChangeList>, bool)>
        remote_form_changes_received,
    base::RepeatingClosure sync_enabled_or_disabled_cb,
    base::RepeatingCallback<void(password_manager::IsAccountStore)>
        on_undecryptable_passwords_removed,
    std::unique_ptr<os_crypt_async::Encryptor> encryptor) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  remote_forms_changes_received_callback_ =
      std::move(remote_form_changes_received);

  bool success = true;
  if (!login_db_->Init(std::move(on_undecryptable_passwords_removed),
                       std::move(encryptor))) {
    login_db_.reset();
    // The initialization should be continued, because PasswordSyncBridge
    // has to be initialized even if database initialization failed.
    success = false;
    LOG(ERROR) << "Could not create/open login database.";
  }
  if (success) {
    login_db_->password_sync_metadata_store()
        .SetPasswordDeletionsHaveSyncedCallback(base::BindRepeating(
            &LoginDatabaseAsyncHelper::NotifyDeletionsHaveSynced,
            weak_ptr_factory_.GetWeakPtr()));

    // Delay the actual reporting by 30 seconds, to ensure it doesn't happen
    // during the "hot phase" of Chrome startup.
    base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&LoginDatabaseAsyncHelper::ReportMetrics,
                       weak_ptr_factory_.GetWeakPtr()),
        base::Seconds(30));
  }
  if (password_sync_bridge_) {
    password_sync_bridge_->Init(this, sync_enabled_or_disabled_cb);
  }

// On Windows encryption capability is expected to be available by default.
// On MacOS encrpytion is also expected to be available unless the user didn't
// unlock the Keychain.
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
  // Check that the backend works.
  if (success && !OSCrypt::IsEncryptionAvailable()) {
    success = false;
    LOG(ERROR) << "Encryption is not available.";
  }
#endif

  return success;
}

LoginsResultOrError LoginDatabaseAsyncHelper::GetAllLogins() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::vector<PasswordForm> forms;

  if (!login_db_) {
    return PasswordStoreBackendError(
        PasswordStoreBackendErrorType::kUncategorized);
  }
  FormRetrievalResult result = login_db_->GetAllLogins(&forms);
  if (result != FormRetrievalResult::kSuccess &&
      result != FormRetrievalResult::kEncryptionServiceFailureWithPartialData) {
    return PasswordStoreBackendError(
        PasswordStoreBackendErrorType::kUncategorized);
  }
  return forms;
}

LoginsResultOrError LoginDatabaseAsyncHelper::GetAutofillableLogins() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::vector<PasswordForm> results;
  if (!login_db_ || !login_db_->GetAutofillableLogins(&results)) {
    return PasswordStoreBackendError(
        PasswordStoreBackendErrorType::kUncategorized);
  }
  return results;
}

LoginsResultOrError LoginDatabaseAsyncHelper::FillMatchingLogins(
    const std::vector<PasswordFormDigest>& forms,
    bool include_psl) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::vector<PasswordForm> results;
  for (const auto& form : forms) {
    std::vector<PasswordForm> matched_forms;
    if (!login_db_ ||
        !login_db_->GetLogins(form, include_psl, &matched_forms)) {
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
      return PasswordStoreBackendError(
          OSCrypt::IsEncryptionAvailable()
              ? PasswordStoreBackendErrorType::kUncategorized
              : PasswordStoreBackendErrorType::kKeychainError);
#else
      return PasswordStoreBackendError(
          PasswordStoreBackendErrorType::kUncategorized);
#endif
    }
    results.insert(results.end(),
                   std::make_move_iterator(matched_forms.begin()),
                   std::make_move_iterator(matched_forms.end()));
  }
  return results;
}

PasswordChangesOrError LoginDatabaseAsyncHelper::AddLogin(
    const PasswordForm& form) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  BeginTransaction();
  AddCredentialError error = AddCredentialError::kNone;
  PasswordStoreChangeList changes = AddLoginImpl(form, &error);
  if (password_sync_bridge_ && !changes.empty()) {
    password_sync_bridge_->ActOnPasswordStoreChanges(FROM_HERE, changes);
  }
  // Sync metadata get updated in ActOnPasswordStoreChanges(). Therefore,
  // CommitTransaction() must be called after ActOnPasswordStoreChanges(),
  // because sync codebase needs to update metadata atomically together with
  // the login data.
  CommitTransaction();
  return error == AddCredentialError::kNone
             ? changes
             : PasswordChangesOrError(PasswordStoreBackendError(
                   PasswordStoreBackendErrorType::kUncategorized));
}

PasswordChangesOrError LoginDatabaseAsyncHelper::UpdateLogin(
    const PasswordForm& form) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  BeginTransaction();
  UpdateCredentialError error = UpdateCredentialError::kNone;
  PasswordStoreChangeList changes = UpdateLoginImpl(form, &error);
  if (password_sync_bridge_ && !changes.empty()) {
    password_sync_bridge_->ActOnPasswordStoreChanges(FROM_HERE, changes);
  }
  // Sync metadata get updated in ActOnPasswordStoreChanges(). Therefore,
  // CommitTransaction() must be called after ActOnPasswordStoreChanges(),
  // because sync codebase needs to update metadata atomically together with
  // the login data.
  CommitTransaction();
  return error == UpdateCredentialError::kNone
             ? changes
             : PasswordChangesOrError(PasswordStoreBackendError(
                   PasswordStoreBackendErrorType::kUncategorized));
}

PasswordChangesOrError LoginDatabaseAsyncHelper::RemoveLogin(
    const base::Location& location,
    const PasswordForm& form) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  BeginTransaction();
  PasswordStoreChangeList changes;
  if (login_db_ && login_db_->RemoveLogin(form, &changes)) {
    if (password_sync_bridge_ && !changes.empty()) {
      password_sync_bridge_->ActOnPasswordStoreChanges(location, changes);
    }
  }
  // Sync metadata get updated in ActOnPasswordStoreChanges(). Therefore,
  // CommitTransaction() must be called after ActOnPasswordStoreChanges(),
  // because sync codebase needs to update metadata atomically together with
  // the login data.
  CommitTransaction();
  return changes;
}

PasswordChangesOrError LoginDatabaseAsyncHelper::RemoveLoginsCreatedBetween(
    const base::Location& location,
    base::Time delete_begin,
    base::Time delete_end) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  BeginTransaction();
  PasswordStoreChangeList changes;
  bool success = login_db_ && login_db_->RemoveLoginsCreatedBetween(
                                  delete_begin, delete_end, &changes);
  if (success && password_sync_bridge_ && !changes.empty()) {
    password_sync_bridge_->ActOnPasswordStoreChanges(location, changes);
  }
  // Sync metadata get updated in ActOnPasswordStoreChanges(). Therefore,
  // CommitTransaction() must be called after ActOnPasswordStoreChanges(),
  // because sync codebase needs to update metadata atomically together with
  // the login data.
  CommitTransaction();
  return success ? changes
                 : PasswordChangesOrError(PasswordStoreBackendError(
                       PasswordStoreBackendErrorType::kUncategorized));
}

PasswordChangesOrError LoginDatabaseAsyncHelper::RemoveLoginsByURLAndTime(
    const base::Location& location,
    const base::RepeatingCallback<bool(const GURL&)>& url_filter,
    base::Time delete_begin,
    base::Time delete_end,
    base::OnceCallback<void(bool)> sync_completion) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  BeginTransaction();
  std::vector<PasswordForm> forms;
  PasswordStoreChangeList changes;
  bool success = login_db_ && login_db_->GetLoginsCreatedBetween(
                                  delete_begin, delete_end, &forms);
  if (success) {
    for (const auto& form : forms) {
      PasswordStoreChangeList remove_changes;
      if (url_filter.Run(form.url) &&
          login_db_->RemoveLogin(form, &remove_changes)) {
        std::move(remove_changes.begin(), remove_changes.end(),
                  std::back_inserter(changes));
      }
    }
  }
  if (password_sync_bridge_ && !changes.empty()) {
    password_sync_bridge_->ActOnPasswordStoreChanges(location, changes);
  }
  // Sync metadata get updated in ActOnPasswordStoreChanges(). Therefore,
  // CommitTransaction() must be called after ActOnPasswordStoreChanges(),
  // because sync codebase needs to update metadata atomically together with
  // the login data.
  CommitTransaction();

  if (sync_completion) {
    deletions_have_synced_callbacks_.push_back(std::move(sync_completion));
    // Start a timeout for sync, or restart it if it was already running.
    deletions_have_synced_timeout_.Reset(
        base::BindOnce(&LoginDatabaseAsyncHelper::NotifyDeletionsHaveSynced,
                       weak_ptr_factory_.GetWeakPtr(),
                       /*success=*/false));
    base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE, deletions_have_synced_timeout_.callback(), kSyncTaskTimeout);

    // Do an immediate check for the case where there are already no unsynced
    // deletions.
    if (!GetMetadataStore()->HasUnsyncedPasswordDeletions()) {
      NotifyDeletionsHaveSynced(/*success=*/true);
    }
  }
  return success ? changes
                 : PasswordChangesOrError(PasswordStoreBackendError(
                       PasswordStoreBackendErrorType::kUncategorized));
}

PasswordStoreChangeList LoginDatabaseAsyncHelper::DisableAutoSignInForOrigins(
    const base::RepeatingCallback<bool(const GURL&)>& origin_filter) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::vector<PasswordForm> forms;
  PasswordStoreChangeList changes;
  if (!login_db_ || !login_db_->GetAutoSignInLogins(&forms)) {
    return changes;
  }

  std::set<GURL> origins_to_update;
  for (const auto& form : forms) {
    if (origin_filter.Run(form.url)) {
      origins_to_update.insert(form.url);
    }
  }

  std::set<GURL> origins_updated;
  for (const GURL& origin : origins_to_update) {
    if (login_db_->DisableAutoSignInForOrigin(origin)) {
      origins_updated.insert(origin);
    }
  }

  for (const auto& form : forms) {
    if (origins_updated.count(form.url)) {
      changes.emplace_back(PasswordStoreChange::UPDATE, std::move(form));
    }
  }
  return changes;
}

// Synchronous implementation for manipulating with statistics.
void LoginDatabaseAsyncHelper::AddSiteStats(const InteractionsStats& stats) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (login_db_) {
    login_db_->stats_table().AddRow(stats);
  }
}

void LoginDatabaseAsyncHelper::RemoveSiteStats(const GURL& origin_domain) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (login_db_) {
    login_db_->stats_table().RemoveRow(origin_domain);
  }
}

std::vector<InteractionsStats> LoginDatabaseAsyncHelper::GetSiteStats(
    const GURL& origin_domain) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return login_db_ ? login_db_->stats_table().GetRows(origin_domain)
                   : std::vector<InteractionsStats>();
}

void LoginDatabaseAsyncHelper::RemoveStatisticsByOriginAndTime(
    const base::RepeatingCallback<bool(const GURL&)>& origin_filter,
    base::Time delete_begin,
    base::Time delete_end) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (login_db_) {
    login_db_->stats_table().RemoveStatsByOriginAndTime(
        origin_filter, delete_begin, delete_end);
  }
}

base::WeakPtr<syncer::DataTypeControllerDelegate>
LoginDatabaseAsyncHelper::GetSyncControllerDelegate() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(password_sync_bridge_);
  return password_sync_bridge_->change_processor()->GetControllerDelegate();
}

PasswordStoreChangeList LoginDatabaseAsyncHelper::AddCredentialSync(
    const sync_pb::PasswordSpecificsData& password,
    AddCredentialError* error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return AddLoginImpl(PasswordFromSpecifics(password), error);
}

PasswordStoreChangeList LoginDatabaseAsyncHelper::UpdateCredentialSync(
    const sync_pb::PasswordSpecificsData& password,
    UpdateCredentialError* error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return UpdateLoginImpl(PasswordFromSpecifics(password), error);
}

void LoginDatabaseAsyncHelper::NotifyCredentialsChanged(
    const PasswordStoreChangeList& changes) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!remote_forms_changes_received_callback_) {
    return;
  }
  main_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(remote_forms_changes_received_callback_,
                                changes, IsAccountStore()));
}

void LoginDatabaseAsyncHelper::NotifyDeletionsHaveSynced(bool success) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Either all deletions have been committed to the Sync server, or Sync is
  // telling us that it won't commit them (because Sync was turned off
  // permanently). In either case, run the corresponding callbacks now (on the
  // main task runner).
  DCHECK(!success || !GetMetadataStore()->HasUnsyncedPasswordDeletions());
  for (auto& callback : deletions_have_synced_callbacks_) {
    main_task_runner_->PostTask(FROM_HERE,
                                base::BindOnce(std::move(callback), success));
  }
  deletions_have_synced_timeout_.Cancel();
  deletions_have_synced_callbacks_.clear();
}

void LoginDatabaseAsyncHelper::NotifyUnsyncedCredentialsWillBeDeleted(
    std::vector<PasswordForm> unsynced_credentials) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(IsAccountStore());
  // |deletion_notifier_| only gets set for desktop.
  if (deletion_notifier_) {
    deletion_notifier_.Run(std::move(unsynced_credentials));
  }
}

bool LoginDatabaseAsyncHelper::BeginTransaction() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (login_db_) {
    return login_db_->BeginTransaction();
  }
  return false;
}

void LoginDatabaseAsyncHelper::RollbackTransaction() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (login_db_) {
    login_db_->RollbackTransaction();
  }
}

bool LoginDatabaseAsyncHelper::CommitTransaction() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (login_db_) {
    return login_db_->CommitTransaction();
  }
  return false;
}

FormRetrievalResult LoginDatabaseAsyncHelper::ReadAllCredentials(
    PrimaryKeyToPasswordSpecificsDataMap* key_to_specifics_map) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!login_db_) {
    return FormRetrievalResult::kDbError;
  }
  std::vector<PasswordForm> forms;
  FormRetrievalResult result = login_db_->GetAllLogins(&forms);
  for (const auto& form : forms) {
    CHECK(form.primary_key.has_value());
    key_to_specifics_map->emplace(
        form.primary_key->value(),
        std::make_unique<sync_pb::PasswordSpecificsData>(
            SpecificsDataFromPassword(form, /*base_password_data=*/{})));
  }

  return result;
}

PasswordStoreChangeList
LoginDatabaseAsyncHelper::RemoveCredentialByPrimaryKeySync(
    FormPrimaryKey primary_key) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  PasswordStoreChangeList changes;
  if (login_db_ && login_db_->RemoveLoginByPrimaryKey(primary_key, &changes)) {
    return changes;
  }
  return PasswordStoreChangeList();
}

PasswordStoreSync::MetadataStore* LoginDatabaseAsyncHelper::GetMetadataStore() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return login_db_ ? &login_db_->password_sync_metadata_store() : nullptr;
}

bool LoginDatabaseAsyncHelper::IsAccountStore() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return login_db_ && login_db_->is_account_store();
}

bool LoginDatabaseAsyncHelper::DeleteAndRecreateDatabaseFile() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return login_db_ && login_db_->DeleteAndRecreateDatabaseFile();
}

DatabaseCleanupResult
LoginDatabaseAsyncHelper::DeleteUndecryptableCredentials() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!login_db_) {
    return DatabaseCleanupResult::kDatabaseUnavailable;
  }
  return login_db_->DeleteUndecryptableLogins();
}

std::optional<bool> LoginDatabaseAsyncHelper::WereUndecryptableLoginsDeleted()
    const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!login_db_) {
    return false;
  }
  return login_db_->were_undecryptable_logins_deleted();
}

void LoginDatabaseAsyncHelper::ClearWereUndecryptableLoginsDeleted() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!login_db_) {
    return;
  }
  login_db_->clear_were_undecryptable_logins_deleted();
}

PasswordStoreChangeList LoginDatabaseAsyncHelper::AddLoginImpl(
    const PasswordForm& form,
    AddCredentialError* error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!login_db_) {
    if (error) {
      *error = AddCredentialError::kDbNotAvailable;
    }
    return PasswordStoreChangeList();
  }
  return login_db_->AddLogin(form, error);
}

PasswordStoreChangeList LoginDatabaseAsyncHelper::UpdateLoginImpl(
    const PasswordForm& form,
    UpdateCredentialError* error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!login_db_) {
    if (error) {
      *error = UpdateCredentialError::kDbNotAvailable;
    }
    return PasswordStoreChangeList();
  }
  return login_db_->UpdateLogin(form, error);
}

// Reports password store metrics that aren't reported by the
// StoreMetricsReporter. Namely, metrics related to inaccessible passwords,
// and bubble statistics.
void LoginDatabaseAsyncHelper::ReportMetrics() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!login_db_) {
    return;
  }
  login_db_->ReportMetrics();
}

}  // namespace password_manager
