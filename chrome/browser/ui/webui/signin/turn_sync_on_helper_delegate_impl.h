// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SIGNIN_TURN_SYNC_ON_HELPER_DELEGATE_IMPL_H_
#define CHROME_BROWSER_UI_WEBUI_SIGNIN_TURN_SYNC_ON_HELPER_DELEGATE_IMPL_H_

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "chrome/browser/ui/webui/signin/login_ui_service.h"
#include "chrome/browser/ui/webui/signin/turn_sync_on_helper.h"

class Browser;
class Profile;
class SigninUIError;
struct AccountInfo;

namespace policy {
class ProfileSeparationPolicies;
class UserCloudSigninRestrictionPolicyFetcher;
}

// Default implementation for TurnSyncOnHelper::Delegate.
class TurnSyncOnHelperDelegateImpl : public TurnSyncOnHelper::Delegate,
                                     public BrowserListObserver,
                                     public LoginUIService::Observer {
 public:
  explicit TurnSyncOnHelperDelegateImpl(Browser* browser, bool is_sync_promo);

  TurnSyncOnHelperDelegateImpl(const TurnSyncOnHelperDelegateImpl&) = delete;
  TurnSyncOnHelperDelegateImpl& operator=(const TurnSyncOnHelperDelegateImpl&) =
      delete;

  ~TurnSyncOnHelperDelegateImpl() override;

 protected:
  void ShowEnterpriseAccountConfirmation(
      const AccountInfo& account_info,
      signin::SigninChoiceCallback callback) override;
  virtual void ShouldEnterpriseConfirmationPromptForNewProfile(
      Profile* profile,
      base::OnceCallback<void(bool)> callback);

 private:
  // TurnSyncOnHelper::Delegate:
  void ShowLoginError(const SigninUIError& error) override;
  void ShowMergeSyncDataConfirmation(
      const std::string& previous_email,
      const std::string& new_email,
      signin::SigninChoiceCallback callback) override;
  void ShowSyncConfirmation(
      base::OnceCallback<void(LoginUIService::SyncConfirmationUIClosedResult)>
          callback) override;
  void ShowSyncDisabledConfirmation(
      bool is_managed_account,
      base::OnceCallback<void(LoginUIService::SyncConfirmationUIClosedResult)>
          callback) override;
  void ShowSyncSettings() override;
  void SwitchToProfile(Profile* new_profile) override;

  // LoginUIService::Observer:
  void OnSyncConfirmationUIClosed(
      LoginUIService::SyncConfirmationUIClosedResult result) override;

  // BrowserListObserver:
  void OnBrowserRemoved(Browser* browser) override;

#if !BUILDFLAG(IS_CHROMEOS_LACROS)
  void OnProfileSigninRestrictionsFetched(
      const AccountInfo& account_info,
      signin::SigninChoiceCallback callback,
      const policy::ProfileSeparationPolicies& profile_separation_policies);
#endif  //! BUILDFLAG(IS_CHROMEOS_LACROS)

  void OnProfileCheckComplete(const AccountInfo& account_info,
                              signin::SigninChoiceCallback callback,
                              bool prompt_for_new_profile);

  raw_ptr<Browser> browser_;
  raw_ptr<Profile> profile_;

#if !BUILDFLAG(IS_CHROMEOS_LACROS)
  // Used to fetch the cloud user level policy value of
  // ManagedAccountsSigninRestriction. This can only fetch one policy value for
  // one account at the time.
  std::unique_ptr<policy::UserCloudSigninRestrictionPolicyFetcher>
      account_level_signin_restriction_policy_fetcher_;
#endif  // !BUILDFLAG(IS_CHROMEOS_LACROS)
  base::OnceCallback<void(LoginUIService::SyncConfirmationUIClosedResult)>
      sync_confirmation_callback_;
  base::ScopedObservation<LoginUIService, LoginUIService::Observer>
      scoped_login_ui_service_observation_{this};
  const bool is_sync_promo_;

  base::WeakPtrFactory<TurnSyncOnHelperDelegateImpl> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_WEBUI_SIGNIN_TURN_SYNC_ON_HELPER_DELEGATE_IMPL_H_
