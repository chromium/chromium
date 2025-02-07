// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/public/browser/web_signin_tracker.h"

#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/accounts_in_cookie_jar_info.h"

namespace signin {

WebSigninTracker::WebSigninTracker(
    IdentityManager* identity_manager,
    AccountReconcilor* account_reconcilor,
    CoreAccountId signin_account,
    base::OnceCallback<void(WebSigninTracker::Result)> callback)
    : identity_manager_(identity_manager),
      account_reconcilor_(account_reconcilor),
      signin_account_(std::move(signin_account)),
      callback_(std::move(callback)) {
  CHECK(callback_);

  identity_manager_->AddObserver(this);
  account_reconcilor_->AddObserver(this);

  signin::AccountsInCookieJarInfo info =
      identity_manager_->GetAccountsInCookieJar();
  if (info.AreAccountsFresh()) {
    // Check whether the target primary account is already in cookies.
    OnAccountsInCookieUpdated(info, GoogleServiceAuthError::AuthErrorNone());
  }
}

WebSigninTracker::~WebSigninTracker() {
  identity_manager_->RemoveObserver(this);
  account_reconcilor_->RemoveObserver(this);
}

void WebSigninTracker::OnAccountsInCookieUpdated(
    const signin::AccountsInCookieJarInfo& accounts_in_cookie_jar_info,
    const GoogleServiceAuthError& error) {
  for (const auto& account :
       accounts_in_cookie_jar_info.GetPotentiallyInvalidSignedInAccounts()) {
    if (account.valid && account.id == signin_account_) {
      std::move(callback_).Run(Result::kSuccess);
      return;
    }
  }
}

void WebSigninTracker::OnStateChanged(
    signin_metrics::AccountReconcilorState state) {
  if (state != signin_metrics::AccountReconcilorState::kError) {
    return;
  }

  bool is_auth_error =
      identity_manager_->HasAccountWithRefreshTokenInPersistentErrorState(
          signin_account_);
  Result result = is_auth_error ? Result::kAuthError : Result::kOtherError;
  std::move(callback_).Run(result);
}

}  // namespace signin
