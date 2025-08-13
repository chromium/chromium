// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SIGNIN_HISTORY_SYNC_OPTIN_HELPER_H_
#define CHROME_BROWSER_UI_WEBUI_SIGNIN_HISTORY_SYNC_OPTIN_HELPER_H_

#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/tribool.h"

class AccountCapabilityFetcher;
namespace signin {
class IdentityManager;
}  // namespace signin

// Waits until the capability designating that account-level policies apply
// to an account is fetched.
class EnterprisePolicyCapabilityObserver {
 public:
  EnterprisePolicyCapabilityObserver(
      signin::IdentityManager* identity_manager,
      const AccountInfo& account_info,
      base::OnceCallback<void(signin::Tribool)>
          on_enterprise_policy_eligibility_fetched_callback);
  ~EnterprisePolicyCapabilityObserver();

  void FetchCapability();

  AccountCapabilityFetcher* GetAccountCapabilityFetcherForTesting() {
    return account_enterprise_policy_capability_fetcher_.get();
  }

 private:
  signin::Tribool CanApplyAccountLevelEnterprisePolicies(
      const AccountInfo& account_info);

  std::unique_ptr<AccountCapabilityFetcher>
      account_enterprise_policy_capability_fetcher_;
};

#endif  // CHROME_BROWSER_UI_WEBUI_SIGNIN_HISTORY_SYNC_OPTIN_HELPER_H_
