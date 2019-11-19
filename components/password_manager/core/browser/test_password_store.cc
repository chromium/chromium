// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/test_password_store.h"

#include <stddef.h>

#include <memory>

#include "base/logging.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "components/autofill/core/common/password_form.h"
#include "components/password_manager/core/browser/login_database.h"
#include "components/password_manager/core/browser/psl_matching_helper.h"
#include "components/password_manager/core/browser/statistics_table.h"
#include "url/gurl.h"

namespace password_manager {

TestPasswordStore::TestPasswordStore() = default;

TestPasswordStore::~TestPasswordStore() = default;

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

scoped_refptr<base::SequencedTaskRunner>
TestPasswordStore::CreateBackgroundTaskRunner() const {
  return base::SequencedTaskRunnerHandle::Get();
}

PasswordStoreChangeList TestPasswordStore::AddLoginImpl(
    const autofill::PasswordForm& form,
    AddLoginError* error) {
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
    return changes;
  }

  changes.emplace_back(PasswordStoreChange::ADD, form);
  passwords_for_signon_realm.push_back(form);
  return changes;
}

PasswordStoreChangeList TestPasswordStore::UpdateLoginImpl(
    const autofill::PasswordForm& form,
    UpdateLoginError* error) {
  PasswordStoreChangeList changes;
  std::vector<autofill::PasswordForm>& forms =
      stored_passwords_[form.signon_realm];
  for (auto it = forms.begin(); it != forms.end(); ++it) {
    if (ArePasswordFormUniqueKeysEqual(form, *it)) {
      *it = form;
      changes.push_back(PasswordStoreChange(PasswordStoreChange::UPDATE, form));
    }
  }
  return changes;
}

PasswordStoreChangeList TestPasswordStore::RemoveLoginImpl(
    const autofill::PasswordForm& form) {
  PasswordStoreChangeList changes;
  std::vector<autofill::PasswordForm>& forms =
      stored_passwords_[form.signon_realm];
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

std::vector<std::unique_ptr<autofill::PasswordForm>>
TestPasswordStore::FillMatchingLogins(const FormDigest& form) {
  ++fill_matching_logins_calls_;
  std::vector<std::unique_ptr<autofill::PasswordForm>> matched_forms;
  for (const auto& elements : stored_passwords_) {
    // The code below doesn't support PSL federated credential. It's doable but
    // no tests need it so far.
    const bool realm_matches = elements.first == form.signon_realm;
    const bool realm_psl_matches =
        IsPublicSuffixDomainMatch(elements.first, form.signon_realm);
    if (realm_matches || realm_psl_matches ||
        (form.scheme == autofill::PasswordForm::Scheme::kHtml &&
         password_manager::IsFederatedRealm(elements.first, form.origin))) {
      const bool is_psl = !realm_matches && realm_psl_matches;
      for (const auto& stored_form : elements.second) {
        // Repeat the condition above with an additional check for origin.
        if (realm_matches || realm_psl_matches ||
            (form.scheme == autofill::PasswordForm::Scheme::kHtml &&
             stored_form.origin.GetOrigin() == form.origin.GetOrigin() &&
             password_manager::IsFederatedRealm(stored_form.signon_realm,
                                                form.origin))) {
          matched_forms.push_back(
              std::make_unique<autofill::PasswordForm>(stored_form));
          matched_forms.back()->is_public_suffix_match = is_psl;
        }
      }
    }
  }
  return matched_forms;
}

std::vector<std::unique_ptr<autofill::PasswordForm>>
TestPasswordStore::FillMatchingLoginsByPassword(
    const base::string16& plain_text_password) {
  std::vector<std::unique_ptr<autofill::PasswordForm>> matched_forms;
  for (const auto& elements : stored_passwords_) {
    for (const auto& password_form : elements.second) {
      if (password_form.password_value == plain_text_password)
        matched_forms.push_back(
            std::make_unique<autofill::PasswordForm>(password_form));
    }
  }
  return matched_forms;
}

bool TestPasswordStore::FillAutofillableLogins(
    std::vector<std::unique_ptr<autofill::PasswordForm>>* forms) {
  for (const auto& forms_for_realm : stored_passwords_) {
    for (const autofill::PasswordForm& form : forms_for_realm.second) {
      if (!form.blacklisted_by_user)
        forms->push_back(std::make_unique<autofill::PasswordForm>(form));
    }
  }
  return true;
}

bool TestPasswordStore::FillBlacklistLogins(
    std::vector<std::unique_ptr<autofill::PasswordForm>>* forms) {
  for (const auto& forms_for_realm : stored_passwords_) {
    for (const autofill::PasswordForm& form : forms_for_realm.second) {
      if (form.blacklisted_by_user)
        forms->push_back(std::make_unique<autofill::PasswordForm>(form));
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
                                          bool custom_passphrase_sync_enabled) {
  NOTIMPLEMENTED();
}

PasswordStoreChangeList TestPasswordStore::RemoveLoginsByURLAndTimeImpl(
    const base::Callback<bool(const GURL&)>& url_filter,
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
    const base::Callback<bool(const GURL&)>& origin_filter) {
  NOTIMPLEMENTED();
  return PasswordStoreChangeList();
}

bool TestPasswordStore::RemoveStatisticsByOriginAndTimeImpl(
    const base::Callback<bool(const GURL&)>& origin_filter,
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

void TestPasswordStore::AddCompromisedCredentialsImpl(
    const CompromisedCredentials& stats) {
  NOTIMPLEMENTED();
}

void TestPasswordStore::RemoveCompromisedCredentialsImpl(
    const GURL& url,
    const base::string16& username) {
  NOTIMPLEMENTED();
}

std::vector<CompromisedCredentials>
TestPasswordStore::GetAllCompromisedCredentialsImpl() {
  NOTIMPLEMENTED();
  return std::vector<CompromisedCredentials>();
}

void TestPasswordStore::RemoveCompromisedCredentialsByUrlAndTimeImpl(
    const base::RepeatingCallback<bool(const GURL&)>& url_filter,
    base::Time remove_begin,
    base::Time remove_end) {
  NOTIMPLEMENTED();
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

void TestPasswordStore::RollbackTransaction() {}

bool TestPasswordStore::CommitTransaction() {
  return true;
}

FormRetrievalResult TestPasswordStore::ReadAllLogins(
    PrimaryKeyToFormMap* key_to_form_map) {
  NOTIMPLEMENTED();
  return FormRetrievalResult::kSuccess;
}

PasswordStoreChangeList TestPasswordStore::RemoveLoginByPrimaryKeySync(
    int primary_key) {
  NOTIMPLEMENTED();
  return PasswordStoreChangeList();
}

PasswordStoreSync::MetadataStore* TestPasswordStore::GetMetadataStore() {
  NOTIMPLEMENTED();
  return nullptr;
}

bool TestPasswordStore::IsAccountStore() const {
  return false;
}

bool TestPasswordStore::DeleteAndRecreateDatabaseFile() {
  NOTIMPLEMENTED();
  return false;
}

}  // namespace password_manager
