// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SIGNIN_TURN_SYNC_ON_HELPER_H_
#define CHROME_BROWSER_UI_WEBUI_SIGNIN_TURN_SYNC_ON_HELPER_H_

#include <memory>
#include <string>

#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/timer/elapsed_timer.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engine_choice/search_engine_choice_dialog_service.h"
#include "chrome/browser/sync/sync_startup_tracker.h"
#include "chrome/browser/ui/webui/signin/login_ui_service.h"
#include "chrome/browser/ui/webui/signin/signin_utils.h"
#include "components/signin/public/base/signin_buildflags.h"
#include "components/signin/public/base/signin_metrics.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "google_apis/gaia/core_account_id.h"

#if !BUILDFLAG(ENABLE_DICE_SUPPORT) && !BUILDFLAG(ENABLE_MIRROR)
#error "This file should only be included if DICE support / mirror is enabled"
#endif

class Browser;
class SigninUIError;
class TurnSyncOnHelperPolicyFetchTracker;
class AccountSelectionInProgressHandle;

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
class DiceSignedInProfileCreator;
#endif

#if BUILDFLAG(IS_CHROMEOS_LACROS)
class ProfilePickerLacrosSignInProvider;
#endif

namespace signin {
class IdentityManager;
}

namespace syncer {
class SyncService;
class SyncSetupInProgressHandle;
}  // namespace syncer

// Handles details of setting the primary account with IdentityManager and
// turning on sync for an account for which there is already a refresh token.
class TurnSyncOnHelper {
 public:
  // Behavior when the signin is aborted (by an error or cancelled by the user).
  // The mode has no effect on the sync-is-disabled flow where cancelling always
  // implies removing the account.
  enum class SigninAbortedMode {
    // The token is revoked and the account is signed out of the web.
    REMOVE_ACCOUNT,
    // The account is kept as primary account in Chrome and on the web.
    KEEP_ACCOUNT,
    // The primary account is cleared, but the account is kept on the web only.
    KEEP_ACCOUNT_ON_WEB_ONLY,
  };

  // Delegate implementing the UI prompts.
  class Delegate {
   public:
    virtual ~Delegate() {}

    // Shows a login error to the user.
    virtual void ShowLoginError(const SigninUIError& error) = 0;

    // Shows a confirmation dialog when the user was previously signed in with a
    // different account in the same profile. |callback| must be called.
    virtual void ShowMergeSyncDataConfirmation(
        const std::string& previous_email,
        const std::string& new_email,
        signin::SigninChoiceCallback callback) = 0;

    // Shows a confirmation dialog when the user is signing in a managed
    // account. |callback| must be called.
    // NOTE: When this is called, any subsequent call to
    // ShowSync(Disabled)Confirmation will have is_managed_account set to true.
    // The other implication is only partially true: for a managed account,
    // ShowEnterpriseAccountConfirmation() must be called before calling
    // ShowSyncConfirmation() but it does not have to be called before calling
    // ShowSyncDisabledConfirmation(). Namely, Chrome can have clarity about
    // sync being disabled even before fetching enterprise policies (e.g. sync
    // engine gets a 'disabled-by-enterprise' error from the server).
    virtual void ShowEnterpriseAccountConfirmation(
        const AccountInfo& account_info,
        signin::SigninChoiceCallback callback) = 0;

    // Shows a sync confirmation screen offering to open the Sync settings.
    // |callback| must be called.
    // NOTE: The account is managed iff ShowEnterpriseAccountConfirmation() has
    // been called before.
    virtual void ShowSyncConfirmation(
        base::OnceCallback<void(LoginUIService::SyncConfirmationUIClosedResult)>
            callback) = 0;

    // Whether the delegate wants to silently abort the turn sync on process
    // when the sync is disabled for the user before showing the sync disabled
    // UI.
    // This can be used in cases when the turn sync on is triggered not by the
    // user action but a promo process.
    // Defaults to false.
    virtual bool ShouldAbortBeforeShowSyncDisabledConfirmation();

    // Shows a screen informing that sync is disabled for the user.
    // |is_managed_account| is true if the account (where sync is being set up)
    // is managed (which may influence the UI or strings). |callback| must be
    // called.
    // TODO(crbug.com/40249681): Use a new enum for this callback with only
    // values that make sense here (stay signed-in / signout).
    virtual void ShowSyncDisabledConfirmation(
        bool is_managed_account,
        base::OnceCallback<void(LoginUIService::SyncConfirmationUIClosedResult)>
            callback) = 0;

    // Opens the Sync settings page.
    virtual void ShowSyncSettings() = 0;

    // Informs the delegate that the flow is switching to a new profile.
    virtual void SwitchToProfile(Profile* new_profile) = 0;

    // Shows the `error` for `browser`.
    // This helper is static because in some cases it needs to be called
    // after this object gets destroyed.
    static void ShowLoginErrorForBrowser(const SigninUIError& error,
                                         Browser* browser);
  };

  // Create a helper that turns sync on for an account that is already present
  // in the token service.
  // |callback| is called at the end of the flow (i.e. after the user closes the
  // sync confirmation dialog).
  TurnSyncOnHelper(Profile* profile,
                   signin_metrics::AccessPoint signin_access_point,
                   signin_metrics::PromoAction signin_promo_action,
                   const CoreAccountId& account_id,
                   SigninAbortedMode signin_aborted_mode,
                   std::unique_ptr<Delegate> delegate,
                   base::OnceClosure callback);

  // Convenience constructor using the default delegate and empty callback.
  // `is_sync_promo` is true if the sync confirmation dialog is offered as an
  // option. It is false if the user explicitly initiated the flow.
  TurnSyncOnHelper(Profile* profile,
                   Browser* browser,
                   signin_metrics::AccessPoint signin_access_point,
                   signin_metrics::PromoAction signin_promo_action,
                   const CoreAccountId& account_id,
                   SigninAbortedMode signin_aborted_mode,
                   bool is_sync_promo);

  TurnSyncOnHelper(const TurnSyncOnHelper&) = delete;
  TurnSyncOnHelper& operator=(const TurnSyncOnHelper&) = delete;

  // Fakes that sync enabled for testing, but does not create a sync service.
  static void SetShowSyncEnabledUiForTesting(
      bool show_sync_enabled_ui_for_testing);

  // Returns true if a `TurnSyncOnHelper` is currently active for `profile`.
  static bool HasCurrentTurnSyncOnHelperForTesting(Profile* profile);

  // Used as callback for `SyncStartupTracker`.
  // Public for testing.
  void OnSyncStartupStateChanged(SyncStartupTracker::ServiceStartupState state);

  static void EnsureFactoryBuilt();

 private:
  enum class ProfileMode {
    // Attempts to sign the user in |profile_|. Note that if the account to be
    // signed in is a managed account, then a profile confirmation dialog is
    // shown and the user has the possibility to create a new profile before
    // signing in.
    CURRENT_PROFILE,

    // Creates a new profile and signs the user in this new profile.
    NEW_PROFILE
  };

  // TurnSyncOnHelper deletes itself.
  ~TurnSyncOnHelper();

  // Triggers the start of the flow.
  void TurnSyncOnInternal();

  // Handles can offer sign-in errors.  It returns true if there is an error,
  // and false otherwise.
  bool HasCanOfferSigninError();

  // Used as callback for ShowMergeSyncDataConfirmation().
  void OnMergeAccountConfirmation(signin::SigninChoice choice);

  // Used as callback for ShowEnterpriseAccountConfirmation().
  void OnEnterpriseAccountConfirmation(signin::SigninChoice choice);

  // Turns sync on with the current profile or a new profile.
  void TurnSyncOnWithProfileMode(ProfileMode profile_mode);

  // Callback invoked once policy registration is complete.
  void OnRegisteredForPolicy(bool is_account_managed);

  // Helper function that loads policy with the cached |dm_token_| and
  // |client_id|, then completes the signin process.
  void LoadPolicyWithCachedCredentials();

  // Callback invoked when a policy fetch request has completed. |success| is
  // true if policy was successfully fetched.
  void OnPolicyFetchComplete(bool success);

  // Called to create a new profile, which is then signed in with the
  // in-progress auth credentials currently stored in this object.
  void CreateNewSignedInProfile();

  // Called when the new profile is created.
  void OnNewSignedInProfileCreated(
      search_engines::ChoiceData search_engine_choice_data,
      Profile* new_profile);

  // Returns the SyncService, or nullptr if sync is not allowed.
  syncer::SyncService* GetSyncService();

  // Completes the signin in IdentityManager and displays the Sync confirmation
  // UI.
  void SigninAndShowSyncConfirmationUI();

  // Displays the Sync confirmation UI.
  // Note: If sync fails to start (e.g. sync is disabled by admin), the sync
  // confirmation dialog will be updated accordingly.
  void ShowSyncConfirmationUI();

  // Handles the user input from the sync confirmation UI and deletes this
  // object.
  void FinishSyncSetupAndDelete(
      LoginUIService::SyncConfirmationUIClosedResult result);

  // Switch to a new profile after exporting the token.
  void SwitchToProfile(Profile* new_profile);

  // Only one TurnSyncOnHelper can be attached per profile. This deletes
  // any other helper attached to the profile.
  void AttachToProfile();

  // Aborts the flow and deletes this object.
  void AbortAndDelete();

  // Removes the account on abort taking into consideration if it is the primary
  // account.
  void RemoveAccount();

  std::unique_ptr<Delegate> delegate_;
  raw_ptr<Profile> profile_;
  raw_ptr<signin::IdentityManager> identity_manager_;
  const signin_metrics::AccessPoint signin_access_point_;
  const signin_metrics::PromoAction signin_promo_action_;

  // Whether the refresh token should be deleted if the Sync flow is aborted.
  SigninAbortedMode signin_aborted_mode_;

  // Account information.
  const AccountInfo account_info_;

  // Prevents Sync from running until configuration is complete.
  std::unique_ptr<syncer::SyncSetupInProgressHandle> sync_blocker_;

  // Prevents `SigninManager` from changing the unconsented primary account
  // until the flow is complete.
  std::unique_ptr<AccountSelectionInProgressHandle> account_change_blocker_;

  // Called when this object is deleted.
  base::ScopedClosureRunner scoped_callback_runner_;

  std::unique_ptr<SyncStartupTracker> sync_startup_tracker_;
  std::unique_ptr<TurnSyncOnHelperPolicyFetchTracker> policy_fetch_tracker_;
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  std::unique_ptr<DiceSignedInProfileCreator> dice_signed_in_profile_creator_;
#endif
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  std::unique_ptr<ProfilePickerLacrosSignInProvider> lacros_sign_in_provider_;
#endif

  // The initial primary account is restored if the flow aborts. This is only
  // needed on Lacros or if UNO Desktop is enabled, because the `SigninManager`
  // does it automatically on DICE platforms.
  CoreAccountId initial_primary_account_;
  base::CallbackListSubscription shutdown_subscription_;
  bool enterprise_account_confirmed_ = false;

  // The time at which all user input has been collected, prior to this helper
  // running heuristics for displaying the sync consent screen.
  //
  // When in the flow this is set depends on the properties - for example it
  // could be:
  // * At the start of the flow
  // * After the user completes the user merge choice dialog
  // * After the user acknowledge enterprise management
  //
  // Used for metrics, to output the timing histograms.
  std::optional<base::ElapsedTimer> user_input_complete_timer_;

  base::WeakPtrFactory<TurnSyncOnHelper> weak_pointer_factory_{this};
};

#endif  // CHROME_BROWSER_UI_WEBUI_SIGNIN_TURN_SYNC_ON_HELPER_H_
