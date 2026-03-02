// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SIGNIN_TURN_SYNC_ON_HELPER_DELEGATE_IMPL_H_
#define CHROME_BROWSER_UI_WEBUI_SIGNIN_TURN_SYNC_ON_HELPER_DELEGATE_IMPL_H_

#include "base/callback_list.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ui/webui/signin/login_ui_service.h"
#include "chrome/browser/ui/webui/signin/turn_sync_on_helper.h"

class Browser;
class Profile;
class SigninUIError;
class BrowserWindowInterface;
struct AccountInfo;

namespace policy {
class ProfileSeparationPolicies;
class UserCloudSigninRestrictionPolicyFetcher;
}  // namespace policy

// Default implementation for TurnSyncOnHelper::Delegate.
class TurnSyncOnHelperDelegateImpl : public TurnSyncOnHelper::Delegate,
                                     public LoginUIService::Observer {
 public:
  explicit TurnSyncOnHelperDelegateImpl(Browser* browser,
                                        bool is_sync_promo,
                                        bool user_already_signed_in = false);

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
  bool IsProfileCreationRequiredByPolicy() const override;
  void ShowLoginError(const SigninUIError& error) override;
  void ShowMergeSyncDataConfirmation(
      const std::string& previous_email,
      const std::string& new_email,
      signin::SigninChoiceCallback callback) override;
  void ShowSyncConfirmation(
      base::OnceCallback<void(LoginUIService::SyncConfirmationUIClosedResult)>
          callback) override;
  bool ShouldAbortBeforeShowSyncDisabledConfirmation() override;
  void ShowSyncDisabledConfirmation(
      bool is_managed_account,
      base::OnceCallback<void(LoginUIService::SyncConfirmationUIClosedResult)>
          callback) override;
  void ShowSyncSettings() override;
  void SwitchToProfile(Profile* new_profile) override;

  // LoginUIService::Observer:
  void OnSyncConfirmationUIClosed(
      LoginUIService::SyncConfirmationUIClosedResult result) override;

  void OnBrowserDidClose(BrowserWindowInterface* browser);

  void OnProfileSigninRestrictionsFetched(
      const AccountInfo& account_info,
      signin::SigninChoiceCallback callback,
      policy::ProfileSeparationPolicies profile_separation_policies);

  void OnProfileCheckComplete(const AccountInfo& account_info,
                              signin::SigninChoiceCallback callback,
                              bool prompt_for_new_profile);

  raw_ptr<Browser> browser_;
  raw_ptr<Profile> profile_;

  // Used to fetch the cloud user level policy value of
  // ManagedAccountsSigninRestriction. This can only fetch one policy value for
  // one account at the time.
  std::unique_ptr<policy::UserCloudSigninRestrictionPolicyFetcher>
      account_level_signin_restriction_policy_fetcher_;
  base::OnceCallback<void(LoginUIService::SyncConfirmationUIClosedResult)>
      sync_confirmation_callback_;
  base::ScopedObservation<LoginUIService, LoginUIService::Observer>
      scoped_login_ui_service_observation_{this};
  base::CallbackListSubscription browser_close_subscription_;
  const bool is_sync_promo_;
  const bool user_already_signed_in_;
  bool profile_creation_required_by_policy_ = false;

  base::WeakPtrFactory<TurnSyncOnHelperDelegateImpl> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_WEBUI_SIGNIN_TURN_SYNC_ON_HELPER_DELEGATE_IMPL_H_
