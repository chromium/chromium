// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SIGNIN_DICE_TURN_SYNC_ON_HELPER_DELEGATE_IMPL_H_
#define CHROME_BROWSER_UI_WEBUI_SIGNIN_DICE_TURN_SYNC_ON_HELPER_DELEGATE_IMPL_H_

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/scoped_observer.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "chrome/browser/ui/sync/profile_signin_confirmation_helper.h"
#include "chrome/browser/ui/webui/signin/dice_turn_sync_on_helper.h"
#include "chrome/browser/ui/webui/signin/login_ui_service.h"

class Browser;
class Profile;

// Default implementation for DiceTurnSyncOnHelper::Delegate.
class DiceTurnSyncOnHelperDelegateImpl : public DiceTurnSyncOnHelper::Delegate,
                                         public BrowserListObserver,
                                         public LoginUIService::Observer {
 public:
  explicit DiceTurnSyncOnHelperDelegateImpl(Browser* browser);
  ~DiceTurnSyncOnHelperDelegateImpl() override;

 private:
  // User input handler for the signin confirmation dialog.
  class SigninDialogDelegate : public ui::ProfileSigninConfirmationDelegate {
   public:
    explicit SigninDialogDelegate(
        DiceTurnSyncOnHelper::SigninChoiceCallback callback);
    ~SigninDialogDelegate() override;
    void OnCancelSignin() override;
    void OnContinueSignin() override;
    void OnSigninWithNewProfile() override;

   private:
    DiceTurnSyncOnHelper::SigninChoiceCallback callback_;

    DISALLOW_COPY_AND_ASSIGN(SigninDialogDelegate);
  };

  // DiceTurnSyncOnHelper::Delegate:
  void ShowLoginError(const std::string& email,
                      const std::string& error_message) override;
  void ShowMergeSyncDataConfirmation(
      const std::string& previous_email,
      const std::string& new_email,
      DiceTurnSyncOnHelper::SigninChoiceCallback callback) override;
  void ShowEnterpriseAccountConfirmation(
      const std::string& email,
      DiceTurnSyncOnHelper::SigninChoiceCallback callback) override;
  void ShowSyncConfirmation(
      base::OnceCallback<void(LoginUIService::SyncConfirmationUIClosedResult)>
          callback) override;
  void ShowSyncSettings() override;
  void SwitchToProfile(Profile* new_profile) override;

  // LoginUIService::Observer:
  void OnSyncConfirmationUIClosed(
      LoginUIService::SyncConfirmationUIClosedResult result) override;

  // BrowserListObserver:
  void OnBrowserRemoved(Browser* browser) override;

  Browser* browser_;
  Profile* profile_;
  base::OnceCallback<void(LoginUIService::SyncConfirmationUIClosedResult)>
      sync_confirmation_callback_;
  ScopedObserver<LoginUIService, LoginUIService::Observer>
      scoped_login_ui_service_observer_{this};

  DISALLOW_COPY_AND_ASSIGN(DiceTurnSyncOnHelperDelegateImpl);
};

#endif  // CHROME_BROWSER_UI_WEBUI_SIGNIN_DICE_TURN_SYNC_ON_HELPER_DELEGATE_IMPL_H_
