// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/core/browser/signin_error_controller.h"

#include "components/signin/core/browser/signin_metrics.h"

namespace {

typedef std::set<const SigninErrorController::AuthStatusProvider*>
    AuthStatusProviderSet;

}  // namespace

SigninErrorController::AuthStatusProvider::AuthStatusProvider() {
}

SigninErrorController::AuthStatusProvider::~AuthStatusProvider() {
}

SigninErrorController::SigninErrorController(AccountMode mode)
    : account_mode_(mode),
      auth_error_(GoogleServiceAuthError::AuthErrorNone()) {}

SigninErrorController::~SigninErrorController() {
  DCHECK(provider_set_.empty())
      << "All AuthStatusProviders should be unregistered before "
      << "SigninErrorController is destroyed";
}

void SigninErrorController::AddProvider(const AuthStatusProvider* provider) {
  DCHECK(provider_set_.find(provider) == provider_set_.end())
      << "Adding same AuthStatusProvider multiple times";
  provider_set_.insert(provider);
  AuthStatusChanged();
}

void SigninErrorController::RemoveProvider(const AuthStatusProvider* provider) {
  auto iter = provider_set_.find(provider);
  DCHECK(iter != provider_set_.end())
      << "Removing provider that was never added";
  provider_set_.erase(iter);
  AuthStatusChanged();
}

void SigninErrorController::AuthStatusChanged() {
  GoogleServiceAuthError::State prev_state = auth_error_.state();
  std::string prev_account_id = error_account_id_;
  bool error_changed = false;

  // Find an error among the status providers. If |auth_error_| has an
  // actionable error state and some provider exposes a similar error and
  // account id, use that error. Otherwise, just take the first actionable
  // error we find.
  for (auto it = provider_set_.begin(); it != provider_set_.end(); ++it) {
    std::string account_id = (*it)->GetAccountId();

    // In PRIMARY_ACCOUNT mode, ignore all secondary accounts.
    if (account_mode_ == AccountMode::PRIMARY_ACCOUNT &&
        (account_id != primary_account_id_)) {
      continue;
    }

    GoogleServiceAuthError error = (*it)->GetAuthStatus();

    // Ignore the states we don't want to elevate to the user.
    if (error.state() == GoogleServiceAuthError::NONE ||
        error.IsTransientError()) {
      continue;
    }

    // Prioritize this error if it matches the previous |auth_error_|.
    if (error.state() == prev_state && account_id == prev_account_id) {
      auth_error_ = error;
      error_account_id_ = account_id;
      error_changed = true;
      break;
    }

    // Use this error if we haven't found one already, but keep looking for the
    // previous |auth_error_| in case there's a match elsewhere in the set.
    if (!error_changed) {
      auth_error_ = error;
      error_account_id_ = account_id;
      error_changed = true;
    }
  }

  if (!error_changed && prev_state != GoogleServiceAuthError::NONE) {
    // No provider reported an error, so clear the error we have now.
    auth_error_ = GoogleServiceAuthError::AuthErrorNone();
    error_account_id_.clear();
    error_changed = true;
  }

  if (error_changed) {
    signin_metrics::LogAuthError(auth_error_);
    for (auto& observer : observer_list_)
      observer.OnErrorChanged();
  }
}

bool SigninErrorController::HasError() const {
  return auth_error_.state() != GoogleServiceAuthError::NONE &&
      auth_error_.state() != GoogleServiceAuthError::CONNECTION_FAILED;
}

void SigninErrorController::SetPrimaryAccountID(const std::string& account_id) {
  primary_account_id_ = account_id;
  if (account_mode_ == AccountMode::PRIMARY_ACCOUNT)
    AuthStatusChanged();  // Recompute the error state.
}

void SigninErrorController::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void SigninErrorController::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}
