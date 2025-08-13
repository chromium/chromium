// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/public/identity_manager/account_capability_fetcher.h"

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/scoped_observation.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/tribool.h"

namespace {
base::TimeDelta g_account_capability_fetch_delay = base::Seconds(3);
}  // namespace

AccountCapabilityFetcher::AccountCapabilityFetcher(
    signin::IdentityManager* identity_manager,
    CoreAccountInfo core_account_info,
    base::RepeatingCallback<signin::Tribool(const AccountInfo&)>
        get_capability_state_callback,
    base::OnceCallback<void(signin::Tribool)> on_capability_fetched_callback)
    : identity_manager_(identity_manager),
      core_account_info_(core_account_info),
      get_capability_state_callback_(std::move(get_capability_state_callback)),
      on_capability_fetched_callback_(
          std::move(on_capability_fetched_callback)) {
  CHECK(identity_manager_);
  CHECK(on_capability_fetched_callback_);
  CHECK(get_capability_state_callback_);
}

AccountCapabilityFetcher::~AccountCapabilityFetcher() = default;

void AccountCapabilityFetcher::FetchCapability() {
  // Start a timeout for the capability fetching.
  capability_available_timeout_timer_.Start(
      FROM_HERE, g_account_capability_fetch_delay,
      base::BindOnce(&AccountCapabilityFetcher::OnCapabilityFetchedTimeout,
                     // Unretained is fine as this class owns the timer.
                     base::Unretained(this)));
  // Wait for the capability to arrive.
  GetOrWaitForCapability(core_account_info_);
}

void AccountCapabilityFetcher::EnforceTimeoutReachedForTesting() {
  capability_available_timeout_timer_.FireNow();
}

void AccountCapabilityFetcher::OnExtendedAccountInfoUpdated(
    const AccountInfo& account_info) {
  if (account_info.account_id != core_account_info_.account_id) {
    return;
  }
  GetOrWaitForCapability(account_info);
}

void AccountCapabilityFetcher::OnIdentityManagerShutdown(
    signin::IdentityManager* identity_manager) {
  CHECK(identity_manager == identity_manager_.get());
  account_info_update_observation_.Reset();
  identity_manager_ = nullptr;
}

void AccountCapabilityFetcher::GetOrWaitForCapability(
    const CoreAccountInfo& core_account_info) {
  CHECK(core_account_info.account_id == core_account_info_.account_id);
  AccountInfo extended_account_info =
      identity_manager_->FindExtendedAccountInfo(core_account_info);
  signin::Tribool capability_value =
      get_capability_state_callback_.Run(extended_account_info);

  if (capability_value == signin::Tribool::kUnknown) {
    if (!account_info_update_observation_.IsObserving()) {
      account_info_update_observation_.Observe(identity_manager_);
    }
    return;
  }
  CHECK(capability_value != signin::Tribool::kUnknown);
  account_info_update_observation_.Reset();
  OnCapabilityFetched(capability_value);
}

void AccountCapabilityFetcher::OnCapabilityFetched(
    signin::Tribool account_capability_value) {
  // TODO(434964019): Confirm whether the capability is expected to remain
  // unchanged, or whether we should observe and handle updates of the
  // capability.
  // Cancel the timeout callback.
  capability_available_timeout_timer_.Stop();

  CHECK(!on_capability_fetched_callback_.is_null());
  std::move(on_capability_fetched_callback_).Run(account_capability_value);
}

void AccountCapabilityFetcher::OnCapabilityFetchedTimeout() {
  account_info_update_observation_.Reset();
  OnCapabilityFetched(signin::Tribool::kUnknown);
  // TODO(anthie): Maybe record metrics for this case.
}
