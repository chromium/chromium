// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/ui/insecure_credentials_manager.h"

#include <algorithm>
#include <iterator>
#include <set>

#include "base/bind.h"
#include "base/containers/flat_set.h"
#include "base/metrics/histogram_functions.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "components/password_manager/core/browser/compromised_credentials_table.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_list_sorter.h"
#include "components/password_manager/core/browser/ui/credential_utils.h"
#include "components/password_manager/core/browser/ui/saved_passwords_presenter.h"
#include "components/password_manager/core/browser/ui/weak_check_utility.h"
#include "components/password_manager/core/common/password_manager_features.h"

namespace password_manager {

// Extra information about InsecureCredentials which is required by UI.
struct CredentialMetadata {
  std::vector<PasswordForm> forms;
  InsecureCredentialTypeFlags type = InsecureCredentialTypeFlags::kSecure;
  base::Time latest_time;
};

namespace {

using CredentialPasswordsMap =
    std::map<CredentialView, CredentialMetadata, PasswordCredentialLess>;

// Transparent comparator that can compare CompromisedCredentials and
// PasswordForm.
struct CredentialWithoutPasswordLess {
  template <typename T, typename U>
  bool operator()(const T& lhs, const U& rhs) const {
    return CredentialOriginAndUsernameAndStore(lhs) <
           CredentialOriginAndUsernameAndStore(rhs);
  }

  using is_transparent = void;

 private:
  static auto CredentialOriginAndUsernameAndStore(const PasswordForm& form) {
    return std::tie(form.signon_realm, form.username_value, form.in_store);
  }

  static auto CredentialOriginAndUsernameAndStore(
      const CompromisedCredentials& c) {
    return std::tie(c.signon_realm, c.username, c.in_store);
  }
};

InsecureCredentialTypeFlags ConvertCompromiseType(CompromiseType type) {
  switch (type) {
    case CompromiseType::kLeaked:
      return InsecureCredentialTypeFlags::kCredentialLeaked;
    case CompromiseType::kPhished:
      return InsecureCredentialTypeFlags::kCredentialPhished;
  }
  NOTREACHED();
}

// This function takes three lists of compromised credentials, weak passwords
// and saved passwords and joins them, producing a map that contains
// CredentialWithPassword as keys and vector<PasswordForm> as values with
// InsecureCredentialTypeFlags as values.
CredentialPasswordsMap JoinInsecureCredentialsWithSavedPasswords(
    const std::vector<CompromisedCredentials>& compromised_credentials,
    const base::flat_set<base::string16>& weak_passwords,
    SavedPasswordsPresenter::SavedPasswordsView saved_passwords) {
  CredentialPasswordsMap credentials_to_forms;

  bool mark_all_credentials_leaked_for_testing =
      base::GetFieldTrialParamByFeatureAsBool(
          password_manager::features::kPasswordChangeInSettings,
          password_manager::features::
              kPasswordChangeInSettingsWithForcedWarningForEverySite,
          false);
  if (mark_all_credentials_leaked_for_testing) {
    for (const auto& form : saved_passwords) {
      CredentialView compromised_credential(form);
      auto& credential_to_form = credentials_to_forms[compromised_credential];
      credential_to_form.type = InsecureCredentialTypeFlags::kCredentialLeaked;
      credential_to_form.forms.push_back(form);
      credential_to_form.latest_time = form.date_created;
    }
    return credentials_to_forms;
  }

  // Since a single (signon_realm, username) pair might have multiple
  // corresponding entries in saved_passwords, we are using a multiset and doing
  // look-up via equal_range. In most cases the resulting |range| should have a
  // size of 1, however.
  std::multiset<PasswordForm, CredentialWithoutPasswordLess> password_forms(
      saved_passwords.begin(), saved_passwords.end());
  for (const auto& credential : compromised_credentials) {
    auto range = password_forms.equal_range(credential);
    // Make use of a set to only filter out repeated passwords, if any.
    std::for_each(
        range.first, range.second, [&](const PasswordForm& form) {
          CredentialView compromised_credential(form);
          auto& credential_to_form =
              credentials_to_forms[compromised_credential];

          // Using |= operator to save in a bit mask both Leaked and Phished.
          credential_to_form.type |=
              ConvertCompromiseType(credential.compromise_type);

          // Use the latest time. Relevant when the same credential is both
          // phished and compromised.
          credential_to_form.latest_time =
              std::max(credential_to_form.latest_time, credential.create_time);

          // Populate the map. The values are vectors, because it is
          // possible that multiple saved passwords match to the same
          // compromised credential.
          credential_to_form.forms.push_back(form);
        });
  }

  for (const auto& form : saved_passwords) {
    if (weak_passwords.contains(form.password_value)) {
      CredentialView weak_credential(form);
      auto& credential_to_form = credentials_to_forms[weak_credential];
      credential_to_form.type |= InsecureCredentialTypeFlags::kWeakCredential;

      // This helps not to create a copy of the |form| in case the credential
      // has also been compromised. This is important because we don't want to
      // delete the form twice in the RemoveCredential.
      if (!IsCompromised(credential_to_form.type)) {
        credential_to_form.forms.push_back(form);
      }
    }
  }

  return credentials_to_forms;
}

std::vector<CredentialWithPassword> ExtractInsecureCredentials(
    const CredentialPasswordsMap& credentials_to_forms,
    bool (*condition)(const InsecureCredentialTypeFlags&)) {
  std::vector<CredentialWithPassword> credentials;
  for (const auto& credential_to_forms : credentials_to_forms) {
    if (condition(credential_to_forms.second.type)) {
      CredentialWithPassword credential(credential_to_forms.first);
      credential.insecure_type = credential_to_forms.second.type;
      credential.create_time = credential_to_forms.second.latest_time;
      credentials.push_back(std::move(credential));
    }
  }
  return credentials;
}

base::flat_set<base::string16> ExtractPasswords(
    SavedPasswordsPresenter::SavedPasswordsView password_forms) {
  std::vector<base::string16> passwords;
  passwords.reserve(password_forms.size());
  for (const auto& form : password_forms) {
    passwords.push_back(form.password_value);
  }
  return base::flat_set<base::string16>(std::move(passwords));
}

}  // namespace

CredentialView::CredentialView(std::string signon_realm,
                               GURL url,
                               base::string16 username,
                               base::string16 password)
    : signon_realm(std::move(signon_realm)),
      url(std::move(url)),
      username(std::move(username)),
      password(std::move(password)) {}

CredentialView::CredentialView(const PasswordForm& form)
    : signon_realm(form.signon_realm),
      url(form.url),
      username(form.username_value),
      password(form.password_value) {}

CredentialView::CredentialView(const CredentialView& credential) = default;
CredentialView::CredentialView(CredentialView&& credential) = default;
CredentialView& CredentialView::operator=(const CredentialView& credential) =
    default;
CredentialView& CredentialView::operator=(CredentialView&& credential) =
    default;
CredentialView::~CredentialView() = default;

CredentialWithPassword::CredentialWithPassword(const CredentialView& credential)
    : CredentialView(credential) {}
CredentialWithPassword::~CredentialWithPassword() = default;
CredentialWithPassword::CredentialWithPassword(
    const CredentialWithPassword& other) = default;

CredentialWithPassword::CredentialWithPassword(CredentialWithPassword&& other) =
    default;
CredentialWithPassword::CredentialWithPassword(
    const CompromisedCredentials& credential)
    : CredentialView(credential.signon_realm,
                     GURL(credential.signon_realm),
                     credential.username,
                     /*password=*/{}),
      create_time(credential.create_time),
      insecure_type(ConvertCompromiseType(credential.compromise_type)) {}

CredentialWithPassword& CredentialWithPassword::operator=(
    const CredentialWithPassword& other) = default;
CredentialWithPassword& CredentialWithPassword::operator=(
    CredentialWithPassword&& other) = default;

InsecureCredentialsManager::InsecureCredentialsManager(
    SavedPasswordsPresenter* presenter,
    scoped_refptr<PasswordStore> profile_store,
    scoped_refptr<PasswordStore> account_store)
    : presenter_(presenter),
      profile_store_(std::move(profile_store)),
      account_store_(std::move(account_store)),
      compromised_credentials_reader_(profile_store_.get(),
                                      account_store_.get()) {
  observed_compromised_credentials_reader_.Add(
      &compromised_credentials_reader_);
  observed_saved_password_presenter_.Add(presenter_);
}

InsecureCredentialsManager::~InsecureCredentialsManager() = default;

void InsecureCredentialsManager::Init() {
  compromised_credentials_reader_.Init();
}

void InsecureCredentialsManager::StartWeakCheck() {
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
      base::BindOnce(&BulkWeakCheck,
                     ExtractPasswords(presenter_->GetSavedPasswords())),
      base::BindOnce(&InsecureCredentialsManager::OnWeakCheckDone,
                     weak_ptr_factory_.GetWeakPtr(), base::ElapsedTimer()));
}

void InsecureCredentialsManager::SaveCompromisedCredential(
    const LeakCheckCredential& credential) {
  // Iterate over all currently saved credentials and mark those as compromised
  // that have the same canonicalized username and password.
  const base::string16 canonicalized_username =
      CanonicalizeUsername(credential.username());
  for (const PasswordForm& saved_password : presenter_->GetSavedPasswords()) {
    if (saved_password.password_value == credential.password() &&
        CanonicalizeUsername(saved_password.username_value) ==
            canonicalized_username) {
      GetStoreFor(saved_password)
          .AddCompromisedCredentials({
              .signon_realm = saved_password.signon_realm,
              .username = saved_password.username_value,
              .create_time = base::Time::Now(),
              .compromise_type = CompromiseType::kLeaked,
          });
    }
  }
}

bool InsecureCredentialsManager::UpdateCredential(
    const CredentialView& credential,
    const base::StringPiece password) {
  auto it = credentials_to_forms_.find(credential);
  if (it == credentials_to_forms_.end())
    return false;

  // Make sure there are matching password forms. Also erase duplicates if there
  // are any.
  const auto& forms = it->second.forms;
  if (forms.empty())
    return false;

  for (size_t i = 1; i < forms.size(); ++i)
    GetStoreFor(forms[i]).RemoveLogin(forms[i]);

  // Note: We Invoke EditPassword on the presenter rather than UpdateLogin() on
  // the store, so that observers of the presenter get notified of this event.
  return presenter_->EditPassword(forms[0], base::UTF8ToUTF16(password));
}

bool InsecureCredentialsManager::RemoveCredential(
    const CredentialView& credential) {
  auto it = credentials_to_forms_.find(credential);
  if (it == credentials_to_forms_.end())
    return false;

  // Erase all matching credentials from the store. Return whether any
  // credentials were deleted.
  const auto& saved_passwords = it->second.forms;
  for (const PasswordForm& saved_password : saved_passwords)
    GetStoreFor(saved_password).RemoveLogin(saved_password);

  return !saved_passwords.empty();
}

std::vector<CredentialWithPassword>
InsecureCredentialsManager::GetCompromisedCredentials() const {
  return ExtractInsecureCredentials(credentials_to_forms_, &IsCompromised);
}

std::vector<CredentialWithPassword>
InsecureCredentialsManager::GetWeakCredentials() const {
  std::vector<CredentialWithPassword> weak_credentials =
      ExtractInsecureCredentials(credentials_to_forms_, &IsWeak);

  auto get_sort_key = [this](const CredentialWithPassword& credential) {
    return CreateSortKey(GetSavedPasswordsFor(credential)[0],
                         IgnoreStore(true));
  };
  base::ranges::sort(weak_credentials, {}, get_sort_key);
  return weak_credentials;
}

SavedPasswordsPresenter::SavedPasswordsView
InsecureCredentialsManager::GetSavedPasswordsFor(
    const CredentialView& credential) const {
  auto it = credentials_to_forms_.find(credential);
  return it != credentials_to_forms_.end()
             ? it->second.forms
             : SavedPasswordsPresenter::SavedPasswordsView();
}

void InsecureCredentialsManager::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void InsecureCredentialsManager::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void InsecureCredentialsManager::OnWeakCheckDone(
    base::ElapsedTimer timer_since_weak_check_start,
    base::flat_set<base::string16> weak_passwords) {
  weak_passwords_ = std::move(weak_passwords);

  credentials_to_forms_ = JoinInsecureCredentialsWithSavedPasswords(
      compromised_credentials_, weak_passwords_,
      presenter_->GetSavedPasswords());
  base::UmaHistogramTimes("PasswordManager.WeakCheck.Time",
                          timer_since_weak_check_start.Elapsed());
  NotifyWeakCredentialsChanged();
}

// Re-computes the list of compromised credentials with passwords after
// obtaining a new list of compromised credentials.
void InsecureCredentialsManager::OnCompromisedCredentialsChanged(
    const std::vector<CompromisedCredentials>& compromised_credentials) {
  compromised_credentials_ = compromised_credentials;

  credentials_to_forms_ = JoinInsecureCredentialsWithSavedPasswords(
      compromised_credentials_, weak_passwords_,
      presenter_->GetSavedPasswords());
  NotifyCompromisedCredentialsChanged();
}

// Re-computes the list of insecure credentials with passwords after obtaining a
// new list of saved passwords.
void InsecureCredentialsManager::OnSavedPasswordsChanged(
    SavedPasswordsPresenter::SavedPasswordsView saved_passwords) {
  credentials_to_forms_ = JoinInsecureCredentialsWithSavedPasswords(
      compromised_credentials_, weak_passwords_, saved_passwords);
  NotifyCompromisedCredentialsChanged();
  NotifyWeakCredentialsChanged();
}

void InsecureCredentialsManager::NotifyCompromisedCredentialsChanged() {
  std::vector<CredentialWithPassword> compromised_credentials =
      ExtractInsecureCredentials(credentials_to_forms_, &IsCompromised);
  for (auto& observer : observers_) {
    observer.OnCompromisedCredentialsChanged(compromised_credentials);
  }
}

void InsecureCredentialsManager::NotifyWeakCredentialsChanged() {
  for (auto& observer : observers_) {
    observer.OnWeakCredentialsChanged();
  }
}

PasswordStore& InsecureCredentialsManager::GetStoreFor(
    const PasswordForm& form) {
  return form.IsUsingAccountStore() ? *account_store_ : *profile_store_;
}

}  // namespace password_manager
