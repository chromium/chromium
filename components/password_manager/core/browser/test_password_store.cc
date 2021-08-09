// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/test_password_store.h"

#include <stddef.h>

#include <memory>

#include "base/check_op.h"
#include "base/containers/cxx20_erase.h"
#include "base/notreached.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "components/password_manager/core/browser/login_database.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/psl_matching_helper.h"
#include "components/password_manager/core/browser/statistics_table.h"
#include "components/sync/model/model_type_controller_delegate.h"
#include "components/sync/model/proxy_model_type_controller_delegate.h"
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
      metadata_store_(std::make_unique<TestPasswordSyncMetadataStore>()) {
  backend_ = this;
}

const TestPasswordStore::PasswordMap& TestPasswordStore::stored_passwords()
    const {
  return stored_passwords_;
}

void TestPasswordStore::Clear() {
  stored_passwords_.clear();
}

bool TestPasswordStore::IsEmpty() const {
  // The store is empty, if the sum of all stored passwords across all entries
  // in |stored_passwords_| is 0.
  size_t number_of_passwords = 0u;
  for (auto it = stored_passwords_.begin();
       !number_of_passwords && it != stored_passwords_.end(); ++it) {
    number_of_passwords += it->second.size();
  }
  return number_of_passwords == 0u;
}

TestPasswordStore::~TestPasswordStore() = default;

scoped_refptr<base::SequencedTaskRunner>
TestPasswordStore::CreateBackgroundTaskRunner() const {
  return base::SequencedTaskRunnerHandle::Get();
}

void TestPasswordStore::InitBackend(
    RemoteChangesReceived remote_form_changes_received,
    base::RepeatingClosure sync_enabled_or_disabled_cb,
    base::OnceCallback<void(bool)> completion) {
  main_task_runner()->PostTask(FROM_HERE,
                               base::BindOnce(std::move(completion), true));
}

void TestPasswordStore::GetAllLoginsAsync(LoginsReply callback) {
  background_task_runner()->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&TestPasswordStore::GetAllLoginsInternal,
                     RetainedRef(this)),
      std::move(callback));
}

void TestPasswordStore::GetAutofillableLoginsAsync(LoginsReply callback) {
  background_task_runner()->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&TestPasswordStore::GetAutofillableLoginsInternal,
                     RetainedRef(this)),
      std::move(callback));
}

void TestPasswordStore::FillMatchingLoginsAsync(
    LoginsReply callback,
    bool include_psl,
    const std::vector<PasswordFormDigest>& forms) {
  background_task_runner()->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&TestPasswordStore::FillMatchingLoginsBulk,
                     base::Unretained(this), forms, include_psl),
      std::move(callback));
}

void TestPasswordStore::AddLoginAsync(const PasswordForm& form,
                                      PasswordStoreChangeListReply callback) {
  background_task_runner()->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&TestPasswordStore::AddLoginImpl, base::Unretained(this),
                     form),
      std::move(callback));
}

void TestPasswordStore::UpdateLoginAsync(
    const PasswordForm& form,
    PasswordStoreChangeListReply callback) {
  background_task_runner()->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&TestPasswordStore::UpdateLoginImpl,
                     base::Unretained(this), form),
      std::move(callback));
}

void TestPasswordStore::RemoveLoginAsync(
    const PasswordForm& form,
    PasswordStoreChangeListReply callback) {
  background_task_runner()->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&TestPasswordStore::RemoveLoginImpl,
                     base::Unretained(this), form),
      std::move(callback));
}

void TestPasswordStore::RemoveLoginsCreatedBetweenAsync(
    base::Time delete_begin,
    base::Time delete_end,
    PasswordStoreChangeListReply callback) {
  NOTIMPLEMENTED();
}

void TestPasswordStore::RemoveLoginsByURLAndTimeAsync(
    const base::RepeatingCallback<bool(const GURL&)>& url_filter,
    base::Time delete_begin,
    base::Time delete_end,
    base::OnceCallback<void(bool)> sync_completion,
    PasswordStoreChangeListReply callback) {
  NOTIMPLEMENTED();
}

void TestPasswordStore::DisableAutoSignInForOriginsAsync(
    const base::RepeatingCallback<bool(const GURL&)>& origin_filter,
    base::OnceClosure completion) {
  NOTIMPLEMENTED();
}

SmartBubbleStatsStore* TestPasswordStore::GetSmartBubbleStatsStore() {
  return nullptr;
}

FieldInfoStore* TestPasswordStore::GetFieldInfoStore() {
  return nullptr;
}

std::unique_ptr<syncer::ProxyModelTypeControllerDelegate>
TestPasswordStore::CreateSyncControllerDelegateFactory() {
  NOTIMPLEMENTED();
  return nullptr;
}

void TestPasswordStore::ReportMetricsImpl(const std::string& sync_username,
                                          bool custom_passphrase_sync_enabled,
                                          BulkCheckDone bulk_check_done) {
  NOTIMPLEMENTED();
}

bool TestPasswordStore::IsAccountStore() const {
  return is_account_store_.value();
}

LoginsResult TestPasswordStore::GetAllLoginsInternal() {
  LoginsResult forms;
  for (const auto& elements : stored_passwords_) {
    for (const auto& password_form : elements.second) {
      forms.push_back(std::make_unique<PasswordForm>(password_form));
    }
  }
  return forms;
}

LoginsResult TestPasswordStore::GetAutofillableLoginsInternal() {
  LoginsResult forms;
  for (const auto& forms_for_realm : stored_passwords_) {
    for (const PasswordForm& form : forms_for_realm.second) {
      if (!form.blocked_by_user)
        forms.push_back(std::make_unique<PasswordForm>(form));
    }
  }
  return forms;
}

LoginsResult TestPasswordStore::FillMatchingLogins(
    const PasswordFormDigest& form,
    bool include_psl) {
  ++fill_matching_logins_calls_;
  std::vector<std::unique_ptr<PasswordForm>> matched_forms;
  for (const auto& elements : stored_passwords_) {
    // The code below doesn't support PSL federated credential. It's doable but
    // no tests need it so far.
    const bool realm_matches = elements.first == form.signon_realm;
    const bool realm_psl_matches =
        IsPublicSuffixDomainMatch(elements.first, form.signon_realm);
    if (realm_matches || (realm_psl_matches && include_psl) ||
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

LoginsResult TestPasswordStore::FillMatchingLoginsBulk(
    const std::vector<PasswordFormDigest>& forms,
    bool include_psl) {
  std::vector<std::unique_ptr<PasswordForm>> results;
  for (const auto& form : forms) {
    std::vector<std::unique_ptr<PasswordForm>> matched_forms =
        FillMatchingLogins(form, include_psl);
    results.insert(results.end(),
                   std::make_move_iterator(matched_forms.begin()),
                   std::make_move_iterator(matched_forms.end()));
  }
  return results;
}

PasswordStoreChangeList TestPasswordStore::AddLoginImpl(
    const PasswordForm& form) {
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
    const PasswordForm& form) {
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

}  // namespace password_manager
