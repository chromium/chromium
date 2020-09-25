// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/test_password_store.h"

#include <stddef.h>

#include <memory>

#include "base/check_op.h"
#include "base/notreached.h"
#include "base/stl_util.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "components/password_manager/core/browser/compromised_credentials_table.h"
#include "components/password_manager/core/browser/login_database.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/psl_matching_helper.h"
#include "components/password_manager/core/browser/statistics_table.h"
#include "components/sync/protocol/entity_metadata.pb.h"
#include "components/sync/protocol/model_type_state.pb.h"
#include "url/gurl.h"

namespace password_manager {

namespace {

class TestPasswordSyncMetadataStore : public PasswordStoreSync::MetadataStore {
 public:
  TestPasswordSyncMetadataStore() = default;
  ~TestPasswordSyncMetadataStore() override = default;

  // PasswordStoreSync::MetadataStore interface.
  bool UpdateSyncMetadata(syncer::ModelType model_type,
                          const std::string& storage_key,
                          const sync_pb::EntityMetadata& metadata) override;
  bool ClearSyncMetadata(syncer::ModelType model_type,
                         const std::string& storage_key) override;
  bool UpdateModelTypeState(
      syncer::ModelType model_type,
      const sync_pb::ModelTypeState& model_type_state) override;
  bool ClearModelTypeState(syncer::ModelType model_type) override;
  std::unique_ptr<syncer::MetadataBatch> GetAllSyncMetadata() override;
  void DeleteAllSyncMetadata() override;
  void SetDeletionsHaveSyncedCallback(
      base::RepeatingCallback<void(bool)> callback) override;
  bool HasUnsyncedDeletions() override;

 private:
  sync_pb::ModelTypeState sync_model_type_state_;
  std::map<std::string, sync_pb::EntityMetadata> sync_metadata_;
};

bool TestPasswordSyncMetadataStore::UpdateSyncMetadata(
    syncer::ModelType model_type,
    const std::string& storage_key,
    const sync_pb::EntityMetadata& metadata) {
  DCHECK_EQ(model_type, syncer::PASSWORDS);
  sync_metadata_[storage_key] = metadata;
  return true;
}

bool TestPasswordSyncMetadataStore::ClearSyncMetadata(
    syncer::ModelType model_type,
    const std::string& storage_key) {
  sync_metadata_.erase(storage_key);
  return true;
}

bool TestPasswordSyncMetadataStore::UpdateModelTypeState(
    syncer::ModelType model_type,
    const sync_pb::ModelTypeState& model_type_state) {
  DCHECK_EQ(model_type, syncer::PASSWORDS);
  sync_model_type_state_ = model_type_state;
  return true;
}

bool TestPasswordSyncMetadataStore::ClearModelTypeState(
    syncer::ModelType model_type) {
  DCHECK_EQ(model_type, syncer::PASSWORDS);
  sync_model_type_state_ = sync_pb::ModelTypeState();
  return true;
}

std::unique_ptr<syncer::MetadataBatch>
TestPasswordSyncMetadataStore::GetAllSyncMetadata() {
  auto metadata_batch = std::make_unique<syncer::MetadataBatch>();
  for (const auto& storage_key_and_metadata : sync_metadata_) {
    metadata_batch->AddMetadata(storage_key_and_metadata.first,
                                std::make_unique<sync_pb::EntityMetadata>(
                                    storage_key_and_metadata.second));
  }
  metadata_batch->SetModelTypeState(sync_model_type_state_);
  return metadata_batch;
}

void TestPasswordSyncMetadataStore::DeleteAllSyncMetadata() {
  ClearModelTypeState(syncer::PASSWORDS);
  sync_metadata_.clear();
}

void TestPasswordSyncMetadataStore::SetDeletionsHaveSyncedCallback(
    base::RepeatingCallback<void(bool)> callback) {
  NOTIMPLEMENTED();
}

bool TestPasswordSyncMetadataStore::HasUnsyncedDeletions() {
  return false;
}

}  // namespace

TestPasswordStore::TestPasswordStore(
    password_manager::IsAccountStore is_account_store)
    : is_account_store_(is_account_store),
      metadata_store_(std::make_unique<TestPasswordSyncMetadataStore>()) {}

TestPasswordStore::~TestPasswordStore() = default;

const TestPasswordStore::PasswordMap& TestPasswordStore::stored_passwords()
    const {
  return stored_passwords_;
}

void TestPasswordStore::Clear() {
  stored_passwords_.clear();
}

bool TestPasswordStore::IsEmpty() {
  // The store is empty, if the sum of all stored passwords across all entries
  // in |stored_passwords_| is 0.
  size_t number_of_passwords = 0u;
  for (auto it = stored_passwords_.begin();
       !number_of_passwords && it != stored_passwords_.end(); ++it) {
    number_of_passwords += it->second.size();
  }
  return number_of_passwords == 0u;
}

scoped_refptr<base::SequencedTaskRunner>
TestPasswordStore::CreateBackgroundTaskRunner() const {
  return base::SequencedTaskRunnerHandle::Get();
}

PasswordStoreChangeList TestPasswordStore::AddLoginImpl(
    const PasswordForm& form,
    AddLoginError* error) {
  if (error)
    *error = AddLoginError::kNone;

  PasswordStoreChangeList changes;
  auto& passwords_for_signon_realm = stored_passwords_[form.signon_realm];
  auto iter = std::find_if(
      passwords_for_signon_realm.begin(), passwords_for_signon_realm.end(),
      [&form](const auto& password) {
        return ArePasswordFormUniqueKeysEqual(form, password);
      });

  if (iter != passwords_for_signon_realm.end()) {
    changes.emplace_back(PasswordStoreChange::REMOVE, *iter);
    changes.emplace_back(PasswordStoreChange::ADD, form);
    *iter = form;
    iter->in_store = IsAccountStore() ? PasswordForm::Store::kAccountStore
                                      : PasswordForm::Store::kProfileStore;
    return changes;
  }

  changes.emplace_back(PasswordStoreChange::ADD, form);
  passwords_for_signon_realm.push_back(form);
  passwords_for_signon_realm.back().in_store =
      IsAccountStore() ? PasswordForm::Store::kAccountStore
                       : PasswordForm::Store::kProfileStore;
  return changes;
}

PasswordStoreChangeList TestPasswordStore::UpdateLoginImpl(
    const PasswordForm& form,
    UpdateLoginError* error) {
  if (error)
    *error = UpdateLoginError::kNone;

  PasswordStoreChangeList changes;
  std::vector<PasswordForm>& forms = stored_passwords_[form.signon_realm];
  for (auto& stored_form : forms) {
    if (ArePasswordFormUniqueKeysEqual(form, stored_form)) {
      stored_form = form;
      stored_form.in_store = IsAccountStore()
                                 ? PasswordForm::Store::kAccountStore
                                 : PasswordForm::Store::kProfileStore;
      changes.push_back(PasswordStoreChange(PasswordStoreChange::UPDATE, form));
    }
  }
  return changes;
}

PasswordStoreChangeList TestPasswordStore::RemoveLoginImpl(
    const PasswordForm& form) {
  PasswordStoreChangeList changes;
  std::vector<PasswordForm>& forms = stored_passwords_[form.signon_realm];
  auto it = forms.begin();
  while (it != forms.end()) {
    if (ArePasswordFormUniqueKeysEqual(form, *it)) {
      it = forms.erase(it);
      changes.push_back(PasswordStoreChange(PasswordStoreChange::REMOVE, form));
    } else {
      ++it;
    }
  }
  return changes;
}

std::vector<std::unique_ptr<PasswordForm>>
TestPasswordStore::FillMatchingLogins(const FormDigest& form) {
  ++fill_matching_logins_calls_;
  std::vector<std::unique_ptr<PasswordForm>> matched_forms;
  for (const auto& elements : stored_passwords_) {
    // The code below doesn't support PSL federated credential. It's doable but
    // no tests need it so far.
    const bool realm_matches = elements.first == form.signon_realm;
    const bool realm_psl_matches =
        IsPublicSuffixDomainMatch(elements.first, form.signon_realm);
    if (realm_matches || realm_psl_matches ||
        (form.scheme == PasswordForm::Scheme::kHtml &&
         password_manager::IsFederatedRealm(elements.first, form.url))) {
      const bool is_psl = !realm_matches && realm_psl_matches;
      for (const auto& stored_form : elements.second) {
        // Repeat the condition above with an additional check for origin.
        if (realm_matches || realm_psl_matches ||
            (form.scheme == PasswordForm::Scheme::kHtml &&
             stored_form.url.GetOrigin() == form.url.GetOrigin() &&
             password_manager::IsFederatedRealm(stored_form.signon_realm,
                                                form.url))) {
          matched_forms.push_back(std::make_unique<PasswordForm>(stored_form));
          matched_forms.back()->is_public_suffix_match = is_psl;
        }
      }
    }
  }
  return matched_forms;
}

std::vector<std::unique_ptr<PasswordForm>>
TestPasswordStore::FillMatchingLoginsByPassword(
    const base::string16& plain_text_password) {
  std::vector<std::unique_ptr<PasswordForm>> matched_forms;
  for (const auto& elements : stored_passwords_) {
    for (const auto& password_form : elements.second) {
      if (password_form.password_value == plain_text_password)
        matched_forms.push_back(std::make_unique<PasswordForm>(password_form));
    }
  }
  return matched_forms;
}

bool TestPasswordStore::FillAutofillableLogins(
    std::vector<std::unique_ptr<PasswordForm>>* forms) {
  for (const auto& forms_for_realm : stored_passwords_) {
    for (const PasswordForm& form : forms_for_realm.second) {
      if (!form.blocked_by_user)
        forms->push_back(std::make_unique<PasswordForm>(form));
    }
  }
  return true;
}

bool TestPasswordStore::FillBlacklistLogins(
    std::vector<std::unique_ptr<PasswordForm>>* forms) {
  for (const auto& forms_for_realm : stored_passwords_) {
    for (const PasswordForm& form : forms_for_realm.second) {
      if (form.blocked_by_user)
        forms->push_back(std::make_unique<PasswordForm>(form));
    }
  }
  return true;
}

DatabaseCleanupResult TestPasswordStore::DeleteUndecryptableLogins() {
  return DatabaseCleanupResult::kSuccess;
}

std::vector<InteractionsStats> TestPasswordStore::GetSiteStatsImpl(
    const GURL& origin_domain) {
  return std::vector<InteractionsStats>();
}

void TestPasswordStore::ReportMetricsImpl(const std::string& sync_username,
                                          bool custom_passphrase_sync_enabled,
                                          BulkCheckDone bulk_check_done) {
  NOTIMPLEMENTED();
}

PasswordStoreChangeList TestPasswordStore::RemoveLoginsByURLAndTimeImpl(
    const base::RepeatingCallback<bool(const GURL&)>& url_filter,
    base::Time begin,
    base::Time end) {
  NOTIMPLEMENTED();
  return PasswordStoreChangeList();
}

PasswordStoreChangeList TestPasswordStore::RemoveLoginsCreatedBetweenImpl(
    base::Time begin,
    base::Time end) {
  NOTIMPLEMENTED();
  return PasswordStoreChangeList();
}

PasswordStoreChangeList TestPasswordStore::DisableAutoSignInForOriginsImpl(
    const base::RepeatingCallback<bool(const GURL&)>& origin_filter) {
  NOTIMPLEMENTED();
  return PasswordStoreChangeList();
}

bool TestPasswordStore::RemoveStatisticsByOriginAndTimeImpl(
    const base::RepeatingCallback<bool(const GURL&)>& origin_filter,
    base::Time delete_begin,
    base::Time delete_end) {
  NOTIMPLEMENTED();
  return false;
}

void TestPasswordStore::AddSiteStatsImpl(const InteractionsStats& stats) {
  NOTIMPLEMENTED();
}

void TestPasswordStore::RemoveSiteStatsImpl(const GURL& origin_domain) {
  NOTIMPLEMENTED();
}

std::vector<InteractionsStats> TestPasswordStore::GetAllSiteStatsImpl() {
  NOTIMPLEMENTED();
  return std::vector<InteractionsStats>();
}

bool TestPasswordStore::AddCompromisedCredentialsImpl(
    const CompromisedCredentials& compromised_credentials) {
  CompromisedCredentials cred = compromised_credentials;
  cred.in_store = IsAccountStore() ? PasswordForm::Store::kAccountStore
                                   : PasswordForm::Store::kProfileStore;
  return compromised_credentials_.insert(std::move(cred)).second;
}

bool TestPasswordStore::RemoveCompromisedCredentialsImpl(
    const std::string& signon_realm,
    const base::string16& username,
    RemoveCompromisedCredentialsReason reason) {
  const size_t old_size = compromised_credentials_.size();
  base::EraseIf(compromised_credentials_, [&](const auto& credential) {
    return credential.signon_realm == signon_realm &&
           credential.username == username;
  });

  return old_size != compromised_credentials_.size();
}

bool TestPasswordStore::RemoveCompromisedCredentialsByCompromiseTypeImpl(
    const std::string& signon_realm,
    const base::string16& username,
    const CompromiseType& compromise_type,
    RemoveCompromisedCredentialsReason reason) {
  const size_t old_size = compromised_credentials_.size();
  base::EraseIf(compromised_credentials_, [&](const auto& credential) {
    return credential.signon_realm == signon_realm &&
           credential.username == username &&
           credential.compromise_type == compromise_type;
  });
  return old_size != compromised_credentials_.size();
}

std::vector<CompromisedCredentials>
TestPasswordStore::GetAllCompromisedCredentialsImpl() {
  return std::vector<CompromisedCredentials>(compromised_credentials_.begin(),
                                             compromised_credentials_.end());
}

std::vector<CompromisedCredentials>
TestPasswordStore::GetMatchingCompromisedCredentialsImpl(
    const std::string& signon_realm) {
  std::vector<CompromisedCredentials> result;
  std::copy_if(compromised_credentials_.begin(), compromised_credentials_.end(),
               std::back_inserter(result),
               [&signon_realm](const CompromisedCredentials& credential) {
                 return credential.signon_realm == signon_realm;
               });
  return result;
}

bool TestPasswordStore::RemoveCompromisedCredentialsByUrlAndTimeImpl(
    const base::RepeatingCallback<bool(const GURL&)>& url_filter,
    base::Time remove_begin,
    base::Time remove_end) {
  const size_t old_size = compromised_credentials_.size();
  base::EraseIf(compromised_credentials_, [&](const auto& credential) {
    return remove_begin <= credential.create_time &&
           credential.create_time < remove_end &&
           (!url_filter || url_filter.Run(GURL(credential.signon_realm)));
  });

  return old_size != compromised_credentials_.size();
}

void TestPasswordStore::AddFieldInfoImpl(const FieldInfo& field_info) {
  NOTIMPLEMENTED();
}
std::vector<FieldInfo> TestPasswordStore::GetAllFieldInfoImpl() {
  NOTIMPLEMENTED();
  return std::vector<FieldInfo>();
}

void TestPasswordStore::RemoveFieldInfoByTimeImpl(base::Time remove_begin,
                                                  base::Time remove_end) {
  NOTIMPLEMENTED();
}

bool TestPasswordStore::BeginTransaction() {
  return true;
}

void TestPasswordStore::RollbackTransaction() {
  NOTIMPLEMENTED();
}

bool TestPasswordStore::CommitTransaction() {
  return true;
}

FormRetrievalResult TestPasswordStore::ReadAllLogins(
    PrimaryKeyToFormMap* key_to_form_map) {
  if (stored_passwords_.empty()) {
    key_to_form_map->clear();
    return FormRetrievalResult::kSuccess;
  }
  // This currently can't be implemented properly, since TestPasswordStore
  // doesn't have primary keys. Right now no tests actually depend on it, so
  // just leave it not implemented.
  NOTIMPLEMENTED();
  return FormRetrievalResult::kDbError;
}

PasswordStoreChangeList TestPasswordStore::RemoveLoginByPrimaryKeySync(
    int primary_key) {
  NOTIMPLEMENTED();
  return PasswordStoreChangeList();
}

PasswordStoreSync::MetadataStore* TestPasswordStore::GetMetadataStore() {
  return metadata_store_.get();
}

bool TestPasswordStore::IsAccountStore() const {
  return is_account_store_.value();
}

bool TestPasswordStore::DeleteAndRecreateDatabaseFile() {
  stored_passwords_.clear();
  metadata_store_->DeleteAllSyncMetadata();
  return true;
}

}  // namespace password_manager
