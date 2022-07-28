// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_SIGNIN_FATAL_ERROR_SCREEN_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_SIGNIN_FATAL_ERROR_SCREEN_HANDLER_H_

#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "chrome/browser/ash/login/screens/signin_fatal_error_screen.h"
#include "chrome/browser/ui/webui/chromeos/login/base_screen_handler.h"

namespace chromeos {

// Interface for dependency injection between SignInFatalErrorScreen and its
// WebUI representation.
class SignInFatalErrorView
    : public base::SupportsWeakPtr<SignInFatalErrorView> {
 public:
  inline constexpr static StaticOobeScreenId kScreenId{
      "signin-fatal-error", "SignInFatalErrorScreen"};
  virtual ~SignInFatalErrorView() = default;

  // Shows the contents of the screen.
  virtual void Show(SignInFatalErrorScreen::Error error,
                    const base::Value* params) = 0;
};

class SignInFatalErrorScreenHandler : public SignInFatalErrorView,
                                      public BaseScreenHandler {
 public:
  using TView = SignInFatalErrorView;

  SignInFatalErrorScreenHandler();
  SignInFatalErrorScreenHandler(const SignInFatalErrorScreenHandler&) = delete;
  SignInFatalErrorScreenHandler& operator=(
      const SignInFatalErrorScreenHandler&) = delete;
  ~SignInFatalErrorScreenHandler() override;

 private:
  void Show(SignInFatalErrorScreen::Error error,
            const base::Value* params) override;

  // BaseScreenHandler:
  void DeclareLocalizedValues(
      ::login::LocalizedValuesBuilder* builder) override;
};

}  // namespace chromeos

// TODO(https://crbug.com/1164001): remove after the //chrome/browser/chromeos
// source migration is finished.
namespace ash {
using ::chromeos::SignInFatalErrorScreenHandler;
using ::chromeos::SignInFatalErrorView;
}

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_SIGNIN_FATAL_ERROR_SCREEN_HANDLER_H_
