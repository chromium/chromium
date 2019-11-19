// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/gcm_driver/account_tracker.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/stl_util.h"
#include "base/trace_event/trace_event.h"
#include "components/signin/public/identity_manager/access_token_info.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace gcm {

AccountTracker::AccountTracker(
    signin::IdentityManager* identity_manager,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
    : identity_manager_(identity_manager),
      url_loader_factory_(std::move(url_loader_factory)),
      shutdown_called_(false) {
  identity_manager_->AddObserver(this);
}

AccountTracker::~AccountTracker() {
  DCHECK(shutdown_called_);
}

void AccountTracker::Shutdown() {
  shutdown_called_ = true;
  user_info_requests_.clear();
  identity_manager_->RemoveObserver(this);
}

bool AccountTracker::IsAllUserInfoFetched() const {
  return user_info_requests_.empty();
}

void AccountTracker::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void AccountTracker::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

std::vector<AccountIds> AccountTracker::GetAccounts() const {
  const CoreAccountId active_account_id =
      identity_manager_->GetPrimaryAccountId();
  std::vector<AccountIds> accounts;

  for (auto it = accounts_.begin(); it != accounts_.end(); ++it) {
    const AccountState& state = it->second;
    bool is_visible = state.is_signed_in && !state.ids.gaia.empty();

    if (it->first == active_account_id) {
      if (is_visible)
        accounts.insert(accounts.begin(), state.ids);
      else
        return std::vector<AccountIds>();

    } else if (is_visible) {
      accounts.push_back(state.ids);
    }
  }
  return accounts;
}

void AccountTracker::OnRefreshTokenUpdatedForAccount(
    const CoreAccountInfo& account_info) {
  TRACE_EVENT1("identity", "AccountTracker::OnRefreshTokenUpdatedForAccount",
               "account_id", account_info.account_id.id);

  // Ignore refresh tokens if there is no active account ID at all.
  if (!identity_manager_->HasPrimaryAccount())
    return;

  DVLOG(1) << "AVAILABLE " << account_info.account_id;
  UpdateSignInState(account_info.account_id, /*is_signed_in=*/true);
}

void AccountTracker::OnRefreshTokenRemovedForAccount(
    const CoreAccountId& account_id) {
  TRACE_EVENT1("identity", "AccountTracker::OnRefreshTokenRemovedForAccount",
               "account_id", account_id.id);

  DVLOG(1) << "REVOKED " << account_id;
  UpdateSignInState(account_id, /*is_signed_in=*/false);
}

void AccountTracker::OnPrimaryAccountSet(
    const CoreAccountInfo& primary_account_info) {
  TRACE_EVENT0("identity", "AccountTracker::OnPrimaryAccountSet");

  std::vector<CoreAccountInfo> accounts =
      identity_manager_->GetAccountsWithRefreshTokens();

  DVLOG(1) << "LOGIN " << accounts.size() << " accounts available.";

  for (const CoreAccountInfo& account_info : accounts) {
    UpdateSignInState(account_info.account_id, /*is_signed_in=*/true);
  }
}

void AccountTracker::OnPrimaryAccountCleared(
    const CoreAccountInfo& previous_primary_account_info) {
  TRACE_EVENT0("identity", "AccountTracker::OnPrimaryAccountCleared");
  DVLOG(1) << "LOGOUT";
  StopTrackingAllAccounts();
}

void AccountTracker::NotifySignInChanged(const AccountState& account) {
  DCHECK(!account.ids.gaia.empty());
  for (auto& observer : observer_list_)
    observer.OnAccountSignInChanged(account.ids, account.is_signed_in);
}

void AccountTracker::UpdateSignInState(const CoreAccountId& account_key,
                                       bool is_signed_in) {
  StartTrackingAccount(account_key);
  AccountState& account = accounts_[account_key];
  bool needs_gaia_id = account.ids.gaia.empty();
  bool was_signed_in = account.is_signed_in;
  account.is_signed_in = is_signed_in;

  if (needs_gaia_id && is_signed_in)
    StartFetchingUserInfo(account_key);

  if (!needs_gaia_id && (was_signed_in != is_signed_in))
    NotifySignInChanged(account);
}

void AccountTracker::StartTrackingAccount(const CoreAccountId& account_key) {
  if (!base::Contains(accounts_, account_key)) {
    DVLOG(1) << "StartTracking " << account_key;
    AccountState account_state;
    account_state.ids.account_key = account_key;
    account_state.ids.email = account_key.id;
    account_state.is_signed_in = false;
    accounts_.insert(std::make_pair(account_key, account_state));
  }
}

void AccountTracker::StopTrackingAccount(const CoreAccountId account_key) {
  DVLOG(1) << "StopTracking " << account_key;
  if (base::Contains(accounts_, account_key)) {
    AccountState& account = accounts_[account_key];
    if (!account.ids.gaia.empty()) {
      UpdateSignInState(account_key, /*is_signed_in=*/false);
    }
    accounts_.erase(account_key);
  }

  if (base::Contains(user_info_requests_, account_key))
    DeleteFetcher(user_info_requests_[account_key].get());
}

void AccountTracker::StopTrackingAllAccounts() {
  while (!accounts_.empty())
    StopTrackingAccount(accounts_.begin()->first);
}

void AccountTracker::StartFetchingUserInfo(const CoreAccountId& account_key) {
  if (base::Contains(user_info_requests_, account_key)) {
    DeleteFetcher(user_info_requests_[account_key].get());
  }

  DVLOG(1) << "StartFetching " << account_key;
  AccountIdFetcher* fetcher = new AccountIdFetcher(
      identity_manager_, url_loader_factory_, this, account_key);
  user_info_requests_[account_key] = base::WrapUnique(fetcher);
  fetcher->Start();
}

void AccountTracker::OnUserInfoFetchSuccess(AccountIdFetcher* fetcher,
                                            const std::string& gaia_id) {
  const CoreAccountId& account_key = fetcher->account_key();
  DCHECK(base::Contains(accounts_, account_key));
  AccountState& account = accounts_[account_key];

  account.ids.gaia = gaia_id;

  if (account.is_signed_in)
    NotifySignInChanged(account);

  DeleteFetcher(fetcher);
}

void AccountTracker::OnUserInfoFetchFailure(AccountIdFetcher* fetcher) {
  LOG(WARNING) << "Failed to get UserInfo for " << fetcher->account_key();
  CoreAccountId key = fetcher->account_key();
  DeleteFetcher(fetcher);
  StopTrackingAccount(key);
}

void AccountTracker::DeleteFetcher(AccountIdFetcher* fetcher) {
  DVLOG(1) << "DeleteFetcher " << fetcher->account_key();
  const CoreAccountId& account_key = fetcher->account_key();
  DCHECK(base::Contains(user_info_requests_, account_key));
  DCHECK_EQ(fetcher, user_info_requests_[account_key].get());
  user_info_requests_.erase(account_key);
}

AccountIdFetcher::AccountIdFetcher(
    signin::IdentityManager* identity_manager,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    AccountTracker* tracker,
    const CoreAccountId& account_key)
    : identity_manager_(identity_manager),
      url_loader_factory_(std::move(url_loader_factory)),
      tracker_(tracker),
      account_key_(account_key) {
  TRACE_EVENT_ASYNC_BEGIN1("identity", "AccountIdFetcher", this, "account_key",
                           account_key.id);
}

AccountIdFetcher::~AccountIdFetcher() {
  TRACE_EVENT_ASYNC_END0("identity", "AccountIdFetcher", this);
}

void AccountIdFetcher::Start() {
  identity::ScopeSet scopes;
  scopes.insert("https://www.googleapis.com/auth/userinfo.profile");
  access_token_fetcher_ = identity_manager_->CreateAccessTokenFetcherForAccount(
      account_key_, "gaia_account_tracker", scopes,
      base::BindOnce(&AccountIdFetcher::AccessTokenFetched,
                     base::Unretained(this)),
      signin::AccessTokenFetcher::Mode::kImmediate);
}

void AccountIdFetcher::AccessTokenFetched(
    GoogleServiceAuthError error,
    signin::AccessTokenInfo access_token_info) {
  access_token_fetcher_.reset();

  if (error != GoogleServiceAuthError::AuthErrorNone()) {
    TRACE_EVENT_ASYNC_STEP_PAST1("identity", "AccountIdFetcher", this,
                                 "AccessTokenFetched",
                                 "google_service_auth_error", error.ToString());
    LOG(ERROR) << "AccessTokenFetched error: " << error.ToString();
    tracker_->OnUserInfoFetchFailure(this);
    return;
  }

  TRACE_EVENT_ASYNC_STEP_PAST0("identity", "AccountIdFetcher", this,
                               "AccessTokenFetched");

  gaia_oauth_client_.reset(new gaia::GaiaOAuthClient(url_loader_factory_));

  const int kMaxGetUserIdRetries = 3;
  gaia_oauth_client_->GetUserId(access_token_info.token, kMaxGetUserIdRetries,
                                this);
}

void AccountIdFetcher::OnGetUserIdResponse(const std::string& gaia_id) {
  TRACE_EVENT_ASYNC_STEP_PAST1("identity", "AccountIdFetcher", this,
                               "OnGetUserIdResponse", "gaia_id", gaia_id);
  tracker_->OnUserInfoFetchSuccess(this, gaia_id);
}

void AccountIdFetcher::OnOAuthError() {
  TRACE_EVENT_ASYNC_STEP_PAST0("identity", "AccountIdFetcher", this,
                               "OnOAuthError");
  LOG(ERROR) << "OnOAuthError";
  tracker_->OnUserInfoFetchFailure(this);
}

void AccountIdFetcher::OnNetworkError(int response_code) {
  TRACE_EVENT_ASYNC_STEP_PAST1("identity", "AccountIdFetcher", this,
                               "OnNetworkError", "response_code",
                               response_code);
  LOG(ERROR) << "OnNetworkError " << response_code;
  tracker_->OnUserInfoFetchFailure(this);
}

}  // namespace gcm
