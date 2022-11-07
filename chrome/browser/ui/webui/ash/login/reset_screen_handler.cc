// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/login/reset_screen_handler.h"

#include <string>

#include "base/values.h"
#include "chrome/browser/ash/login/oobe_screen.h"
#include "chrome/grit/chromium_strings.h"
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
  is_rollback_available_ = value;
  CallExternalAPI("setIsRollbackAvailable", value);
}

// Only serve the request if the confirmation dialog isn't being shown.
void ResetScreenHandler::SetIsRollbackRequested(bool value) {
  if (is_showing_confirmation_dialog_)
    return;

  is_rollback_requested_ = value;

  CallExternalAPI("setIsRollbackRequested", value);
}

void ResetScreenHandler::SetIsTpmFirmwareUpdateAvailable(bool value) {
  CallExternalAPI("setIsTpmFirmwareUpdateAvailable", value);
}

void ResetScreenHandler::SetIsTpmFirmwareUpdateChecked(bool value) {
  is_tpm_firmware_update_checked_ = value;
  CallExternalAPI("setIsTpmFirmwareUpdateChecked", value);
}

void ResetScreenHandler::SetIsTpmFirmwareUpdateEditable(bool value) {
  CallExternalAPI("setIsTpmFirmwareUpdateEditable", value);
}

void ResetScreenHandler::SetTpmFirmwareUpdateMode(
    tpm_firmware_update::Mode value) {
  mode_ = value;
  CallExternalAPI("setTpmFirmwareUpdateMode", static_cast<int>(value));
}

void ResetScreenHandler::SetShouldShowConfirmationDialog(bool value) {
  is_showing_confirmation_dialog_ = value;
  CallExternalAPI("setShouldShowConfirmationDialog", value);
}

void ResetScreenHandler::SetConfirmationDialogClosed() {
  is_showing_confirmation_dialog_ = false;
}

void ResetScreenHandler::SetScreenState(State value) {
  state_ = value;
  CallExternalAPI("setScreenState", static_cast<int>(value));
}

ResetView::State ResetScreenHandler::GetScreenState() {
  return state_;
}

tpm_firmware_update::Mode ResetScreenHandler::GetTpmFirmwareUpdateMode() {
  return mode_;
}

bool ResetScreenHandler::GetIsRollbackAvailable() {
  return is_rollback_available_;
}

bool ResetScreenHandler::GetIsRollbackRequested() {
  return is_rollback_requested_;
}

bool ResetScreenHandler::GetIsTpmFirmwareUpdateChecked() {
  return is_tpm_firmware_update_checked_;
}

void ResetScreenHandler::HandleSetTpmFirmwareUpdateChecked(bool value) {
  is_tpm_firmware_update_checked_ = value;
}

}  // namespace ash
