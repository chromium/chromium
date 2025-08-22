// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SIGNIN_HISTORY_SYNC_OPTIN_HELPER_H_
#define CHROME_BROWSER_UI_WEBUI_SIGNIN_HISTORY_SYNC_OPTIN_HELPER_H_

#include <memory>

#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"
#include "chrome/browser/ui/webui/signin/signin_utils.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/tribool.h"

class AccountCapabilityFetcher;
namespace signin {
class IdentityManager;
}  // namespace signin

// This is a skeleton for the class that shows the history sync optin screen,
// potentially after the account management screen.
// TODO(404806750):
// 1) Add tracking of the SyncService state (which may be disabled).
// 2) Fetch applicable policies for managed accounts.
// 3) Incorporate spinner screens in the flow while we wait for the above
// necessary information to be fetched.
class HistorySyncOptinHelper {
 public:
  class Delegate {
   public:
    virtual void ShowHistorySyncOptinScreen() = 0;
  };

  HistorySyncOptinHelper(signin::IdentityManager* identity_manager,
                         const AccountInfo& account_info,
                         Delegate* delegate);
  ~HistorySyncOptinHelper();

  void StartHistorySyncOptinFlow();

  AccountCapabilityFetcher* GetAccountCapabilityFetcherForTesting() {
    return account_enterprise_policy_capability_fetcher_.get();
  }

 private:
  void MaybeShowHistorySyncOptinScreen(signin::Tribool is_managed_account);

  void ShowHistorySyncOptinScreen();

  signin::Tribool CanApplyAccountLevelEnterprisePolicies(
      const AccountInfo& account_info);

  raw_ptr<Delegate> delegate_;
  std::unique_ptr<AccountCapabilityFetcher>
      account_enterprise_policy_capability_fetcher_;

  base::WeakPtrFactory<HistorySyncOptinHelper> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_WEBUI_SIGNIN_HISTORY_SYNC_OPTIN_HELPER_H_
