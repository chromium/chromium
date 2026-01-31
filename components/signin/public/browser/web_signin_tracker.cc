// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/public/browser/web_signin_tracker.h"

#include <optional>

#include "base/feature_list.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "components/signin/public/base/signin_metrics.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/accounts_in_cookie_jar_info.h"
#include "third_party/abseil-cpp/absl/functional/overload.h"

namespace {
constexpr std::string_view kLatencyResultHistogramName =
    "Signin.WebSigninTracker.Latency";

// LINT.IfChange(GetHistogramResultSuffix)
std::string_view GetHistogramResultSuffix(
    signin::WebSigninTracker::Result result) {
  using enum signin::WebSigninTracker::Result;
  switch (result) {
    case kSuccess:
      return ".Success";
    case kAuthError:
      return ".AuthError";
    case kOtherError:
      return ".OtherError";
    case kTimeout:
      return ".Timeout";
  }
  NOTREACHED();
}
// LINT.ThenChange(//tools/metrics/histograms/metadata/signin/histograms.xml:WebSignintrackerLatency)

}  // namespace
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
  web_signin_tracker_start_time_ = base::TimeTicks::Now();

  identity_manager_observation_.Observe(identity_manager_);
  account_reconcilor_observation_.Observe(account_reconcilor_);

  if (timeout) {
    timeout_timer_.Start(FROM_HERE, *timeout, this,
                         &WebSigninTracker::OnTimeoutReached);
  }

  signin::AccountsInCookieJarInfo info =
      identity_manager_->GetAccountsInCookieJar();
  if (info.AreAccountsFresh() &&
      FinishIfRequestedAccountPresentInCookies(info)) {
    // Already finished successfully - do not call `OnStateChanged`.
    return;
  }

  OnStateChanged(account_reconcilor->GetState());
}

WebSigninTracker::~WebSigninTracker() = default;

void WebSigninTracker::OnAccountsInCookieUpdated(
    const signin::AccountsInCookieJarInfo& accounts_in_cookie_jar_info,
    const GoogleServiceAuthError& error) {
  FinishIfRequestedAccountPresentInCookies(accounts_in_cookie_jar_info);
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

bool WebSigninTracker::FinishIfRequestedAccountPresentInCookies(
    const signin::AccountsInCookieJarInfo& accounts_in_cookie_jar_info) {
  for (const auto& account :
       accounts_in_cookie_jar_info.GetValidSignedInAccounts()) {
    if (MatchesRequestedAccount(account.id, account.email)) {
      FinishWithResult(Result::kSuccess);
      return true;
    }
  }
  return false;
}

void WebSigninTracker::FinishWithResult(WebSigninTracker::Result result) {
  base::UmaHistogramTimes(
      base::StrCat(
          {kLatencyResultHistogramName, GetHistogramResultSuffix(result)}),
      base::TimeTicks::Now() - web_signin_tracker_start_time_);
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
