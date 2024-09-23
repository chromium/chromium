// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/login/saml_confirm_password_handler.h"

#include "base/values.h"
#include "chrome/browser/ash/login/screens/saml_confirm_password_screen.h"
#include "chrome/browser/ui/webui/ash/login/base_screen_handler.h"
#include "chrome/grit/generated_resources.h"
#include "components/login/localized_values_builder.h"
#include "ui/chromeos/devicetype_utils.h"

namespace ash {

SamlConfirmPasswordHandler::SamlConfirmPasswordHandler()
    : BaseScreenHandler(kScreenId) {}

SamlConfirmPasswordHandler::~SamlConfirmPasswordHandler() = default;

void SamlConfirmPasswordHandler::Show(const std::string& email,
                                      bool is_manual) {
  base::Value::Dict data;
  data.Set("email", email);
  data.Set("manualPasswordInput", is_manual);
  ShowInWebUI(std::move(data));
}

void SamlConfirmPasswordHandler::ShowPasswordStep(bool retry) {
  CallExternalAPI("showPasswordStep", retry);
}

base::WeakPtr<SamlConfirmPasswordView> SamlConfirmPasswordHandler::AsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void SamlConfirmPasswordHandler::DeclareLocalizedValues(
    ::login::LocalizedValuesBuilder* builder) {
  builder->AddF("confirmPasswordTitle", IDS_LOGIN_CONFIRM_PASSWORD_TITLE,
                ui::GetChromeOSDeviceName());
  builder->Add("manualPasswordTitle", IDS_LOGIN_MANUAL_PASSWORD_TITLE);
  builder->Add("manualPasswordInputLabel",
               IDS_LOGIN_MANUAL_PASSWORD_INPUT_LABEL);
  builder->Add("manualPasswordMismatch", IDS_LOGIN_MANUAL_PASSWORD_MISMATCH);
  builder->Add("confirmPasswordLabel", IDS_LOGIN_CONFIRM_PASSWORD_LABEL);
  builder->Add("confirmPasswordIncorrectPassword",
               IDS_LOGIN_CONFIRM_PASSWORD_INCORRECT_PASSWORD);
  builder->Add("accountSetupCancelDialogTitle",
               IDS_LOGIN_ACCOUNT_SETUP_CANCEL_DIALOG_TITLE);
  builder->Add("accountSetupCancelDialogNo",
               IDS_LOGIN_ACCOUNT_SETUP_CANCEL_DIALOG_NO);
  builder->Add("accountSetupCancelDialogYes",
               IDS_LOGIN_ACCOUNT_SETUP_CANCEL_DIALOG_YES);
}

}  // namespace ash
