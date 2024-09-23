// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SIGNIN_DICE_WEB_SIGNIN_INTERCEPT_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_SIGNIN_DICE_WEB_SIGNIN_INTERCEPT_HANDLER_H_

#include <string>

#include "base/functional/callback.h"
#include "base/gtest_prod_util.h"
#include "base/scoped_observation.h"
#include "base/values.h"
#include "chrome/browser/signin/dice_web_signin_interceptor.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "content/public/browser/web_ui_message_handler.h"

// WebUI message handler for the Dice web signin intercept bubble.
class DiceWebSigninInterceptHandler : public content::WebUIMessageHandler,
                                      public signin::IdentityManager::Observer {
 public:
  DiceWebSigninInterceptHandler(
      const WebSigninInterceptor::Delegate::BubbleParameters& bubble_parameters,
      base::OnceCallback<void(int)> show_widget_with_height_callback,
      base::OnceCallback<void(SigninInterceptionUserChoice)>
          completion_callback);
  ~DiceWebSigninInterceptHandler() override;

  DiceWebSigninInterceptHandler(const DiceWebSigninInterceptHandler&) = delete;
  DiceWebSigninInterceptHandler& operator=(
      const DiceWebSigninInterceptHandler&) = delete;

  // content::WebUIMessageHandler:
  void RegisterMessages() override;
  void OnJavascriptAllowed() override;
  void OnJavascriptDisallowed() override;

  // signin::IdentityManager::Observer
  void OnExtendedAccountInfoUpdated(const AccountInfo& info) override;

 private:
  friend class DiceWebSigninInterceptHandlerTestBase;
  const AccountInfo& primary_account();
  const AccountInfo& intercepted_account();

  void HandleAccept(const base::Value::List& args);
  void HandleCancel(const base::Value::List& args);
  void HandlePageLoaded(const base::Value::List& args);
  void HandleInitializedWithHeight(const base::Value::List& args);
  void HandleChromeSigninPageLoaded(const base::Value::List& args);

  // Gets the values sent to javascript.
  base::Value::Dict GetInterceptionParametersValue();
  // Get the values for ChromeSignin bubble sent to javascript.
  base::Value::Dict GetInterceptionChromeSigninParametersValue();

  // The dialog string is different when the device is managed. This function
  // returns whether the version for managed devices should be used.
  bool ShouldShowManagedDeviceVersion();

  std::string GetHeaderText();
  std::string GetChromeSigninTitle();
  std::string GetChromeSigninSubtitle();
  std::string GetBodyTitle();
  std::string GetBodyText();
  std::string GetConfirmButtonLabel();
  std::string GetCancelButtonLabel();
  std::string GetManagedDisclaimerText();
  bool GetShouldUseV2Design();

  void UpdateExtendedAccountsInfo();

  base::ScopedObservation<signin::IdentityManager,
                          signin::IdentityManager::Observer>
      identity_observation_{this};
  WebSigninInterceptor::Delegate::BubbleParameters bubble_parameters_;

  base::OnceCallback<void(int)> show_widget_with_height_callback_;
  base::OnceCallback<void(SigninInterceptionUserChoice)> completion_callback_;
};

#endif  // CHROME_BROWSER_UI_WEBUI_SIGNIN_DICE_WEB_SIGNIN_INTERCEPT_HANDLER_H_
