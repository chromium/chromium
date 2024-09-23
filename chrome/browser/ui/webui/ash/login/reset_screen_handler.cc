// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/login/reset_screen_handler.h"

#include <string>

#include "base/values.h"
#include "chrome/browser/ash/login/oobe_screen.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/login/localized_values_builder.h"
#include "components/strings/grit/components_strings.h"
#include "ui/chromeos/devicetype_utils.h"
#include "ui/strings/grit/ui_strings.h"

namespace ash {

constexpr StaticOobeScreenId ResetView::kScreenId;

ResetScreenHandler::ResetScreenHandler() : BaseScreenHandler(kScreenId) {}

ResetScreenHandler::~ResetScreenHandler() = default;

void ResetScreenHandler::Show() {
  ShowInWebUI();
}

void ResetScreenHandler::DeclareLocalizedValues(
    ::login::LocalizedValuesBuilder* builder) {
  builder->Add("resetScreenAccessibleTitle", IDS_RESET_SCREEN_TITLE);
  builder->Add("resetScreenIconTitle", IDS_RESET_SCREEN_ICON_TITLE);
  builder->Add("cancelButton", IDS_CANCEL);

  builder->Add("resetButtonRestart", IDS_RELAUNCH_BUTTON);
  builder->Add("resetButtonPowerwash", IDS_RESET_SCREEN_POWERWASH);
  builder->Add("resetButtonPowerwashAndRollback",
               IDS_RESET_SCREEN_POWERWASH_AND_REVERT);

  builder->Add("resetWarningDataDetails",
               IDS_RESET_SCREEN_WARNING_DETAILS_DATA);
  builder->Add("resetRestartMessage", IDS_RESET_SCREEN_RESTART_MSG);
  builder->AddF("resetRevertPromise",
                IDS_RESET_SCREEN_PREPARING_REVERT_PROMISE,
                IDS_SHORT_PRODUCT_NAME);
  builder->AddF("resetRevertSpinnerMessage",
                IDS_RESET_SCREEN_PREPARING_REVERT_SPINNER_MESSAGE,
                IDS_SHORT_PRODUCT_NAME);

  builder->Add("resetTPMFirmwareUpdate",
               IDS_RESET_SCREEN_TPM_FIRMWARE_UPDATE_OPTION);

  // Variants for screen title.
  builder->AddF("resetWarningTitle", IDS_RESET_SCREEN_WARNING_MSG,
                ui::GetChromeOSDeviceName());
  builder->Add("resetRollbackErrorTitle", IDS_RESET_SCREEN_REVERT_ERROR);

  // Variants for screen message.
  builder->AddF("resetPowerwashWarningDetails",
                IDS_RESET_SCREEN_WARNING_POWERWASH_MSG,
                ui::GetChromeOSDeviceName());
  builder->AddF("resetPowerwashRollbackWarningDetails",
                IDS_RESET_SCREEN_WARNING_POWERWASH_AND_ROLLBACK_MSG,
                ui::GetChromeOSDeviceName());
  builder->AddF("resetRollbackErrorMessageBody",
                IDS_RESET_SCREEN_REVERT_ERROR_EXPLANATION,
                IDS_SHORT_PRODUCT_NAME);

  builder->Add("confirmPowerwashTitle", IDS_RESET_SCREEN_POPUP_POWERWASH_TITLE);
  builder->Add("confirmRollbackTitle", IDS_RESET_SCREEN_POPUP_ROLLBACK_TITLE);
  builder->Add("confirmPowerwashMessage",
               IDS_RESET_SCREEN_POPUP_POWERWASH_TEXT);
  builder->Add("confirmRollbackMessage", IDS_RESET_SCREEN_POPUP_ROLLBACK_TEXT);
  builder->Add("confirmResetButton", IDS_RESET_SCREEN_POPUP_CONFIRM_BUTTON);
  builder->Add("okButton", IDS_APP_OK);
}

void ResetScreenHandler::SetIsRollbackAvailable(bool value) {
  CallExternalAPI("setIsRollbackAvailable", value);
}

// Only serve the request if the confirmation dialog isn't being shown.
void ResetScreenHandler::SetIsRollbackRequested(bool value) {
  CallExternalAPI("setIsRollbackRequested", value);
}

void ResetScreenHandler::SetIsTpmFirmwareUpdateAvailable(bool value) {
  CallExternalAPI("setIsTpmFirmwareUpdateAvailable", value);
}

void ResetScreenHandler::SetIsTpmFirmwareUpdateChecked(bool value) {
  CallExternalAPI("setIsTpmFirmwareUpdateChecked", value);
}

void ResetScreenHandler::SetIsTpmFirmwareUpdateEditable(bool value) {
  CallExternalAPI("setIsTpmFirmwareUpdateEditable", value);
}

void ResetScreenHandler::SetTpmFirmwareUpdateMode(
    tpm_firmware_update::Mode value) {
  CallExternalAPI("setTpmFirmwareUpdateMode", static_cast<int>(value));
}

void ResetScreenHandler::SetShouldShowConfirmationDialog(bool value) {
  CallExternalAPI("setShouldShowConfirmationDialog", value);
}

void ResetScreenHandler::SetScreenState(int value) {
  CallExternalAPI("setScreenState", value);
}

base::WeakPtr<ResetView> ResetScreenHandler::AsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

}  // namespace ash
