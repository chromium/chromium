// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_store_impl.h"

#include <iterator>
#include <set>
#include <utility>

#include "base/bind.h"
#include "base/logging.h"
#include "base/stl_util.h"
#include "components/password_manager/core/browser/password_store_change.h"
#include "components/prefs/pref_service.h"

namespace password_manager {

namespace {

// Generates PasswordStoreChangeList for affected forms during
// InsecureCredentials update.
PasswordStoreChangeList BuildPasswordChangeListForInsecureCredentialsUpdate(
    PrimaryKeyToFormMap key_to_form_map) {
  PasswordStoreChangeList changes;
  changes.reserve(key_to_form_map.size());
  for (auto& pair : key_to_form_map) {
    changes.emplace_back(PasswordStoreChange::UPDATE, std::move(*pair.second),
                         pair.first);
  }
  return changes;
}

}  // namespace

PasswordStoreImpl::PasswordStoreImpl(std::unique_ptr<LoginDatabase> login_db)
    : login_db_(std::move(login_db)) {}

PasswordStoreImpl::~PasswordStoreImpl() = default;

void PasswordStoreImpl::ShutdownOnUIThread() {
  PasswordStore::ShutdownOnUIThread();
  ScheduleTask(base::BindOnce(&PasswordStoreImpl::ResetLoginDB, this));
}

bool PasswordStoreImpl::InitOnBackgroundSequence(
    bool upload_phished_credentials_to_sync) {
  DCHECK(background_task_runner()->RunsTasksInCurrentSequence());
  DCHECK(login_db_);
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
        &PasswordStoreImpl::NotifyDeletionsHaveSynced, base::Unretained(this)));
  }
  return PasswordStore::InitOnBackgroundSequence(
             upload_phished_credentials_to_sync) &&
         success;
}

void PasswordStoreImpl::ReportMetricsImpl(const std::string& sync_username,
                                          bool custom_passphrase_sync_enabled,
                                          BulkCheckDone bulk_check_done) {
  if (!login_db_)
    return;
  DCHECK(background_task_runner()->RunsTasksInCurrentSequence());
  login_db_->ReportMetrics(sync_username, custom_passphrase_sync_enabled,
                           bulk_check_done);
}

PasswordStoreChangeList PasswordStoreImpl::AddLoginImpl(
    const PasswordForm& form,
    AddLoginError* error) {
  DCHECK(background_task_runner()->RunsTasksInCurrentSequence());
  if (!login_db_) {
    if (error) {
      *error = AddLoginError::kDbNotAvailable;
    }
    return PasswordStoreChangeList();
  }
  return login_db_->AddLogin(form, error);
}

PasswordStoreChangeList PasswordStoreImpl::UpdateLoginImpl(
    const PasswordForm& form,
    UpdateLoginError* error) {
  DCHECK(background_task_runner()->RunsTasksInCurrentSequence());
  if (!login_db_) {
    if (error) {
      *error = UpdateLoginError::kDbNotAvailable;
    }
    return PasswordStoreChangeList();
  }
  return login_db_->UpdateLogin(form, error);
}

PasswordStoreChangeList PasswordStoreImpl::RemoveLoginImpl(
    const PasswordForm& form) {
  DCHECK(background_task_runner()->RunsTasksInCurrentSequence());
  PasswordStoreChangeList changes;
  if (login_db_ && login_db_->RemoveLogin(form, &changes)) {
    return changes;
  }
  return PasswordStoreChangeList();
}

PasswordStoreChangeList PasswordStoreImpl::RemoveLoginsByURLAndTimeImpl(
    const base::RepeatingCallback<bool(const GURL&)>& url_filter,
    base::Time delete_begin,
    base::Time delete_end) {
  PrimaryKeyToFormMap key_to_form_map;
  PasswordStoreChangeList changes;
  if (login_db_ && login_db_->GetLoginsCreatedBetween(delete_begin, delete_end,
                                                      &key_to_form_map)) {
    for (const auto& pair : key_to_form_map) {
      PasswordForm* form = pair.second.get();
      PasswordStoreChangeList remove_changes;
      if (url_filter.Run(form->url) &&
          login_db_->RemoveLogin(*form, &remove_changes)) {
        std::move(remove_changes.begin(), remove_changes.end(),
                  std::back_inserter(changes));
      }
    }
  }
  return changes;
}

PasswordStoreChangeList PasswordStoreImpl::RemoveLoginsCreatedBetweenImpl(
    base::Time delete_begin,
    base::Time delete_end) {
  PasswordStoreChangeList changes;
  if (!login_db_ || !login_db_->RemoveLoginsCreatedBetween(
                        delete_begin, delete_end, &changes)) {
    return PasswordStoreChangeList();
  }
  return changes;
}

PasswordStoreChangeList PasswordStoreImpl::DisableAutoSignInForOriginsImpl(
    const base::RepeatingCallback<bool(const GURL&)>& origin_filter) {
  PrimaryKeyToFormMap key_to_form_map;
  PasswordStoreChangeList changes;
  if (!login_db_ || !login_db_->GetAutoSignInLogins(&key_to_form_map))
    return changes;

  std::set<GURL> origins_to_update;
  for (const auto& pair : key_to_form_map) {
    if (origin_filter.Run(pair.second->url))
      origins_to_update.insert(pair.second->url);
  }

  std::set<GURL> origins_updated;
  for (const GURL& origin : origins_to_update) {
    if (login_db_->DisableAutoSignInForOrigin(origin))
      origins_updated.insert(origin);
  }

  for (const auto& pair : key_to_form_map) {
    if (origins_updated.count(pair.second->url)) {
      changes.emplace_back(PasswordStoreChange::UPDATE, *pair.second,
                           FormPrimaryKey(pair.first));
    }
  }

  return changes;
}

bool PasswordStoreImpl::RemoveStatisticsByOriginAndTimeImpl(
    const base::RepeatingCallback<bool(const GURL&)>& origin_filter,
    base::Time delete_begin,
    base::Time delete_end) {
  return login_db_ && login_db_->stats_table().RemoveStatsByOriginAndTime(
                          origin_filter, delete_begin, delete_end);
}

std::vector<std::unique_ptr<PasswordForm>>
PasswordStoreImpl::FillMatchingLogins(const FormDigest& form) {
  std::vector<std::unique_ptr<PasswordForm>> matched_forms;
  if (login_db_ && !login_db_->GetLogins(form, &matched_forms))
    return std::vector<std::unique_ptr<PasswordForm>>();
  return matched_forms;
}

std::vector<std::unique_ptr<PasswordForm>>
PasswordStoreImpl::FillMatchingLoginsByPassword(
    const std::u16string& plain_text_password) {
  std::vector<std::unique_ptr<PasswordForm>> matched_forms;
  if (login_db_ &&
      !login_db_->GetLoginsByPassword(plain_text_password, &matched_forms))
    return std::vector<std::unique_ptr<PasswordForm>>();
  return matched_forms;
}

bool PasswordStoreImpl::FillAutofillableLogins(
    std::vector<std::unique_ptr<PasswordForm>>* forms) {
  DCHECK(background_task_runner()->RunsTasksInCurrentSequence());
  return login_db_ && login_db_->GetAutofillableLogins(forms);
}

bool PasswordStoreImpl::FillBlocklistLogins(
    std::vector<std::unique_ptr<PasswordForm>>* forms) {
  DCHECK(background_task_runner()->RunsTasksInCurrentSequence());
  return login_db_ && login_db_->GetBlocklistLogins(forms);
}

DatabaseCleanupResult PasswordStoreImpl::DeleteUndecryptableLogins() {
  DCHECK(background_task_runner()->RunsTasksInCurrentSequence());
  if (!login_db_)
    return DatabaseCleanupResult::kDatabaseUnavailable;
  return login_db_->DeleteUndecryptableLogins();
}

void PasswordStoreImpl::AddSiteStatsImpl(const InteractionsStats& stats) {
  DCHECK(background_task_runner()->RunsTasksInCurrentSequence());
  if (login_db_)
    login_db_->stats_table().AddRow(stats);
}

void PasswordStoreImpl::RemoveSiteStatsImpl(const GURL& origin_domain) {
  DCHECK(background_task_runner()->RunsTasksInCurrentSequence());
  if (login_db_)
    login_db_->stats_table().RemoveRow(origin_domain);
}

std::vector<InteractionsStats> PasswordStoreImpl::GetAllSiteStatsImpl() {
  DCHECK(background_task_runner()->RunsTasksInCurrentSequence());
  return login_db_ ? login_db_->stats_table().GetAllRows()
                   : std::vector<InteractionsStats>();
}

std::vector<InteractionsStats> PasswordStoreImpl::GetSiteStatsImpl(
    const GURL& origin_domain) {
  DCHECK(background_task_runner()->RunsTasksInCurrentSequence());
  return login_db_ ? login_db_->stats_table().GetRows(origin_domain)
                   : std::vector<InteractionsStats>();
}

PasswordStoreChangeList PasswordStoreImpl::AddInsecureCredentialImpl(
    const InsecureCredential& credential) {
  DCHECK(background_task_runner()->RunsTasksInCurrentSequence());
  if (!login_db_ ||
      !login_db_->insecure_credentials_table().AddRow(credential)) {
    return {};
  }

  PrimaryKeyToFormMap key_to_form_map;
  if (login_db_->GetLoginsBySignonRealmAndUsername(
          credential.signon_realm, credential.username, key_to_form_map) !=
      FormRetrievalResult::kSuccess) {
    return {};
  }

  return BuildPasswordChangeListForInsecureCredentialsUpdate(
      std::move(key_to_form_map));
}

PasswordStoreChangeList PasswordStoreImpl::RemoveInsecureCredentialsImpl(
    const std::string& signon_realm,
    const std::u16string& username,
    RemoveInsecureCredentialsReason reason) {
  DCHECK(background_task_runner()->RunsTasksInCurrentSequence());
  if (!login_db_ || !login_db_->insecure_credentials_table().RemoveRow(
                        signon_realm, username, reason)) {
    return {};
  }

  PrimaryKeyToFormMap key_to_form_map;
  if (login_db_->GetLoginsBySignonRealmAndUsername(signon_realm, username,
                                                   key_to_form_map) !=
      FormRetrievalResult::kSuccess) {
    return {};
  }

  return BuildPasswordChangeListForInsecureCredentialsUpdate(
      std::move(key_to_form_map));
}

std::vector<InsecureCredential>
PasswordStoreImpl::GetAllInsecureCredentialsImpl() {
  DCHECK(background_task_runner()->RunsTasksInCurrentSequence());
  std::vector<InsecureCredential> insecure_credentials =
      login_db_ ? login_db_->insecure_credentials_table().GetAllRows()
                : std::vector<InsecureCredential>();
  PasswordForm::Store store = IsAccountStore()
                                  ? PasswordForm::Store::kAccountStore
                                  : PasswordForm::Store::kProfileStore;
  for (InsecureCredential& cred : insecure_credentials)
    cred.in_store = store;
  return insecure_credentials;
}

std::vector<InsecureCredential>
PasswordStoreImpl::GetMatchingInsecureCredentialsImpl(
    const std::string& signon_realm) {
  DCHECK(background_task_runner()->RunsTasksInCurrentSequence());
  std::vector<InsecureCredential> insecure_credentials =
      login_db_ ? login_db_->insecure_credentials_table().GetRows(signon_realm)
                : std::vector<InsecureCredential>();
  PasswordForm::Store store = IsAccountStore()
                                  ? PasswordForm::Store::kAccountStore
                                  : PasswordForm::Store::kProfileStore;
  for (InsecureCredential& cred : insecure_credentials)
    cred.in_store = store;
  return insecure_credentials;
}

void PasswordStoreImpl::AddFieldInfoImpl(const FieldInfo& field_info) {
  if (login_db_)
    login_db_->field_info_table().AddRow(field_info);
}

std::vector<FieldInfo> PasswordStoreImpl::GetAllFieldInfoImpl() {
  return login_db_ ? login_db_->field_info_table().GetAllRows()
                   : std::vector<FieldInfo>();
}

void PasswordStoreImpl::RemoveFieldInfoByTimeImpl(base::Time remove_begin,
                                                  base::Time remove_end) {
  if (login_db_)
    login_db_->field_info_table().RemoveRowsByTime(remove_begin, remove_end);
}

bool PasswordStoreImpl::IsEmpty() {
  if (!login_db_)
    return true;
  return login_db_->IsEmpty();
}

bool PasswordStoreImpl::BeginTransaction() {
  if (login_db_)
    return login_db_->BeginTransaction();
  return false;
}

void PasswordStoreImpl::RollbackTransaction() {
  if (login_db_)
    login_db_->RollbackTransaction();
}

bool PasswordStoreImpl::CommitTransaction() {
  if (login_db_)
    return login_db_->CommitTransaction();
  return false;
}

FormRetrievalResult PasswordStoreImpl::ReadAllLogins(
    PrimaryKeyToFormMap* key_to_form_map) {
  DCHECK(background_task_runner()->RunsTasksInCurrentSequence());
  if (!login_db_)
    return FormRetrievalResult::kDbError;
  return login_db_->GetAllLogins(key_to_form_map);
}

std::vector<InsecureCredential> PasswordStoreImpl::ReadSecurityIssues(
    FormPrimaryKey parent_key) {
  if (!login_db_)
    return {};
  return login_db_->insecure_credentials_table().GetRows(parent_key);
}

PasswordStoreChangeList PasswordStoreImpl::RemoveLoginByPrimaryKeySync(
    FormPrimaryKey primary_key) {
  DCHECK(background_task_runner()->RunsTasksInCurrentSequence());
  PasswordStoreChangeList changes;
  if (login_db_ && login_db_->RemoveLoginByPrimaryKey(primary_key, &changes)) {
    return changes;
  }
  return PasswordStoreChangeList();
}

PasswordStoreSync::MetadataStore* PasswordStoreImpl::GetMetadataStore() {
  return login_db_.get();
}

bool PasswordStoreImpl::IsAccountStore() const {
  return login_db_ && login_db_->is_account_store();
}

bool PasswordStoreImpl::DeleteAndRecreateDatabaseFile() {
  return login_db_ && login_db_->DeleteAndRecreateDatabaseFile();
}

void PasswordStoreImpl::ResetLoginDB() {
  DCHECK(background_task_runner()->RunsTasksInCurrentSequence());
  login_db_.reset();
}

}  // namespace password_manager
