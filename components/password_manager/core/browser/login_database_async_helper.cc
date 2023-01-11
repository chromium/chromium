// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/login_database_async_helper.h"

#include "base/functional/bind.h"
#include "base/task/sequenced_task_runner.h"
#include "components/os_crypt/os_crypt.h"
#include "components/password_manager/core/browser/login_database.h"
#include "components/password_manager/core/browser/sync/password_proto_utils.h"
#include "components/password_manager/core/browser/sync/password_sync_bridge.h"
#include "components/sync/model/client_tag_based_model_type_processor.h"
#include "components/sync/model/model_type_controller_delegate.h"

namespace password_manager {

namespace {

constexpr base::TimeDelta kSyncTaskTimeout = base::Seconds(30);

}  // namespace

LoginDatabaseAsyncHelper::LoginDatabaseAsyncHelper(
    std::unique_ptr<LoginDatabase> login_db,
    std::unique_ptr<UnsyncedCredentialsDeletionNotifier> notifier,
    scoped_refptr<base::SequencedTaskRunner> main_task_runner)
    : login_db_(std::move(login_db)),
      deletion_notifier_(std::move(notifier)),
      main_task_runner_(std::move(main_task_runner)) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
  DCHECK(login_db_);
  DCHECK(main_task_runner_);
}

LoginDatabaseAsyncHelper::~LoginDatabaseAsyncHelper() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

bool LoginDatabaseAsyncHelper::Initialize(
    PasswordStoreBackend::RemoteChangesReceived remote_form_changes_received,
    base::RepeatingClosure sync_enabled_or_disabled_cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  remote_forms_changes_received_callback_ =
      std::move(remote_form_changes_received);

  bool success = true;
  if (!login_db_->Init()) {
    login_db_.reset();
    // The initialization should be continued, because PasswordSyncBridge
    // has to be initialized even if database initialization failed.
    success = false;
    LOG(ERROR) << "Could not create/open login database.";
  }
  if (success) {
    login_db_->SetDeletionsHaveSyncedCallback(base::BindRepeating(
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

  sync_bridge_ = std::make_unique<PasswordSyncBridge>(
      std::make_unique<syncer::ClientTagBasedModelTypeProcessor>(
          syncer::PASSWORDS, base::DoNothing()),
      static_cast<PasswordStoreSync*>(this),
      std::move(sync_enabled_or_disabled_cb));

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
  std::vector<std::unique_ptr<PasswordForm>> forms;

  if (!login_db_) {
    return PasswordStoreBackendError(
        PasswordStoreBackendErrorType::kUncategorized,
        PasswordStoreBackendErrorRecoveryType::kUnrecoverable);
  }
  FormRetrievalResult result = login_db_->GetAllLogins(&forms);
  if (result != FormRetrievalResult::kSuccess &&
      result != FormRetrievalResult::kEncryptionServiceFailureWithPartialData) {
    return PasswordStoreBackendError(
        PasswordStoreBackendErrorType::kUncategorized,
        PasswordStoreBackendErrorRecoveryType::kUnrecoverable);
  }
  return forms;
}

LoginsResultOrError LoginDatabaseAsyncHelper::GetAutofillableLogins() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::vector<std::unique_ptr<PasswordForm>> results;
  if (!login_db_ || !login_db_->GetAutofillableLogins(&results)) {
    return PasswordStoreBackendError(
        PasswordStoreBackendErrorType::kUncategorized,
        PasswordStoreBackendErrorRecoveryType::kUnrecoverable);
  }
  return results;
}

LoginsResultOrError LoginDatabaseAsyncHelper::FillMatchingLogins(
    const std::vector<PasswordFormDigest>& forms,
    bool include_psl) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::vector<std::unique_ptr<PasswordForm>> results;
  for (const auto& form : forms) {
    std::vector<std::unique_ptr<PasswordForm>> matched_forms;
    if (!login_db_ || !login_db_->GetLogins(form, include_psl, &matched_forms))
      return PasswordStoreBackendError(
          PasswordStoreBackendErrorType::kUncategorized,
          PasswordStoreBackendErrorRecoveryType::kUnrecoverable);
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
  if (sync_bridge_ && !changes.empty())
    sync_bridge_->ActOnPasswordStoreChanges(changes);
  // Sync metadata get updated in ActOnPasswordStoreChanges(). Therefore,
  // CommitTransaction() must be called after ActOnPasswordStoreChanges(),
  // because sync codebase needs to update metadata atomically together with
  // the login data.
  CommitTransaction();
  return error == AddCredentialError::kNone
             ? changes
             : PasswordChangesOrError(PasswordStoreBackendError(
                   PasswordStoreBackendErrorType::kUncategorized,
                   PasswordStoreBackendErrorRecoveryType::kUnrecoverable));
}

PasswordChangesOrError LoginDatabaseAsyncHelper::UpdateLogin(
    const PasswordForm& form) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  BeginTransaction();
  UpdateCredentialError error = UpdateCredentialError::kNone;
  PasswordStoreChangeList changes = UpdateLoginImpl(form, &error);
  if (sync_bridge_ && !changes.empty())
    sync_bridge_->ActOnPasswordStoreChanges(changes);
  // Sync metadata get updated in ActOnPasswordStoreChanges(). Therefore,
  // CommitTransaction() must be called after ActOnPasswordStoreChanges(),
  // because sync codebase needs to update metadata atomically together with
  // the login data.
  CommitTransaction();
  return error == UpdateCredentialError::kNone
             ? changes
             : PasswordChangesOrError(PasswordStoreBackendError(
                   PasswordStoreBackendErrorType::kUncategorized,
                   PasswordStoreBackendErrorRecoveryType::kUnrecoverable));
}

PasswordChangesOrError LoginDatabaseAsyncHelper::RemoveLogin(
    const PasswordForm& form) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  BeginTransaction();
  PasswordStoreChangeList changes;
  if (login_db_ && login_db_->RemoveLogin(form, &changes)) {
    if (sync_bridge_ && !changes.empty())
      sync_bridge_->ActOnPasswordStoreChanges(changes);
  }
  // Sync metadata get updated in ActOnPasswordStoreChanges(). Therefore,
  // CommitTransaction() must be called after ActOnPasswordStoreChanges(),
  // because sync codebase needs to update metadata atomically together with
  // the login data.
  CommitTransaction();
  return changes;
}

PasswordChangesOrError LoginDatabaseAsyncHelper::RemoveLoginsCreatedBetween(
    base::Time delete_begin,
    base::Time delete_end) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  BeginTransaction();
  PasswordStoreChangeList changes;
  bool success = login_db_ && login_db_->RemoveLoginsCreatedBetween(
                                  delete_begin, delete_end, &changes);
  if (success && sync_bridge_ && !changes.empty())
    sync_bridge_->ActOnPasswordStoreChanges(changes);
  // Sync metadata get updated in ActOnPasswordStoreChanges(). Therefore,
  // CommitTransaction() must be called after ActOnPasswordStoreChanges(),
  // because sync codebase needs to update metadata atomically together with
  // the login data.
  CommitTransaction();
  return success ? changes
                 : PasswordChangesOrError(PasswordStoreBackendError(
                       PasswordStoreBackendErrorType::kUncategorized,
                       PasswordStoreBackendErrorRecoveryType::kUnrecoverable));
}

PasswordChangesOrError LoginDatabaseAsyncHelper::RemoveLoginsByURLAndTime(
    const base::RepeatingCallback<bool(const GURL&)>& url_filter,
    base::Time delete_begin,
    base::Time delete_end,
    base::OnceCallback<void(bool)> sync_completion) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  BeginTransaction();
  std::vector<std::unique_ptr<PasswordForm>> forms;
  PasswordStoreChangeList changes;
  bool success = login_db_ && login_db_->GetLoginsCreatedBetween(
                                  delete_begin, delete_end, &forms);
  if (success) {
    for (const auto& form : forms) {
      PasswordStoreChangeList remove_changes;
      if (url_filter.Run(form->url) &&
          login_db_->RemoveLogin(*form, &remove_changes)) {
        std::move(remove_changes.begin(), remove_changes.end(),
                  std::back_inserter(changes));
      }
    }
  }
  if (sync_bridge_ && !changes.empty())
    sync_bridge_->ActOnPasswordStoreChanges(changes);
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
    if (!GetMetadataStore()->HasUnsyncedDeletions())
      NotifyDeletionsHaveSynced(/*success=*/true);
  }
  return success ? changes
                 : PasswordChangesOrError(PasswordStoreBackendError(
                       PasswordStoreBackendErrorType::kUncategorized,
                       PasswordStoreBackendErrorRecoveryType::kUnrecoverable));
}

PasswordStoreChangeList LoginDatabaseAsyncHelper::DisableAutoSignInForOrigins(
    const base::RepeatingCallback<bool(const GURL&)>& origin_filter) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::vector<std::unique_ptr<PasswordForm>> forms;
  PasswordStoreChangeList changes;
  if (!login_db_ || !login_db_->GetAutoSignInLogins(&forms))
    return changes;

  std::set<GURL> origins_to_update;
  for (const auto& form : forms) {
    if (origin_filter.Run(form->url))
      origins_to_update.insert(form->url);
  }

  std::set<GURL> origins_updated;
  for (const GURL& origin : origins_to_update) {
    if (login_db_->DisableAutoSignInForOrigin(origin))
      origins_updated.insert(origin);
  }

  for (const auto& form : forms) {
    if (origins_updated.count(form->url)) {
      changes.emplace_back(PasswordStoreChange::UPDATE, *form);
    }
  }
  return changes;
}

// Synchronous implementation for manipulating with statistics.
void LoginDatabaseAsyncHelper::AddSiteStats(const InteractionsStats& stats) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (login_db_)
    login_db_->stats_table().AddRow(stats);
}

void LoginDatabaseAsyncHelper::RemoveSiteStats(const GURL& origin_domain) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (login_db_)
    login_db_->stats_table().RemoveRow(origin_domain);
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

// Synchronous implementation for manipulating with field info.
void LoginDatabaseAsyncHelper::AddFieldInfo(const FieldInfo& field_info) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (login_db_)
    login_db_->field_info_table().AddRow(field_info);
}

std::vector<FieldInfo> LoginDatabaseAsyncHelper::GetAllFieldInfo() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return login_db_ ? login_db_->field_info_table().GetAllRows()
                   : std::vector<FieldInfo>();
}

void LoginDatabaseAsyncHelper::RemoveFieldInfoByTime(base::Time remove_begin,
                                                     base::Time remove_end) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (login_db_)
    login_db_->field_info_table().RemoveRowsByTime(remove_begin, remove_end);
}

base::WeakPtr<syncer::ModelTypeControllerDelegate>
LoginDatabaseAsyncHelper::GetSyncControllerDelegate() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return sync_bridge_->change_processor()->GetControllerDelegate();
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
  if (!remote_forms_changes_received_callback_)
    return;
  main_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(remote_forms_changes_received_callback_, changes));
}

void LoginDatabaseAsyncHelper::NotifyDeletionsHaveSynced(bool success) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Either all deletions have been committed to the Sync server, or Sync is
  // telling us that it won't commit them (because Sync was turned off
  // permanently). In either case, run the corresponding callbacks now (on the
  // main task runner).
  DCHECK(!success || !GetMetadataStore()->HasUnsyncedDeletions());
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
    main_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&UnsyncedCredentialsDeletionNotifier::Notify,
                                  deletion_notifier_->GetWeakPtr(),
                                  std::move(unsynced_credentials)));
  }
}

bool LoginDatabaseAsyncHelper::BeginTransaction() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (login_db_)
    return login_db_->BeginTransaction();
  return false;
}

void LoginDatabaseAsyncHelper::RollbackTransaction() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (login_db_)
    login_db_->RollbackTransaction();
}

bool LoginDatabaseAsyncHelper::CommitTransaction() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (login_db_)
    return login_db_->CommitTransaction();
  return false;
}

FormRetrievalResult LoginDatabaseAsyncHelper::ReadAllCredentials(
    PrimaryKeyToPasswordSpecificsDataMap* key_to_specifics_map) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!login_db_)
    return FormRetrievalResult::kDbError;
  std::vector<std::unique_ptr<PasswordForm>> forms;
  FormRetrievalResult result = login_db_->GetAllLogins(&forms);
  for (const auto& form : forms) {
    DCHECK(form->primary_key.has_value());
    key_to_specifics_map->emplace(
        form->primary_key->value(),
        std::make_unique<sync_pb::PasswordSpecificsData>(
            SpecificsDataFromPassword(*form, /*base_password_data=*/{})));
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
  return login_db_.get();
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
  if (!login_db_)
    return DatabaseCleanupResult::kDatabaseUnavailable;
  return login_db_->DeleteUndecryptableLogins();
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
  if (!login_db_)
    return;
  login_db_->ReportMetrics();
}

}  // namespace password_manager
