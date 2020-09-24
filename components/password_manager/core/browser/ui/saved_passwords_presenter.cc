// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/ui/saved_passwords_presenter.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "base/check.h"
#include "base/ranges/algorithm.h"
#include "base/stl_util.h"
#include "base/strings/string16.h"
#include "components/password_manager/core/browser/password_form.h"

namespace password_manager {

SavedPasswordsPresenter::SavedPasswordsPresenter(
    scoped_refptr<PasswordStore> profile_store,
    scoped_refptr<PasswordStore> account_store)
    : profile_store_(std::move(profile_store)),
      account_store_(std::move(account_store)) {
  DCHECK(profile_store_);
  profile_store_->AddObserver(this);
  if (account_store_)
    account_store_->AddObserver(this);
}

SavedPasswordsPresenter::~SavedPasswordsPresenter() {
  if (account_store_)
    account_store_->RemoveObserver(this);
  profile_store_->RemoveObserver(this);
}

void SavedPasswordsPresenter::Init() {
  profile_store_->GetAllLoginsWithAffiliationAndBrandingInformation(this);
  if (account_store_)
    account_store_->GetAllLoginsWithAffiliationAndBrandingInformation(this);
}

bool SavedPasswordsPresenter::EditPassword(const PasswordForm& form,
                                           base::string16 new_password) {
  auto found = base::ranges::find(passwords_, form);
  if (found == passwords_.end())
    return false;

  found->password_value = std::move(new_password);
  PasswordStore& store =
      form.IsUsingAccountStore() ? *account_store_ : *profile_store_;
  store.UpdateLogin(*found);
  NotifyEdited(*found);
  return true;
}

SavedPasswordsPresenter::SavedPasswordsView
SavedPasswordsPresenter::GetSavedPasswords() const {
  return passwords_;
}

void SavedPasswordsPresenter::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void SavedPasswordsPresenter::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void SavedPasswordsPresenter::NotifyEdited(const PasswordForm& password) {
  for (auto& observer : observers_)
    observer.OnEdited(password);
}

void SavedPasswordsPresenter::NotifySavedPasswordsChanged() {
  for (auto& observer : observers_)
    observer.OnSavedPasswordsChanged(passwords_);
}

void SavedPasswordsPresenter::OnLoginsChanged(
    const PasswordStoreChangeList& changes) {
  // This class overrides OnLoginsChangedIn() (the version of this
  // method that also receives the originating store), so the store-less version
  // never gets called.
  NOTREACHED();
}

void SavedPasswordsPresenter::OnLoginsChangedIn(
    PasswordStore* store,
    const PasswordStoreChangeList& changes) {
  store->GetAllLoginsWithAffiliationAndBrandingInformation(this);
}

void SavedPasswordsPresenter::OnGetPasswordStoreResults(
    std::vector<std::unique_ptr<PasswordForm>> results) {
  // This class overrides OnGetPasswordStoreResultsFrom() (the version of this
  // method that also receives the originating store), so the store-less version
  // never gets called.
  NOTREACHED();
}

void SavedPasswordsPresenter::OnGetPasswordStoreResultsFrom(
    PasswordStore* store,
    std::vector<std::unique_ptr<PasswordForm>> results) {
  // Ignore blocked or federated credentials.
  base::EraseIf(results, [](const auto& form) {
    return form->blocked_by_user || form->IsFederatedCredential();
  });

  // Profile store passwords are always stored first in `passwords_`.
  auto account_passwords_it = base::ranges::partition_point(
      passwords_,
      [](auto& password) { return !password.IsUsingAccountStore(); });
  if (store == profile_store_) {
    // Old profile store passwords are in front. Create a temporary buffer for
    // the new passwords and replace existing passwords.
    std::vector<PasswordForm> new_passwords;
    new_passwords.reserve(results.size() + passwords_.end() -
                          account_passwords_it);
    auto new_passwords_back_inserter = std::back_inserter(new_passwords);
    base::ranges::transform(results, new_passwords_back_inserter,
                            [](auto& result) { return std::move(*result); });
    std::move(account_passwords_it, passwords_.end(),
              new_passwords_back_inserter);
    passwords_ = std::move(new_passwords);
  } else {
    // Need to replace existing account passwords at the end. Can re-use
    // existing `passwords_` vector.
    passwords_.erase(account_passwords_it, passwords_.end());
    if (passwords_.capacity() < passwords_.size() + results.size())
      passwords_.reserve(passwords_.size() + results.size());
    base::ranges::transform(results, std::back_inserter(passwords_),
                            [](auto& result) { return std::move(*result); });
  }
  NotifySavedPasswordsChanged();
}

}  // namespace password_manager
