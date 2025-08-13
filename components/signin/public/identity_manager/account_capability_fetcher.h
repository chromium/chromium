// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_PUBLIC_IDENTITY_MANAGER_ACCOUNT_CAPABILITY_FETCHER_H_
#define COMPONENTS_SIGNIN_PUBLIC_IDENTITY_MANAGER_ACCOUNT_CAPABILITY_FETCHER_H_

#include "base/functional/callback_forward.h"
#include "base/scoped_observation.h"
#include "base/timer/timer.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/tribool.h"

// Waits until a capability is fetched.
// The capability state is computed through the input callback
// `get_capability_state_callback_`.
// When the capability is fetched or we timeout while waiting for it,
// it executes the provided callback `on_capability_fetched_callback`
// and stops observing for further updates.
// Expected to be used for a single fetch only.
class AccountCapabilityFetcher : public signin::IdentityManager::Observer {
 public:
  AccountCapabilityFetcher(
      signin::IdentityManager* identity_manager,
      CoreAccountInfo core_account_info,
      base::RepeatingCallback<signin::Tribool(const AccountInfo&)>
          get_capability_state_callback,
      base::OnceCallback<void(signin::Tribool)> on_capability_fetched_callback);

  AccountCapabilityFetcher();
  ~AccountCapabilityFetcher() override;

  // Starts fetching the capability. Returns the result through
  // `on_capability_fetched_callback_` when ready and may
  // return right away if already available. Should be called once.
  void FetchCapability();

  void EnforceTimeoutReachedForTesting();

 private:
  // signin::IdentityManager::Observer:
  void OnExtendedAccountInfoUpdated(const AccountInfo& account_info) override;
  void OnIdentityManagerShutdown(
      signin::IdentityManager* identity_manager) override;

  void GetOrWaitForCapability(const CoreAccountInfo& core_account_info);

  void OnCapabilityFetched(signin::Tribool account_capability_value);

  void OnCapabilityFetchedTimeout();

  base::ScopedObservation<signin::IdentityManager,
                          signin::IdentityManager::Observer>
      account_info_update_observation_{this};

  raw_ptr<signin::IdentityManager> identity_manager_;
  CoreAccountInfo core_account_info_;
  base::RepeatingCallback<signin::Tribool(const AccountInfo&)>
      get_capability_state_callback_;
  base::OnceCallback<void(signin::Tribool)> on_capability_fetched_callback_;
  base::OneShotTimer capability_available_timeout_timer_;
};

#endif  // COMPONENTS_SIGNIN_PUBLIC_IDENTITY_MANAGER_ACCOUNT_CAPABILITY_FETCHER_H_
