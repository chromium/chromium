// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SIGNIN_HISTORY_SYNC_OPTIN_HELPER_H_
#define CHROME_BROWSER_UI_WEBUI_SIGNIN_HISTORY_SYNC_OPTIN_HELPER_H_

#include <memory>

#include "base/functional/callback.h"
#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"
#include "chrome/browser/sync/sync_startup_tracker.h"
#include "chrome/browser/ui/webui/signin/signin_utils.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/tribool.h"

class AccountCapabilityFetcher;
class Profile;
namespace signin {
class IdentityManager;
}  // namespace signin
namespace syncer {
class SyncService;
}  // namespace syncer

// Helper class to track the state of the SyncService..
// Executes a callback when the SyncService's state is no longer pending.
class SyncServiceStartupStateObserver {
 public:
  SyncServiceStartupStateObserver(syncer::SyncService* sync_service,
                                  base::OnceClosure on_state_updated_callback);
  ~SyncServiceStartupStateObserver();

  static std::unique_ptr<SyncServiceStartupStateObserver>
  MaybeCreateSyncServiceStateObserverForAccountWithClouldPolicies(
      syncer::SyncService* sync_service,
      Profile* profile,
      const AccountInfo& account_info,
      base::OnceClosure callback);

  // Public for testing.
  void OnSyncStartupStateChanged(SyncStartupTracker::ServiceStartupState state);

 private:
  base::OnceClosure on_state_updated_callback_;
  std::unique_ptr<SyncStartupTracker> sync_startup_tracker_;
  base::WeakPtrFactory<SyncServiceStartupStateObserver> weak_pointer_factory_{
      this};
};

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
                         Profile* profile,
                         const AccountInfo& account_info,
                         Delegate* delegate);
  ~HistorySyncOptinHelper();

  void StartHistorySyncOptinFlow();

  AccountCapabilityFetcher* GetAccountCapabilityFetcherForTesting() {
    return account_enterprise_policy_capability_fetcher_.get();
  }

 private:
  void StartShowHistorySyncOptinScreenFlow(signin::Tribool is_managed_account);
  void MaybeShowHistorySyncOptinScreen();
  void ShowHistorySyncOptinScreen();

  signin::Tribool CanApplyAccountLevelEnterprisePolicies(
      const AccountInfo& account_info);

  raw_ptr<Profile> profile_;
  const AccountInfo account_info_;
  raw_ptr<Delegate> delegate_;
  std::unique_ptr<AccountCapabilityFetcher>
      account_enterprise_policy_capability_fetcher_;

  signin::Tribool is_managed_account_ = signin::Tribool::kUnknown;
  std::unique_ptr<SyncServiceStartupStateObserver> sync_startup_state_observer_;

  base::WeakPtrFactory<HistorySyncOptinHelper> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_WEBUI_SIGNIN_HISTORY_SYNC_OPTIN_HELPER_H_
