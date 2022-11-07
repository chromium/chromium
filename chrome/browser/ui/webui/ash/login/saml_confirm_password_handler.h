// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_SAML_CONFIRM_PASSWORD_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_SAML_CONFIRM_PASSWORD_HANDLER_H_

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/login/oobe_screen.h"
#include "chrome/browser/ui/webui/ash/login/base_screen_handler.h"

namespace ash {

class SamlConfirmPasswordView
    : public base::SupportsWeakPtr<SamlConfirmPasswordView> {
 public:
  inline constexpr static StaticOobeScreenId kScreenId{
      "saml-confirm-password", "ConfirmSamlPasswordScreen"};

  SamlConfirmPasswordView() = default;

  SamlConfirmPasswordView(const SamlConfirmPasswordView&) = delete;
  SamlConfirmPasswordView& operator=(const SamlConfirmPasswordView&) = delete;

  virtual void Show(const std::string& email, bool is_manual) = 0;
  virtual void ShowPasswordStep(bool retry) = 0;
};

// A class that handles WebUI hooks in Gaia screen.
class SamlConfirmPasswordHandler : public BaseScreenHandler,
                                   public SamlConfirmPasswordView {
 public:
  using TView = SamlConfirmPasswordView;

  SamlConfirmPasswordHandler();

  SamlConfirmPasswordHandler(const SamlConfirmPasswordHandler&) = delete;
  SamlConfirmPasswordHandler& operator=(const SamlConfirmPasswordHandler&) =
      delete;

  ~SamlConfirmPasswordHandler() override;

  // SamlConfirmPasswordView:
  void Show(const std::string& email, bool is_manual) override;
  void ShowPasswordStep(bool retry) override;

  // BaseScreenHandler:
  void DeclareLocalizedValues(::login::LocalizedValuesBuilder* builder) final;
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_SAML_CONFIRM_PASSWORD_HANDLER_H_
