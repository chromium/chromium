// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_SIGNIN_FATAL_ERROR_SCREEN_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_SIGNIN_FATAL_ERROR_SCREEN_HANDLER_H_

#include <string>

#include "base/values.h"
#include "chrome/browser/ash/login/screens/signin_fatal_error_screen.h"
#include "chrome/browser/ui/webui/chromeos/login/base_screen_handler.h"

namespace chromeos {

class SignInFatalErrorScreen;

// Interface for dependency injection between SignInFatalErrorScreen and its
// WebUI representation.
class SignInFatalErrorView {
 public:
  constexpr static StaticOobeScreenId kScreenId{"signin-fatal-error"};
  virtual ~SignInFatalErrorView() = default;

  // Shows the contents of the screen.
  virtual void Show(SignInFatalErrorScreen::Error error,
                    const base::Value* params) = 0;

  // Binds `screen` to the view.
  virtual void Bind(SignInFatalErrorScreen* screen) = 0;

  // Unbinds the screen from the view.
  virtual void Unbind() = 0;
};

class SignInFatalErrorScreenHandler : public SignInFatalErrorView,
                                      public BaseScreenHandler {
 public:
  using TView = SignInFatalErrorView;

  explicit SignInFatalErrorScreenHandler(JSCallsContainer* js_calls_container);
  SignInFatalErrorScreenHandler(const SignInFatalErrorScreenHandler&) = delete;
  SignInFatalErrorScreenHandler& operator=(
      const SignInFatalErrorScreenHandler&) = delete;
  ~SignInFatalErrorScreenHandler() override;

 private:
  void Show(SignInFatalErrorScreen::Error error,
            const base::Value* params) override;
  void Bind(SignInFatalErrorScreen* screen) override;
  void Unbind() override;

  // BaseScreenHandler:
  void DeclareLocalizedValues(
      ::login::LocalizedValuesBuilder* builder) override;
  void Initialize() override;

  SignInFatalErrorScreen* screen_ = nullptr;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_SIGNIN_FATAL_ERROR_SCREEN_HANDLER_H_
