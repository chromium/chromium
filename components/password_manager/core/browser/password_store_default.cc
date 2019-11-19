// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_store_default.h"

#include <iterator>
#include <set>
#include <utility>

#include "base/bind.h"
#include "base/logging.h"
#include "base/stl_util.h"
#include "components/password_manager/core/browser/password_store_change.h"
#include "components/prefs/pref_service.h"

using autofill::PasswordForm;

namespace password_manager {

PasswordStoreDefault::PasswordStoreDefault(
    std::unique_ptr<LoginDatabase> login_db)
    : login_db_(std::move(login_db)) {}

PasswordStoreDefault::~PasswordStoreDefault() {
}

void PasswordStoreDefault::ShutdownOnUIThread() {
  PasswordStore::ShutdownOnUIThread();
  ScheduleTask(base::BindOnce(&PasswordStoreDefault::ResetLoginDB, this));
}

bool PasswordStoreDefault::InitOnBackgroundSequence(
    const syncer::SyncableService::StartSyncFlare& flare) {
  DCHECK(background_task_runner()->RunsTasksInCurrentSequence());
  DCHECK(login_db_);
  bool success = true;
  if (!login_db_->Init()) {
    login_db_.reset();
    // The initialization should be continued, because PasswordSyncableService
    // has to be initialized even if database initialization failed.
    success = false;
    LOG(ERROR) << "Could not create/open login database.";
  }
  return PasswordStore::InitOnBackgroundSequence(flare) && success;
}

void PasswordStoreDefault::ReportMetricsImpl(
    const std::string& sync_username,
    bool custom_passphrase_sync_enabled) {
  if (!login_db_)
    return;
  DCHECK(background_task_runner()->RunsTasksInCurrentSequence());
  login_db_->ReportMetrics(sync_username, custom_passphrase_sync_enabled);
}

PasswordStoreChangeList PasswordStoreDefault::AddLoginImpl(
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

PasswordStoreChangeList PasswordStoreDefault::UpdateLoginImpl(
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

PasswordStoreChangeList PasswordStoreDefault::RemoveLoginImpl(
    const PasswordForm& form) {
  DCHECK(background_task_runner()->RunsTasksInCurrentSequence());
  PasswordStoreChangeList changes;
  if (login_db_ && login_db_->RemoveLogin(form, &changes)) {
    return changes;
  }
  return PasswordStoreChangeList();
}

PasswordStoreChangeList PasswordStoreDefault::RemoveLoginsByURLAndTimeImpl(
    const base::Callback<bool(const GURL&)>& url_filter,
    base::Time delete_begin,
    base::Time delete_end) {
  PrimaryKeyToFormMap key_to_form_map;
  PasswordStoreChangeList changes;
  if (login_db_ && login_db_->GetLoginsCreatedBetween(delete_begin, delete_end,
                                                      &key_to_form_map)) {
    for (const auto& pair : key_to_form_map) {
      PasswordForm* form = pair.second.get();
      PasswordStoreChangeList remove_changes;
      if (url_filter.Run(form->origin) &&
          login_db_->RemoveLogin(*form, &remove_changes)) {
        std::move(remove_changes.begin(), remove_changes.end(),
                  std::back_inserter(changes));
      }
    }
  }
  return changes;
}

PasswordStoreChangeList PasswordStoreDefault::RemoveLoginsCreatedBetweenImpl(
    base::Time delete_begin,
    base::Time delete_end) {
  PasswordStoreChangeList changes;
  if (!login_db_ || !login_db_->RemoveLoginsCreatedBetween(
                        delete_begin, delete_end, &changes)) {
    return PasswordStoreChangeList();
  }
  return changes;
}

PasswordStoreChangeList PasswordStoreDefault::DisableAutoSignInForOriginsImpl(
    const base::Callback<bool(const GURL&)>& origin_filter) {
  PrimaryKeyToFormMap key_to_form_map;
  PasswordStoreChangeList changes;
  if (!login_db_ || !login_db_->GetAutoSignInLogins(&key_to_form_map))
    return changes;

  std::set<GURL> origins_to_update;
  for (const auto& pair : key_to_form_map) {
    if (origin_filter.Run(pair.second->origin))
      origins_to_update.insert(pair.second->origin);
  }

  std::set<GURL> origins_updated;
  for (const GURL& origin : origins_to_update) {
    if (login_db_->DisableAutoSignInForOrigin(origin))
      origins_updated.insert(origin);
  }

  for (const auto& pair : key_to_form_map) {
    if (origins_updated.count(pair.second->origin)) {
      changes.emplace_back(PasswordStoreChange::UPDATE, *pair.second,
                           /*primary_key=*/pair.first);
    }
  }

  return changes;
}

bool PasswordStoreDefault::RemoveStatisticsByOriginAndTimeImpl(
    const base::Callback<bool(const GURL&)>& origin_filter,
    base::Time delete_begin,
    base::Time delete_end) {
  return login_db_ &&
         login_db_->stats_table().RemoveStatsByOriginAndTime(
             origin_filter, delete_begin, delete_end);
}

std::vector<std::unique_ptr<PasswordForm>>
PasswordStoreDefault::FillMatchingLogins(const FormDigest& form) {
  std::vector<std::unique_ptr<PasswordForm>> matched_forms;
  if (login_db_ && !login_db_->GetLogins(form, &matched_forms))
    return std::vector<std::unique_ptr<PasswordForm>>();
  return matched_forms;
}

std::vector<std::unique_ptr<PasswordForm>>
PasswordStoreDefault::FillMatchingLoginsByPassword(
    const base::string16& plain_text_password) {
  std::vector<std::unique_ptr<PasswordForm>> matched_forms;
  if (login_db_ &&
      !login_db_->GetLoginsByPassword(plain_text_password, &matched_forms))
    return std::vector<std::unique_ptr<PasswordForm>>();
  return matched_forms;
}

bool PasswordStoreDefault::FillAutofillableLogins(
    std::vector<std::unique_ptr<PasswordForm>>* forms) {
  DCHECK(background_task_runner()->RunsTasksInCurrentSequence());
  return login_db_ && login_db_->GetAutofillableLogins(forms);
}

bool PasswordStoreDefault::FillBlacklistLogins(
    std::vector<std::unique_ptr<PasswordForm>>* forms) {
  DCHECK(background_task_runner()->RunsTasksInCurrentSequence());
  return login_db_ && login_db_->GetBlacklistLogins(forms);
}

DatabaseCleanupResult PasswordStoreDefault::DeleteUndecryptableLogins() {
  DCHECK(background_task_runner()->RunsTasksInCurrentSequence());
  if (!login_db_)
    return DatabaseCleanupResult::kDatabaseUnavailable;
  return login_db_->DeleteUndecryptableLogins();
}

void PasswordStoreDefault::AddSiteStatsImpl(const InteractionsStats& stats) {
  DCHECK(background_task_runner()->RunsTasksInCurrentSequence());
  if (login_db_)
    login_db_->stats_table().AddRow(stats);
}

void PasswordStoreDefault::RemoveSiteStatsImpl(const GURL& origin_domain) {
  DCHECK(background_task_runner()->RunsTasksInCurrentSequence());
  if (login_db_)
    login_db_->stats_table().RemoveRow(origin_domain);
}

std::vector<InteractionsStats> PasswordStoreDefault::GetAllSiteStatsImpl() {
  DCHECK(background_task_runner()->RunsTasksInCurrentSequence());
  return login_db_ ? login_db_->stats_table().GetAllRows()
                   : std::vector<InteractionsStats>();
}

std::vector<InteractionsStats> PasswordStoreDefault::GetSiteStatsImpl(
    const GURL& origin_domain) {
  DCHECK(background_task_runner()->RunsTasksInCurrentSequence());
  return login_db_ ? login_db_->stats_table().GetRows(origin_domain)
                   : std::vector<InteractionsStats>();
}

void PasswordStoreDefault::AddCompromisedCredentialsImpl(
    const CompromisedCredentials& compromised_credentials) {
  DCHECK(background_task_runner()->RunsTasksInCurrentSequence());
  if (login_db_)
    login_db_->compromised_credentials_table().AddRow(compromised_credentials);
}

void PasswordStoreDefault::RemoveCompromisedCredentialsImpl(
    const GURL& url,
    const base::string16& username) {
  DCHECK(background_task_runner()->RunsTasksInCurrentSequence());
  if (login_db_)
    login_db_->compromised_credentials_table().RemoveRow(url, username);
}

std::vector<CompromisedCredentials>
PasswordStoreDefault::GetAllCompromisedCredentialsImpl() {
  DCHECK(background_task_runner()->RunsTasksInCurrentSequence());
  return login_db_ ? login_db_->compromised_credentials_table().GetAllRows()
                   : std::vector<CompromisedCredentials>();
}

void PasswordStoreDefault::RemoveCompromisedCredentialsByUrlAndTimeImpl(
    const base::RepeatingCallback<bool(const GURL&)>& url_filter,
    base::Time remove_begin,
    base::Time remove_end) {
  if (login_db_) {
    login_db_->compromised_credentials_table().RemoveRowsByUrlAndTime(
        url_filter, remove_begin, remove_end);
  }
}

void PasswordStoreDefault::AddFieldInfoImpl(const FieldInfo& field_info) {
  if (login_db_)
    login_db_->field_info_table().AddRow(field_info);
}

std::vector<FieldInfo> PasswordStoreDefault::GetAllFieldInfoImpl() {
  return login_db_ ? login_db_->field_info_table().GetAllRows()
                   : std::vector<FieldInfo>();
}

void PasswordStoreDefault::RemoveFieldInfoByTimeImpl(base::Time remove_begin,
                                                     base::Time remove_end) {
  if (login_db_)
    login_db_->field_info_table().RemoveRowsByTime(remove_begin, remove_end);
}

bool PasswordStoreDefault::BeginTransaction() {
  if (login_db_)
    return login_db_->BeginTransaction();
  return false;
}

void PasswordStoreDefault::RollbackTransaction() {
  if (login_db_)
    login_db_->RollbackTransaction();
}

bool PasswordStoreDefault::CommitTransaction() {
  if (login_db_)
    return login_db_->CommitTransaction();
  return false;
}

FormRetrievalResult PasswordStoreDefault::ReadAllLogins(
    PrimaryKeyToFormMap* key_to_form_map) {
  DCHECK(background_task_runner()->RunsTasksInCurrentSequence());
  if (!login_db_)
    return FormRetrievalResult::kDbError;
  return login_db_->GetAllLogins(key_to_form_map);
}

PasswordStoreChangeList PasswordStoreDefault::RemoveLoginByPrimaryKeySync(
    int primary_key) {
  DCHECK(background_task_runner()->RunsTasksInCurrentSequence());
  PasswordStoreChangeList changes;
  if (login_db_ && login_db_->RemoveLoginByPrimaryKey(primary_key, &changes)) {
    return changes;
  }
  return PasswordStoreChangeList();
}

PasswordStoreSync::MetadataStore* PasswordStoreDefault::GetMetadataStore() {
  return login_db_.get();
}

bool PasswordStoreDefault::IsAccountStore() const {
  return login_db_->is_account_store();
}

bool PasswordStoreDefault::DeleteAndRecreateDatabaseFile() {
  return login_db_->DeleteAndRecreateDatabaseFile();
}

void PasswordStoreDefault::ResetLoginDB() {
  DCHECK(background_task_runner()->RunsTasksInCurrentSequence());
  login_db_.reset();
}

}  // namespace password_manager
