// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/public/browser/web_signin_tracker.h"

#include <optional>

#include "base/feature_list.h"
#include "components/signin/public/base/signin_metrics.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/accounts_in_cookie_jar_info.h"
#include "third_party/abseil-cpp/absl/functional/overload.h"
namespace signin {

WebSigninTracker::WebSigninTracker(
    IdentityManager* identity_manager,
    AccountReconcilor* account_reconcilor,
    std::variant<CoreAccountId, std::string> signin_account,
    base::OnceCallback<void(WebSigninTracker::Result)> callback,
    std::optional<base::TimeDelta> timeout)
    : identity_manager_(identity_manager),
      account_reconcilor_(account_reconcilor),
      signin_account_(signin_account),
      callback_(std::move(callback)) {
  CHECK(callback_);

  identity_manager_observation_.Observe(identity_manager_);
  account_reconcilor_observation_.Observe(account_reconcilor_);

  if (timeout) {
    timeout_timer_.Start(FROM_HERE, *timeout, this,
                         &WebSigninTracker::OnTimeoutReached);
  }

  signin::AccountsInCookieJarInfo info =
      identity_manager_->GetAccountsInCookieJar();
  if (info.AreAccountsFresh()) {
    // Check whether the target primary account is already in cookies.
    OnAccountsInCookieUpdated(info, GoogleServiceAuthError::AuthErrorNone());
  }

  OnStateChanged(account_reconcilor->GetState());
}

WebSigninTracker::~WebSigninTracker() = default;

void WebSigninTracker::OnAccountsInCookieUpdated(
    const signin::AccountsInCookieJarInfo& accounts_in_cookie_jar_info,
    const GoogleServiceAuthError& error) {
  for (const auto& account :
       accounts_in_cookie_jar_info.GetValidSignedInAccounts()) {
    if (MatchesRequestedAccount(account.id, account.email)) {
      FinishWithResult(Result::kSuccess);
      return;
    }
  }
}

void WebSigninTracker::OnIdentityManagerShutdown(
    IdentityManager* identity_manager) {
  CHECK_EQ(identity_manager, identity_manager_);
  identity_manager_observation_.Reset();
}

void WebSigninTracker::OnStateChanged(
    signin_metrics::AccountReconcilorState state) {
  if (state != signin_metrics::AccountReconcilorState::kError) {
    return;
  }

  // If account is not on the device ignore errors from the account reconciler.
  for (const CoreAccountInfo& account :
       identity_manager_->GetAccountsWithRefreshTokens()) {
    if (MatchesRequestedAccount(account.account_id, account.email)) {
      bool is_auth_error = false;
      if (std::holds_alternative<CoreAccountId>(signin_account_)) {
        is_auth_error =
            identity_manager_->HasAccountWithRefreshTokenInPersistentErrorState(
                std::get<CoreAccountId>(signin_account_));
      } else {
        // The account is not on the device and so this will be an auth error.
        is_auth_error = true;
      }

      FinishWithResult(is_auth_error ? Result::kAuthError
                                     : Result::kOtherError);
    }
  }
}

void WebSigninTracker::OnTimeoutReached() {
  FinishWithResult(Result::kTimeout);
}

void WebSigninTracker::FinishWithResult(WebSigninTracker::Result result) {
  identity_manager_observation_.Reset();
  account_reconcilor_observation_.Reset();
  timeout_timer_.Stop();
  std::move(callback_).Run(result);
}

bool WebSigninTracker::MatchesRequestedAccount(
    const CoreAccountId& account_id,
    const std::string& account_email) {
  return std::visit(absl::Overload(
                        [&account_id](const CoreAccountId& requested_id) {
                          return account_id == requested_id;
                        },
                        [&account_email](const std::string& requested_email) {
                          return gaia::AreEmailsSame(account_email,
                                                     requested_email);
                        }),
                    signin_account_);
}

}  // namespace signin
