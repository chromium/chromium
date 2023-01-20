// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/login/gaia_password_changed_screen_handler.h"

#include "ash/constants/ash_features.h"
#include "base/values.h"
#include "chrome/browser/ash/login/oobe_screen.h"
#include "chrome/browser/ash/login/screens/gaia_password_changed_screen.h"
#include "chrome/browser/ash/login/ui/login_display_host.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/login/localized_values_builder.h"
#include "ui/chromeos/devicetype_utils.h"

namespace ash {

GaiaPasswordChangedScreenHandler::GaiaPasswordChangedScreenHandler()
    : BaseScreenHandler(kScreenId) {}

GaiaPasswordChangedScreenHandler::~GaiaPasswordChangedScreenHandler() = default;

void GaiaPasswordChangedScreenHandler::DeclareLocalizedValues(
    ::login::LocalizedValuesBuilder* builder) {
  builder->Add("nextButtonText", IDS_OFFLINE_LOGIN_NEXT_BUTTON_TEXT);
  builder->Add("oldPasswordHint", IDS_LOGIN_PASSWORD_CHANGED_OLD_PASSWORD_HINT);
  builder->Add("oldPasswordIncorrect",
               IDS_LOGIN_PASSWORD_CHANGED_INCORRECT_OLD_PASSWORD);
  builder->Add("proceedAnywayButton",
               IDS_LOGIN_PASSWORD_CHANGED_PROCEED_ANYWAY_BUTTON);
  builder->Add("forgotOldPasswordButtonText",
               IDS_LOGIN_PASSWORD_CHANGED_FORGOT_PASSWORD);
  builder->AddF("passwordChangedTitle", IDS_LOGIN_PASSWORD_CHANGED_TITLE,
                ui::GetChromeOSDeviceName());
  builder->Add("passwordChangedProceedAnywayTitle",
               IDS_LOGIN_PASSWORD_CHANGED_PROCEED_ANYWAY);
  builder->Add("passwordChangedTryAgain", IDS_LOGIN_PASSWORD_CHANGED_TRY_AGAIN);
  builder->Add("dataLossWarningTitle",
               IDS_LOGIN_PASSWORD_CHANGED_DATA_LOSS_WARNING_TITLE);
  builder->Add("dataLossWarningSubtitleP1",
               IDS_LOGIN_PASSWORD_CHANGED_DATA_LOSS_WARNING_SUBTITLE_P1);
  builder->Add("dataLossWarningSubtitleP2",
               IDS_LOGIN_PASSWORD_CHANGED_DATA_LOSS_WARNING_SUBTITLE_P2);
  builder->Add("recoverLocalDataTitle",
               IDS_LOGIN_PASSWORD_CHANGED_RECOVER_DATA_TITLE);
  builder->Add("recoverLocalDataSubtitle",
               IDS_LOGIN_PASSWORD_CHANGED_RECOVER_DATA_SUBTITLE);
  builder->Add("continueAndDeleteDataButton",
               IDS_LOGIN_PASSWORD_CHANGED_CONTINUE_AND_DELETE_BUTTON);
  builder->Add("continueWithoutLocalDataButton",
               IDS_LOGIN_PASSWORD_CHANGED_CONTINUE_WITHOUT_LOCAL_DATA_BUTTON);
}

void GaiaPasswordChangedScreenHandler::GetAdditionalParameters(
    base::Value::Dict* dict) {
  dict->Set("isCryptohomeRecoveryUIFlowEnabled",
            features::IsCryptohomeRecoveryFlowEnabled());
  BaseScreenHandler::GetAdditionalParameters(dict);
}

void GaiaPasswordChangedScreenHandler::Show(const std::string& email,
                                            bool has_error) {
  base::Value::Dict data;
  data.Set("email", email);
  data.Set("showError", has_error);
  ShowInWebUI(std::move(data));
}

}  // namespace ash
