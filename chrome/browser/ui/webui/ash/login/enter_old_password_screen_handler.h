// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_ENTER_OLD_PASSWORD_SCREEN_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_ENTER_OLD_PASSWORD_SCREEN_HANDLER_H_

#include "chrome/browser/ash/login/oobe_screen.h"
#include "chrome/browser/ui/webui/ash/login/base_screen_handler.h"

namespace ash {

class EnterOldPasswordScreenView
    : public base::SupportsWeakPtr<EnterOldPasswordScreenView> {
 public:
  inline constexpr static StaticOobeScreenId kScreenId{
      "enter-old-password", "EnterOldPasswordScreen"};

  EnterOldPasswordScreenView() = default;

  EnterOldPasswordScreenView(const EnterOldPasswordScreenView&) = delete;
  EnterOldPasswordScreenView& operator=(const EnterOldPasswordScreenView&) =
      delete;

  virtual void Show() = 0;
  virtual void ShowWrongPasswordError() = 0;
};

// A class that handles WebUI hooks in Gaia screen.
class EnterOldPasswordScreenHandler : public BaseScreenHandler,
                                      public EnterOldPasswordScreenView {
 public:
  using TView = EnterOldPasswordScreenView;

  EnterOldPasswordScreenHandler();

  EnterOldPasswordScreenHandler(const EnterOldPasswordScreenHandler&) = delete;
  EnterOldPasswordScreenHandler& operator=(
      const EnterOldPasswordScreenHandler&) = delete;

  ~EnterOldPasswordScreenHandler() override;

 private:
  // EnterOldPasswordView:
  void Show() override;
  void ShowWrongPasswordError() override;

  // BaseScreenHandler:
  void DeclareLocalizedValues(::login::LocalizedValuesBuilder* builder) final;
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_ENTER_OLD_PASSWORD_SCREEN_HANDLER_H_
