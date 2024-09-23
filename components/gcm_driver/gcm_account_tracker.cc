// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/gcm_driver/gcm_account_tracker.h"

#include <stdint.h>

#include <algorithm>
#include <vector>

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "components/gcm_driver/gcm_driver.h"
#include "components/signin/public/identity_manager/access_token_fetcher.h"
#include "components/signin/public/identity_manager/access_token_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/scope_set.h"
#include "google_apis/gaia/gaia_constants.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "net/base/ip_endpoint.h"

namespace gcm {

namespace {

// Name of the GCM account tracker for fetching access tokens.
const char kGCMAccountTrackerName[] = "gcm_account_tracker";
// Minimum token validity when sending to GCM groups server.
const int64_t kMinimumTokenValidityMs = 500;
// Token reporting interval, when no account changes are detected.
const int64_t kTokenReportingIntervalMs =
    12 * 60 * 60 * 1000;  // 12 hours in ms.

}  // namespace

GCMAccountTracker::AccountInfo::AccountInfo(const std::string& email,
                                            AccountState state)
    : email(email), state(state) {
}

GCMAccountTracker::AccountInfo::~AccountInfo() {
}

GCMAccountTracker::GCMAccountTracker(
    std::unique_ptr<AccountTracker> account_tracker,
    signin::IdentityManager* identity_manager,
    GCMDriver* driver)
    : account_tracker_(account_tracker.release()),
      identity_manager_(identity_manager),
      driver_(driver),
      shutdown_called_(false) {}

GCMAccountTracker::~GCMAccountTracker() {
  DCHECK(shutdown_called_);
}

void GCMAccountTracker::Shutdown() {
  shutdown_called_ = true;
  driver_->RemoveConnectionObserver(this);
  account_tracker_->RemoveObserver(this);
  account_tracker_->Shutdown();
}

void GCMAccountTracker::Start() {
  DCHECK(!shutdown_called_);
  account_tracker_->AddObserver(this);
  driver_->AddConnectionObserver(this);

  std::vector<CoreAccountInfo> accounts = account_tracker_->GetAccounts();
  for (std::vector<CoreAccountInfo>::const_iterator iter = accounts.begin();
       iter != accounts.end(); ++iter) {
    if (!iter->email.empty()) {
      account_infos_.insert(std::make_pair(
          iter->account_id, AccountInfo(iter->email, TOKEN_NEEDED)));
    }
  }

  if (IsTokenReportingRequired())
    ReportTokens();
  else
    ScheduleReportTokens();
}

void GCMAccountTracker::ScheduleReportTokens() {
  // Shortcutting here, in case GCM Driver is not yet connected. In that case
  // reporting will be scheduled/started when the connection is made.
  if (!driver_->IsConnected())
    return;

  DVLOG(1) << "Deferring the token reporting for: "
           << GetTimeToNextTokenReporting().InSeconds() << " seconds.";

  reporting_weak_ptr_factory_.InvalidateWeakPtrs();
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&GCMAccountTracker::ReportTokens,
                     reporting_weak_ptr_factory_.GetWeakPtr()),
      GetTimeToNextTokenReporting());
}

void GCMAccountTracker::OnAccountSignInChanged(const CoreAccountInfo& account,
                                               bool is_signed_in) {
  if (is_signed_in)
    OnAccountSignedIn(account);
  else
    OnAccountSignedOut(account);
}

void GCMAccountTracker::OnAccessTokenFetchCompleteForAccount(
    CoreAccountId account_id,
    GoogleServiceAuthError error,
    signin::AccessTokenInfo access_token_info) {
  auto iter = account_infos_.find(account_id);
  // DCHECK iter!=end() is sensible here as goal is to report missing values.
  DCHECK(iter != account_infos_.end());
  if (iter != account_infos_.end()) {
    DCHECK_EQ(GETTING_TOKEN, iter->second.state);

    if (error.state() == GoogleServiceAuthError::NONE) {
      DVLOG(1) << "Get token success: " << account_id;

      iter->second.state = TOKEN_PRESENT;
      iter->second.access_token = access_token_info.token;
      iter->second.expiration_time = access_token_info.expiration_time;
    } else {
      DVLOG(1) << "Get token failure: " << account_id;

      // Given the fetcher has a built in retry logic, consider this situation
      // to be invalid refresh token, that is only fixed when user signs in.
      // Once the users signs in properly the minting will retry.
      iter->second.access_token.clear();
      iter->second.state = ACCOUNT_REMOVED;
    }
  }

  pending_token_requests_.erase(account_id);
  ReportTokens();
}

void GCMAccountTracker::OnConnected(const net::IPEndPoint& ip_endpoint) {
  // We are sure here, that GCM is running and connected. We can start reporting
  // tokens if reporting is due now, or schedule reporting for later.
  if (IsTokenReportingRequired())
    ReportTokens();
  else
    ScheduleReportTokens();
}

void GCMAccountTracker::OnDisconnected() {
  // We are disconnected, so no point in trying to work with tokens.
}

void GCMAccountTracker::ReportTokens() {
  SanitizeTokens();
  // Make sure all tokens are valid.
  if (IsTokenFetchingRequired()) {
    GetAllNeededTokens();
    return;
  }

  // Wait for all of the pending token requests from GCMAccountTracker to be
  // done before you report the results.
  if (!pending_token_requests_.empty()) {
    return;
  }

  bool account_removed = false;
  // Stop tracking the accounts, that were removed, as it will be reported to
  // the driver.
  for (auto iter = account_infos_.begin(); iter != account_infos_.end();) {
    if (iter->second.state == ACCOUNT_REMOVED) {
      account_removed = true;
      account_infos_.erase(iter++);
    } else {
      ++iter;
    }
  }

  std::vector<GCMClient::AccountTokenInfo> account_tokens;
  for (auto iter = account_infos_.begin(); iter != account_infos_.end();
       ++iter) {
    if (iter->second.state == TOKEN_PRESENT) {
      GCMClient::AccountTokenInfo token_info;
      token_info.account_id = iter->first;
      token_info.email = iter->second.email;
      token_info.access_token = iter->second.access_token;
      account_tokens.push_back(token_info);
    } else {
      // This should not happen, as we are making a check that there are no
      // pending requests above, stopping tracking of removed accounts, or start
      // fetching tokens.
      NOTREACHED_IN_MIGRATION();
    }
  }

  // Make sure that there is something to report, otherwise bail out.
  if (!account_tokens.empty() || account_removed) {
    DVLOG(1) << "Reporting the tokens to driver: " << account_tokens.size();
    driver_->SetAccountTokens(account_tokens);
    driver_->SetLastTokenFetchTime(base::Time::Now());
    ScheduleReportTokens();
  } else {
    DVLOG(1) << "No tokens and nothing removed. Skipping callback.";
  }
}

void GCMAccountTracker::SanitizeTokens() {
  for (auto iter = account_infos_.begin(); iter != account_infos_.end();
       ++iter) {
    if (iter->second.state == TOKEN_PRESENT &&
        iter->second.expiration_time <
            base::Time::Now() + base::Milliseconds(kMinimumTokenValidityMs)) {
      iter->second.access_token.clear();
      iter->second.state = TOKEN_NEEDED;
      iter->second.expiration_time = base::Time();
    }
  }
}

bool GCMAccountTracker::IsTokenReportingRequired() const {
  if (GetTimeToNextTokenReporting().is_zero())
    return true;

  bool reporting_required = false;
  for (auto iter = account_infos_.begin(); iter != account_infos_.end();
       ++iter) {
    if (iter->second.state == ACCOUNT_REMOVED)
      reporting_required = true;
  }

  return reporting_required;
}

bool GCMAccountTracker::IsTokenFetchingRequired() const {
  bool token_needed = false;
  for (auto iter = account_infos_.begin(); iter != account_infos_.end();
       ++iter) {
    if (iter->second.state == TOKEN_NEEDED)
      token_needed = true;
  }

  return token_needed;
}

base::TimeDelta GCMAccountTracker::GetTimeToNextTokenReporting() const {
  base::TimeDelta time_till_next_reporting =
      driver_->GetLastTokenFetchTime() +
      base::Milliseconds(kTokenReportingIntervalMs) - base::Time::Now();

  // Case when token fetching is overdue.
  if (time_till_next_reporting.is_negative())
    return base::TimeDelta();

  // Case when calculated period is larger than expected, including the
  // situation when the method is called before GCM driver is completely
  // initialized.
  if (time_till_next_reporting >
      base::Milliseconds(kTokenReportingIntervalMs)) {
    return base::Milliseconds(kTokenReportingIntervalMs);
  }

  return time_till_next_reporting;
}

void GCMAccountTracker::GetAllNeededTokens() {
  // Only start fetching tokens if driver is running, they have a limited
  // validity time and GCM connection is a good indication of network running.
  // If the GetAllNeededTokens was called as part of periodic schedule, it may
  // not have network. In that case the next network change will trigger token
  // fetching.
  if (!driver_->IsConnected())
    return;

  // Only start fetching access tokens if the user consented for sync.
  // TODO(crbug.com/40067875): Delete account-tracking code, latest when
  // ConsentLevel::kSync is cleaned up from the codebase.
  if (!identity_manager_->HasPrimaryAccount(signin::ConsentLevel::kSync))
    return;

  for (auto iter = account_infos_.begin(); iter != account_infos_.end();
       ++iter) {
    if (iter->second.state == TOKEN_NEEDED)
      GetToken(iter);
  }
}

void GCMAccountTracker::GetToken(AccountInfos::iterator& account_iter) {
  DCHECK_EQ(account_iter->second.state, TOKEN_NEEDED);

  signin::ScopeSet scopes;
  scopes.insert(GaiaConstants::kGCMGroupServerOAuth2Scope);
  scopes.insert(GaiaConstants::kGCMCheckinServerOAuth2Scope);

  // NOTE: It is safe to use base::Unretained() here as |token_fetcher| is owned
  // by this object and guarantees that it will not invoke its callback after
  // its destruction.
  std::unique_ptr<signin::AccessTokenFetcher> token_fetcher =
      identity_manager_->CreateAccessTokenFetcherForAccount(
          account_iter->first, kGCMAccountTrackerName, scopes,
          base::BindOnce(
              &GCMAccountTracker::OnAccessTokenFetchCompleteForAccount,
              base::Unretained(this), account_iter->first),
          signin::AccessTokenFetcher::Mode::kImmediate);

  DCHECK(pending_token_requests_.count(account_iter->first) == 0);
  pending_token_requests_.emplace(account_iter->first,
                                  std::move(token_fetcher));
  account_iter->second.state = GETTING_TOKEN;
}

void GCMAccountTracker::OnAccountSignedIn(const CoreAccountInfo& account) {
  DVLOG(1) << "Account signed in: " << account.email;
  auto iter = account_infos_.find(account.account_id);
  if (iter == account_infos_.end()) {
    DCHECK(!account.email.empty());
    account_infos_.insert(std::make_pair(
        account.account_id, AccountInfo(account.email, TOKEN_NEEDED)));
  } else if (iter->second.state == ACCOUNT_REMOVED) {
    iter->second.state = TOKEN_NEEDED;
  }

  GetAllNeededTokens();
}

void GCMAccountTracker::OnAccountSignedOut(const CoreAccountInfo& account) {
  DVLOG(1) << "Account signed out: " << account.email;
  auto iter = account_infos_.find(account.account_id);
  if (iter == account_infos_.end())
    return;

  iter->second.access_token.clear();
  iter->second.state = ACCOUNT_REMOVED;

  // Delete any ongoing access token request now so that if the account is later
  // re-added and a new access token request made, we do not break this class'
  // invariant that there is at most one ongoing access token request per
  // account.
  pending_token_requests_.erase(account.account_id);
  ReportTokens();
}

}  // namespace gcm
