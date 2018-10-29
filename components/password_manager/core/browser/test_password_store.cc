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
    const autofill::PasswordForm& form) {
  PasswordStoreChangeList changes;
  stored_passwords_[form.signon_realm].push_back(form);
  changes.push_back(PasswordStoreChange(PasswordStoreChange::ADD, form));
  return changes;
}

PasswordStoreChangeList TestPasswordStore::UpdateLoginImpl(
    const autofill::PasswordForm& form) {
  PasswordStoreChangeList changes;
  std::vector<autofill::PasswordForm>& forms =
      stored_passwords_[form.signon_realm];
  for (auto it = forms.begin(); it != forms.end(); ++it) {
    if (ArePasswordFormUniqueKeyEqual(form, *it)) {
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
    if (ArePasswordFormUniqueKeyEqual(form, *it)) {
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
        (form.scheme == autofill::PasswordForm::SCHEME_HTML &&
         password_manager::IsFederatedRealm(elements.first, form.origin))) {
      const bool is_psl = !realm_matches && realm_psl_matches;
      for (const auto& stored_form : elements.second) {
        // Repeat the condition above with an additional check for origin.
        if (realm_matches || realm_psl_matches ||
            (form.scheme == autofill::PasswordForm::SCHEME_HTML &&
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

std::vector<std::unique_ptr<autofill::PasswordForm>>
TestPasswordStore::FillLoginsForSameOrganizationName(
    const std::string& signon_realm) {
  // Note: To keep TestPasswordStore simple, and because no tests currently
  // require anything more complex, this is a simplistic implementation which
  // assumes that that the signon_realm is a serialised URL.
  return FillMatchingLogins(FormDigest(autofill::PasswordForm::SCHEME_HTML,
                                       signon_realm, GURL(signon_realm)));
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

PasswordStoreChangeList TestPasswordStore::RemoveLoginsSyncedBetweenImpl(
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

}  // namespace password_manager
