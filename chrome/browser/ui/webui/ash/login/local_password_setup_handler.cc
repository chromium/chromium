// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/login/local_password_setup_handler.h"

#include "chrome/browser/ui/webui/ash/login/base_screen_handler.h"
#include "chrome/grit/generated_resources.h"
#include "components/login/localized_values_builder.h"
#include "ui/chromeos/devicetype_utils.h"

namespace ash {

LocalPasswordSetupHandler::LocalPasswordSetupHandler()
    : BaseScreenHandler(kScreenId) {}

LocalPasswordSetupHandler::~LocalPasswordSetupHandler() = default;

void LocalPasswordSetupHandler::Show(bool can_go_back, bool is_recovery_flow) {
  base::Value::Dict dict;
  dict.Set("showBackButton", can_go_back);
  dict.Set("isRecoveryFlow", is_recovery_flow);
  ShowInWebUI(std::move(dict));
}

base::WeakPtr<LocalPasswordSetupView> LocalPasswordSetupHandler::AsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void LocalPasswordSetupHandler::DeclareLocalizedValues(
    ::login::LocalizedValuesBuilder* builder) {
  const std::u16string device_name = ui::GetChromeOSDeviceName();

  builder->AddF("localPasswordSetupTitle", IDS_LOGIN_LOCAL_PASSWORD_SETUP_TITLE,
                device_name);
  builder->AddF("localPasswordSetupSubtitle",
                IDS_LOGIN_LOCAL_PASSWORD_SETUP_SUBTITLE, device_name);
  builder->AddF("localPasswordResetTitle", IDS_LOGIN_LOCAL_PASSWORD_RESET_TITLE,
                device_name);
  builder->Add("passwordInputPlaceholderText",
               IDS_LOGIN_MANUAL_PASSWORD_INPUT_LABEL);
  builder->Add("confirmPasswordInputPlaceholderText",
               IDS_LOGIN_CONFIRM_PASSWORD_LABEL);
  builder->Add("passwordMismatchError", IDS_LOGIN_MANUAL_PASSWORD_MISMATCH);
  builder->Add("showPassword", IDS_AUTH_SETUP_SHOW_PASSWORD);
  builder->Add("hidePassword", IDS_AUTH_SETUP_HIDE_PASSWORD);
  builder->Add("setLocalPasswordPlaceholder",
               IDS_AUTH_SETUP_SET_LOCAL_PASSWORD_PLACEHOLDER);
  builder->Add("setLocalPasswordConfirmPlaceholder",
               IDS_AUTH_SETUP_SET_LOCAL_PASSWORD_CONFIRM_PLACEHOLDER);
  builder->Add("setLocalPasswordMinCharsHint",
               IDS_AUTH_SETUP_SET_LOCAL_PASSWORD_MIN_CHARS_HINT);
  builder->Add("setLocalPasswordNoMatchError",
               IDS_AUTH_SETUP_SET_LOCAL_PASSWORD_NO_MATCH_ERROR);
}

void LocalPasswordSetupHandler::ShowLocalPasswordSetupFailure() {
  CallExternalAPI("showLocalPasswordSetupFailure");
}

}  // namespace ash
