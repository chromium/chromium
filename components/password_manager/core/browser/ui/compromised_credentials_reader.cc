// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/ui/compromised_credentials_reader.h"

#include <iterator>

#include "base/ranges/algorithm.h"
#include "base/stl_util.h"
#include "components/password_manager/core/browser/password_form.h"

namespace password_manager {
CompromisedCredentialsReader::CompromisedCredentialsReader(
    PasswordStore* profile_store,
    PasswordStore* account_store)
    : profile_store_(profile_store), account_store_(account_store) {
  DCHECK(profile_store_);
  observed_password_store_.Add(profile_store_);
  if (account_store_) {
    observed_password_store_.Add(account_store_);
  } else {
    // Since we aren't expecting any response from the account store, mark it as
    // responded not to block responses from the the profile waiting for the
    // account store to respond.
    account_store_responded_ = true;
  }
}

CompromisedCredentialsReader::~CompromisedCredentialsReader() = default;

void CompromisedCredentialsReader::Init() {
  profile_store_->GetAllCompromisedCredentials(this);
  if (account_store_)
    account_store_->GetAllCompromisedCredentials(this);
}

void CompromisedCredentialsReader::OnCompromisedCredentialsChanged() {
  // This class overrides OnCompromisedCredentialsChangedIn() (the version of
  // this method that also receives the originating store), so the store-less
  // version never gets called.
  NOTREACHED();
}

void CompromisedCredentialsReader::OnCompromisedCredentialsChangedIn(
    PasswordStore* store) {
  store->GetAllCompromisedCredentials(this);
}

void CompromisedCredentialsReader::OnGetCompromisedCredentials(
    std::vector<CompromisedCredentials> compromised_credentials) {
  // This class overrides OnGetCompromisedCredentialFrom() (the version of this
  // method that also receives the originating store), so the store-less version
  // never gets called.
  NOTREACHED();
}

void CompromisedCredentialsReader::OnGetCompromisedCredentialsFrom(
    PasswordStore* store,
    std::vector<CompromisedCredentials> compromised_credentials) {
  profile_store_responded_ |= store == profile_store_;
  account_store_responded_ |= store == account_store_;
  // Remove all previously cached credentials from `store` and then insert
  // the just received `compromised_credentials`.
  PasswordForm::Store to_remove = store == profile_store_
                                      ? PasswordForm::Store::kProfileStore
                                      : PasswordForm::Store::kAccountStore;

  base::EraseIf(compromised_credentials_, [to_remove](const auto& credential) {
    return credential.in_store == to_remove;
  });

  base::ranges::move(compromised_credentials,
                     std::back_inserter(compromised_credentials_));

  // Observers are reptitively notified of compromised credentials, and hence
  // vbservers can expect partial view of the compromised credentials, so inform
  // the observers directly.
  for (auto& observer : observers_)
    observer.OnCompromisedCredentialsChanged(compromised_credentials_);

  // For the callbacks waiting for the results of
  // `GetAllCompromisedCredentials()`, they should be notified only when both
  // stores responded.
  if (!profile_store_responded_ || !account_store_responded_)
    return;

  for (auto& callback :
       std::exchange(get_all_compromised_credentials_callbacks_, {})) {
    std::move(callback).Run(compromised_credentials_);
  }
}

void CompromisedCredentialsReader::GetAllCompromisedCredentials(
    GetCompromisedCredentialsCallback cb) {
  if (profile_store_responded_ && account_store_responded_) {
    std::move(cb).Run(compromised_credentials_);
    return;
  }
  // Add the callback *before* triggering any of the fetches. This ensures
  // that we don't miss a notitication if the fetches return synchronously
  // (which is the case in tests).
  get_all_compromised_credentials_callbacks_.push_back(std::move(cb));

  if (!profile_store_responded_)
    profile_store_->GetAllCompromisedCredentials(this);
  if (!account_store_responded_) {
    DCHECK(account_store_);
    account_store_->GetAllCompromisedCredentials(this);
  }
}

void CompromisedCredentialsReader::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void CompromisedCredentialsReader::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

}  // namespace password_manager
