// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/public/identity_manager/account_state_fetcher.h"

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/scoped_observation.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/tribool.h"

namespace {
base::TimeDelta g_account_info_fetch_delay = base::Seconds(3);
}  // namespace

AccountStateFetcher::AccountStateFetcher(
    signin::IdentityManager* identity_manager,
    CoreAccountInfo core_account_info,
    base::RepeatingCallback<signin::Tribool(const AccountInfo&)>
        get_account_state_callback,
    base::OnceCallback<void(signin::Tribool)>
        on_account_info_fetched_callback)
    : identity_manager_(identity_manager),
      core_account_info_(core_account_info),
      get_account_state_callback_(std::move(get_account_state_callback)),
      on_account_info_fetched_callback_(
          std::move(on_account_info_fetched_callback)) {
  CHECK(identity_manager_);
  CHECK(on_account_info_fetched_callback_);
  CHECK(get_account_state_callback_);
}

AccountStateFetcher::~AccountStateFetcher() = default;

void AccountStateFetcher::FetchAccountInfo() {
  // Start a timeout for the account info fetching.
  account_info_timeout_timer_.Start(
      FROM_HERE, g_account_info_fetch_delay,
      base::BindOnce(&AccountStateFetcher::OnAccountInfoFetchTimeout,
                     // Unretained is fine as this class owns the timer.
                     base::Unretained(this)));
  // Wait for the account info to arrive.
  GetOrWaitForAccountInfo(core_account_info_);
}

void AccountStateFetcher::EnforceTimeoutReachedForTesting() {
  account_info_timeout_timer_.FireNow();
}

void AccountStateFetcher::OnExtendedAccountInfoUpdated(
    const AccountInfo& account_info) {
  if (account_info.account_id != core_account_info_.account_id) {
    return;
  }
  GetOrWaitForAccountInfo(account_info);
}

void AccountStateFetcher::OnIdentityManagerShutdown(
    signin::IdentityManager* identity_manager) {
  CHECK(identity_manager == identity_manager_.get());
  account_info_update_observation_.Reset();
  identity_manager_ = nullptr;
}

void AccountStateFetcher::GetOrWaitForAccountInfo(
    const CoreAccountInfo& core_account_info) {
  CHECK(core_account_info.account_id == core_account_info_.account_id);
  AccountInfo extended_account_info =
      identity_manager_->FindExtendedAccountInfo(core_account_info);
  signin::Tribool account_info_value =
      get_account_state_callback_.Run(extended_account_info);

  if (account_info_value == signin::Tribool::kUnknown) {
    if (!account_info_update_observation_.IsObserving()) {
      account_info_update_observation_.Observe(identity_manager_);
    }
    return;
  }
  CHECK(account_info_value != signin::Tribool::kUnknown);
  account_info_update_observation_.Reset();
  OnAccountInfoFetched(account_info_value);
}

void AccountStateFetcher::OnAccountInfoFetched(
    signin::Tribool account_info_value) {
  // TODO(434964019): Confirm whether the account info is expected to remain
  // unchanged, or whether we should observe and handle updates of the
  // account info.
  // Cancel the timeout callback.
  account_info_timeout_timer_.Stop();

  CHECK(!on_account_info_fetched_callback_.is_null());
  std::move(on_account_info_fetched_callback_).Run(account_info_value);
}

void AccountStateFetcher::OnAccountInfoFetchTimeout() {
  account_info_update_observation_.Reset();
  OnAccountInfoFetched(signin::Tribool::kUnknown);
  // TODO(anthie): Maybe record metrics for this case.
}
