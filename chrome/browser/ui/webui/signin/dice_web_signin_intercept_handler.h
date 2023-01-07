// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SIGNIN_DICE_WEB_SIGNIN_INTERCEPT_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_SIGNIN_DICE_WEB_SIGNIN_INTERCEPT_HANDLER_H_

#include "content/public/browser/web_ui_message_handler.h"

#include <string>

#include "base/functional/callback.h"
#include "base/scoped_observation.h"
#include "base/values.h"
#include "chrome/browser/signin/dice_web_signin_interceptor.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"

// WebUI message handler for the Dice web signin intercept bubble.
class DiceWebSigninInterceptHandler : public content::WebUIMessageHandler,
                                      public signin::IdentityManager::Observer {
 public:
  DiceWebSigninInterceptHandler(
      const DiceWebSigninInterceptor::Delegate::BubbleParameters&
          bubble_parameters,
      base::OnceCallback<void(SigninInterceptionUserChoice)> callback);
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
  friend class DiceWebSigninInterceptHandlerTest;
  FRIEND_TEST_ALL_PREFIXES(DiceWebSigninInterceptHandlerTest,
                           GetInterceptionParametersValue);

  const AccountInfo& primary_account();
  const AccountInfo& intercepted_account();

  void HandleAccept(const base::Value::List& args);
  void HandleCancel(const base::Value::List& args);
  void HandleGuest(const base::Value::List& args);
  void HandlePageLoaded(const base::Value::List& args);

  // Gets the values sent to javascript.
  base::Value::Dict GetAccountInfoValue(const AccountInfo& info);
  base::Value::Dict GetInterceptionParametersValue();

  // The dialog string is different when the device is managed. This function
  // returns whether the version for managed devices should be used.
  bool ShouldShowManagedDeviceVersion();

  std::string GetHeaderText();
  std::string GetBodyTitle();
  std::string GetBodyText();
  std::string GetConfirmButtonLabel();
  std::string GetCancelButtonLabel();
  std::string GetManagedDisclaimerText();
  bool GetShouldUseV2Design();

  base::ScopedObservation<signin::IdentityManager,
                          signin::IdentityManager::Observer>
      identity_observation_{this};
  DiceWebSigninInterceptor::Delegate::BubbleParameters bubble_parameters_;

  base::OnceCallback<void(SigninInterceptionUserChoice)> callback_;
};

#endif  // CHROME_BROWSER_UI_WEBUI_SIGNIN_DICE_WEB_SIGNIN_INTERCEPT_HANDLER_H_
