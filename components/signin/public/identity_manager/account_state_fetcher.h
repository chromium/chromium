// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_PUBLIC_IDENTITY_MANAGER_ACCOUNT_STATE_FETCHER_H_
#define COMPONENTS_SIGNIN_PUBLIC_IDENTITY_MANAGER_ACCOUNT_STATE_FETCHER_H_

#include "base/functional/callback_forward.h"
#include "base/scoped_observation.h"
#include "base/timer/timer.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/tribool.h"

// Waits until some account info is fetched.
// The account state is computed through the input callback
// `get_account_state_callback_`.
// When the account info is fetched or we timeout while waiting for it,
// it executes the provided callback `on_account_info_fetched_callback_`
// and stops observing for further updates.
// Expected to be used for a single fetch only.
class AccountStateFetcher : public signin::IdentityManager::Observer {
 public:
  AccountStateFetcher(
      signin::IdentityManager* identity_manager,
      CoreAccountInfo core_account_info,
      base::RepeatingCallback<signin::Tribool(const AccountInfo&)>
          get_account_state_callback,
      base::OnceCallback<void(signin::Tribool)>
          on_account_info_fetched_callback);

  AccountStateFetcher();
  ~AccountStateFetcher() override;

  // Starts fetching the account info. Returns the result through
  // `on_account_info_fetched_callback_` when ready and may
  // return right away if already available. Should be called once.
  void FetchAccountInfo();

  void EnforceTimeoutReachedForTesting();

 private:
  // signin::IdentityManager::Observer:
  void OnExtendedAccountInfoUpdated(const AccountInfo& account_info) override;
  void OnIdentityManagerShutdown(
      signin::IdentityManager* identity_manager) override;

  void GetOrWaitForAccountInfo(const CoreAccountInfo& core_account_info);

  void OnAccountInfoFetched(signin::Tribool account_info_value);

  void OnAccountInfoFetchTimeout();

  base::ScopedObservation<signin::IdentityManager,
                          signin::IdentityManager::Observer>
      account_info_update_observation_{this};

  raw_ptr<signin::IdentityManager> identity_manager_;
  CoreAccountInfo core_account_info_;
  base::RepeatingCallback<signin::Tribool(const AccountInfo&)>
      get_account_state_callback_;
  base::OnceCallback<void(signin::Tribool)> on_account_info_fetched_callback_;
  base::OneShotTimer account_info_timeout_timer_;
};

#endif  // COMPONENTS_SIGNIN_PUBLIC_IDENTITY_MANAGER_ACCOUNT_STATE_FETCHER_H_
