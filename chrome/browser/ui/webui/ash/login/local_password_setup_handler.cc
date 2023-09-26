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

void LocalPasswordSetupHandler::Show() {
  ShowInWebUI();
}

void LocalPasswordSetupHandler::DeclareLocalizedValues(
    ::login::LocalizedValuesBuilder* builder) {
  builder->AddF("localPasswordSetupTitle", IDS_LOGIN_LOCAL_PASSWORD_SETUP_TITLE,
                ui::GetChromeOSDeviceName());
  builder->Add("passwordInputPlaceholderText",
               IDS_LOGIN_MANUAL_PASSWORD_INPUT_LABEL);
  builder->Add("confirmPasswordInputPlaceholderText",
               IDS_LOGIN_CONFIRM_PASSWORD_LABEL);
  builder->Add("passwordMismatchError", IDS_LOGIN_MANUAL_PASSWORD_MISMATCH);
  builder->Add("setLocalPasswordPlaceholder",
               IDS_AUTH_SETUP_SET_LOCAL_PASSWORD_PLACEHOLDER);
  builder->Add("setLocalPasswordConfirmPlaceholder",
               IDS_AUTH_SETUP_SET_LOCAL_PASSWORD_CONFIRM_PLACEHOLDER);
  builder->Add("setLocalPasswordMinCharsHint",
               IDS_AUTH_SETUP_SET_LOCAL_PASSWORD_MIN_CHARS_HINT);
  builder->Add("setLocalPasswordNoMatchError",
               IDS_AUTH_SETUP_SET_LOCAL_PASSWORD_NO_MATCH_ERROR);
}

}  // namespace ash
