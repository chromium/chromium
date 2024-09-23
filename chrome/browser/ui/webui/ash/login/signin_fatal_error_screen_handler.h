// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_SIGNIN_FATAL_ERROR_SCREEN_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_SIGNIN_FATAL_ERROR_SCREEN_HANDLER_H_

#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "chrome/browser/ash/login/screens/signin_fatal_error_screen.h"
#include "chrome/browser/ui/webui/ash/login/base_screen_handler.h"

namespace ash {

// Interface for dependency injection between SignInFatalErrorScreen and its
// WebUI representation.
class SignInFatalErrorView {
 public:
  inline constexpr static StaticOobeScreenId kScreenId{
      "signin-fatal-error", "SignInFatalErrorScreen"};
  virtual ~SignInFatalErrorView() = default;

  // Shows the contents of the screen.
  virtual void Show(SignInFatalErrorScreen::Error error,
                    const base::Value::Dict& params) = 0;

  // Gets a WeakPtr to the instance.
  virtual base::WeakPtr<SignInFatalErrorView> AsWeakPtr() = 0;
};

class SignInFatalErrorScreenHandler final : public SignInFatalErrorView,
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
            const base::Value::Dict& params) override;
  base::WeakPtr<SignInFatalErrorView> AsWeakPtr() override;

  // BaseScreenHandler:
  void DeclareLocalizedValues(
      ::login::LocalizedValuesBuilder* builder) override;

  base::WeakPtrFactory<SignInFatalErrorView> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_SIGNIN_FATAL_ERROR_SCREEN_HANDLER_H_
