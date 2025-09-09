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

class AccountStateFetcher;
class Profile;
class TurnSyncOnHelperPolicyFetchTracker;
class ProfileManagementDisclaimerService;

namespace signin {
class IdentityManager;
}  // namespace signin
namespace syncer {
class SyncService;
}  // namespace syncer

// Helper class to track the state of the SyncService.
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

// Helper class to determine if a user is managed and fetch the applicable
// policies. Executes a callback when the policy fetching is done or if the
// user is not managed so there are no policies to fetch.
class HistorySyncOptinPolicyHelper {
 public:
  HistorySyncOptinPolicyHelper(Profile* profile,
               const AccountInfo& account_info,
               base::OnceCallback<void(bool)> on_register_for_policies_callback,
               base::OnceClosure on_policies_fetched_callback);
  ~HistorySyncOptinPolicyHelper();

  // Starts the process of registering the policies for a potentially managed
  // user.
  void RegisterForPolicies();
  // Fetches the policies for a managed account.
  void FetchPolicies();

 private:
  raw_ptr<Profile> profile_;
  const AccountInfo account_info_;

  // Callback executed when the policies are fetched.
  base::OnceCallback<void(bool)> on_register_for_policies_callback_;
  base::OnceClosure on_policies_fetched_callback_;
  std::unique_ptr<TurnSyncOnHelperPolicyFetchTracker> policy_fetch_tracker_;

  base::WeakPtrFactory<HistorySyncOptinPolicyHelper> weak_ptr_factory_{this};
};

// This is a skeleton for the class that shows the history sync optin screen,
// potentially after the account management screen.
// TODO(crbug.com/404806750):Incorporate spinner screens in the flow while we
// wait for the above necessary information to be fetched.
class HistorySyncOptinHelper {
 public:
  // The two contexts below are mutually exclusive.
  // `kInProfilePicker`: The flow is running in the profile picker.
  // `kInBrowser`: The flow is running in a browser window.
  enum class LaunchContext : int { kInProfilePicker = 0, kInBrowser = 1 };

  class Delegate {
   public:
    virtual ~Delegate() = default;
    // Displays the history sync optin screen.
    virtual void ShowHistorySyncOptinScreen(Profile* profile) = 0;
    // Displays the account management screen.
    virtual void ShowAccountManagementScreen(
        signin::SigninChoiceCallback on_account_management_screen_closed) = 0;
    // Invoked in cases we want to exit the flow early without showing the
    // history sync optin screen. Executes any steps that would normally occur
    // after the history sync optin screen (e.g. opening of the browser in the
    // profile picker flow).
    virtual void FinishFlowWithoutHistorySyncOptin() = 0;
  };

  static std::unique_ptr<HistorySyncOptinHelper> Create(
      signin::IdentityManager* identity_manager,
      Profile* profile,
      const AccountInfo& account_info,
      Delegate* delegate,
      LaunchContext launch_context);

  virtual ~HistorySyncOptinHelper();

  void StartHistorySyncOptinFlow();

  AccountStateFetcher* GetAccountStateFetcherForTesting() {
    return account_state_fetcher_.get();
  }

  SyncServiceStartupStateObserver*
  GetSyncServiceStartupStateObserverForTesting() {
    return sync_startup_state_observer_.get();
  }

 protected:
  HistorySyncOptinHelper(signin::IdentityManager* identity_manager,
                         Profile* profile,
                         const AccountInfo& account_info,
                         Delegate* delegate);

  void ResumeShowHistorySyncOptinScreenFlow(
      signin::Tribool maybe_managed_account);

  // Virtual methods for context-specific logic.
  virtual bool DetermineManagementStatusAndShowManagementScreens() = 0;

  void ShowHistorySyncOptinScreen();

  void SetProfile(Profile* profile) { profile_ = profile; }

  // Accessors.
  Profile* profile() { return profile_.get(); }
  const AccountInfo& account_info() const { return account_info_; }
  Delegate* delegate() { return delegate_.get(); }
  signin::Tribool maybe_managed_account() const {
    return maybe_managed_account_;
  }

 private:
  signin::Tribool AccountIsManaged(const AccountInfo& account_info);

  raw_ptr<Profile> profile_;
  const AccountInfo account_info_;
  raw_ptr<Delegate> delegate_;
  std::unique_ptr<AccountStateFetcher> account_state_fetcher_;

  std::unique_ptr<SyncServiceStartupStateObserver> sync_startup_state_observer_;
  signin::Tribool maybe_managed_account_ = signin::Tribool::kUnknown;
  base::WeakPtrFactory<HistorySyncOptinHelper> weak_ptr_factory_{this};
};

// `HistorySyncOptinHelper` implementation for the flow running in a browser
// window.
class HistorySyncOptinHelperInBrowser : public HistorySyncOptinHelper {
 public:
  HistorySyncOptinHelperInBrowser(signin::IdentityManager* identity_manager,
                                  Profile* profile,
                                  const AccountInfo& account_info,
                                  Delegate* delegate);
  ~HistorySyncOptinHelperInBrowser() override;

 private:
  // HistorySyncOptinHelper implementation:
  bool DetermineManagementStatusAndShowManagementScreens() override;

  void OnManagementAccepted(Profile* chosen_profile,
                            bool management_required_by_policy);

  raw_ptr<ProfileManagementDisclaimerService>
      profile_management_disclaimer_service_;

  base::WeakPtrFactory<HistorySyncOptinHelperInBrowser> weak_ptr_factory_{this};
};

// `HistorySyncOptinHelper` implementation for the flow running in the profile
// picker.
class HistorySyncOptinHelperInProfilePicker : public HistorySyncOptinHelper {
 public:
  HistorySyncOptinHelperInProfilePicker(
      signin::IdentityManager* identity_manager,
      Profile* profile,
      const AccountInfo& account_info,
      Delegate* delegate);
  ~HistorySyncOptinHelperInProfilePicker() override;

 private:
  // HistorySyncOptinHelper implementation:
  bool DetermineManagementStatusAndShowManagementScreens() override;

  void MaybeShowAccountManagementScreen(bool is_managed_account);
  void ShowAccountManagementScreen();
  void OnAccountManagementScreenClosed(signin::SigninChoice result);

  std::unique_ptr<HistorySyncOptinPolicyHelper> policy_helper_;

  base::WeakPtrFactory<HistorySyncOptinHelperInProfilePicker> weak_ptr_factory_{
      this};
};

#endif  // CHROME_BROWSER_UI_WEBUI_SIGNIN_HISTORY_SYNC_OPTIN_HELPER_H_
