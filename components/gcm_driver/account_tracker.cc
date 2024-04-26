// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/gcm_driver/account_tracker.h"

#include "base/containers/contains.h"
#include "base/logging.h"
#include "base/observer_list.h"
#include "base/trace_event/trace_event.h"
#include "components/signin/public/identity_manager/access_token_info.h"

namespace gcm {

AccountTracker::AccountTracker(signin::IdentityManager* identity_manager)
    : identity_manager_(identity_manager), shutdown_called_(false) {
  identity_manager_->AddObserver(this);
}

AccountTracker::~AccountTracker() {
  DCHECK(shutdown_called_);
}

void AccountTracker::Shutdown() {
  shutdown_called_ = true;
  identity_manager_->RemoveObserver(this);
}

void AccountTracker::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void AccountTracker::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

std::vector<CoreAccountInfo> AccountTracker::GetAccounts() const {
  // TODO(crbug.com/40067875): Delete account-tracking code, latest when
  // ConsentLevel::kSync is cleaned up from the codebase.
  const CoreAccountId active_account_id =
      identity_manager_->GetPrimaryAccountId(signin::ConsentLevel::kSync);
  std::vector<CoreAccountInfo> accounts;

  for (auto it = accounts_.begin(); it != accounts_.end(); ++it) {
    const AccountState& state = it->second;
    DCHECK(!state.account.account_id.empty());
    DCHECK(!state.account.gaia.empty());
    DCHECK(!state.account.email.empty());
    bool is_visible = state.is_signed_in;

    if (it->first == active_account_id) {
      if (is_visible)
        accounts.insert(accounts.begin(), state.account);
      else
        return std::vector<CoreAccountInfo>();
    } else if (is_visible) {
      accounts.push_back(state.account);
    }
  }
  return accounts;
}

void AccountTracker::OnRefreshTokenUpdatedForAccount(
    const CoreAccountInfo& account_info) {
  TRACE_EVENT1("identity", "AccountTracker::OnRefreshTokenUpdatedForAccount",
               "account_id", account_info.account_id.ToString());

  // Ignore refresh tokens if there is no active account ID at all.
  // TODO(crbug.com/40067875): Delete account-tracking code, latest when
  // ConsentLevel::kSync is cleaned up from the codebase.
  if (!identity_manager_->HasPrimaryAccount(signin::ConsentLevel::kSync))
    return;

  DVLOG(1) << "AVAILABLE " << account_info.account_id;
  StartTrackingAccount(account_info);
  UpdateSignInState(account_info.account_id, /*is_signed_in=*/true);
}

void AccountTracker::OnRefreshTokenRemovedForAccount(
    const CoreAccountId& account_id) {
  TRACE_EVENT1("identity", "AccountTracker::OnRefreshTokenRemovedForAccount",
               "account_id", account_id.ToString());

  DVLOG(1) << "REVOKED " << account_id;
  UpdateSignInState(account_id, /*is_signed_in=*/false);
}

void AccountTracker::OnPrimaryAccountChanged(
    const signin::PrimaryAccountChangeEvent& event) {
  // TODO(crbug.com/40067875): Delete account-tracking code, latest when
  // ConsentLevel::kSync is cleaned up from the codebase.
  switch (event.GetEventTypeFor(signin::ConsentLevel::kSync)) {
    case signin::PrimaryAccountChangeEvent::Type::kSet: {
      TRACE_EVENT0("identity", "AccountTracker::OnPrimaryAccountChanged");
      std::vector<CoreAccountInfo> accounts =
          identity_manager_->GetAccountsWithRefreshTokens();
      DVLOG(1) << "LOGIN " << accounts.size() << " accounts available.";
      for (const CoreAccountInfo& account_info : accounts) {
        StartTrackingAccount(account_info);
        UpdateSignInState(account_info.account_id, /*is_signed_in=*/true);
      }
      break;
    }
    case signin::PrimaryAccountChangeEvent::Type::kCleared: {
      TRACE_EVENT0("identity", "AccountTracker::OnPrimaryAccountCleared");
      DVLOG(1) << "LOGOUT";
      StopTrackingAllAccounts();
      break;
    }
    case signin::PrimaryAccountChangeEvent::Type::kNone:
      break;
  }
}

void AccountTracker::UpdateSignInState(const CoreAccountId& account_id,
                                       bool is_signed_in) {
  if (!is_signed_in && !base::Contains(accounts_, account_id))
    return;

  DCHECK(base::Contains(accounts_, account_id));
  AccountState& account = accounts_[account_id];
  if (account.is_signed_in == is_signed_in)
    return;

  account.is_signed_in = is_signed_in;
  for (auto& observer : observer_list_)
    observer.OnAccountSignInChanged(account.account, account.is_signed_in);
}

void AccountTracker::StartTrackingAccount(const CoreAccountInfo& account) {
  if (base::Contains(accounts_, account.account_id))
    return;

  DVLOG(1) << "StartTracking " << account.account_id;
  AccountState account_state;
  account_state.account = account;
  account_state.is_signed_in = false;
  accounts_.insert(std::make_pair(account.account_id, account_state));
}

void AccountTracker::StopTrackingAccount(const CoreAccountId account_id) {
  DVLOG(1) << "StopTracking " << account_id;
  if (base::Contains(accounts_, account_id)) {
    UpdateSignInState(account_id, /*is_signed_in=*/false);
    accounts_.erase(account_id);
  }
}

void AccountTracker::StopTrackingAllAccounts() {
  while (!accounts_.empty())
    StopTrackingAccount(accounts_.begin()->first);
}

}  // namespace gcm
