// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/login/reset_screen_handler.h"

#include <string>

#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/login/help_app_launcher.h"
#include "chrome/browser/chromeos/login/oobe_screen.h"
#include "chrome/browser/chromeos/login/screens/reset_screen.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/dbus/session_manager/session_manager_client.h"
#include "components/login/localized_values_builder.h"
#include "components/strings/grit/components_strings.h"

namespace chromeos {

constexpr StaticOobeScreenId ResetView::kScreenId;

ResetScreenHandler::ResetScreenHandler(JSCallsContainer* js_calls_container)
    : BaseScreenHandler(kScreenId, js_calls_container) {
  set_user_acted_method_path("login.ResetScreen.userActed");
}

ResetScreenHandler::~ResetScreenHandler() {
  if (screen_)
    screen_->OnViewDestroyed(this);
}

void ResetScreenHandler::Bind(ResetScreen* screen) {
  screen_ = screen;
  BaseScreenHandler::SetBaseScreen(screen_);
}

void ResetScreenHandler::Unbind() {
  screen_ = nullptr;
  BaseScreenHandler::SetBaseScreen(nullptr);
}

void ResetScreenHandler::Show() {
  if (!page_is_ready()) {
    show_on_init_ = true;
    return;
  }
  ShowScreen(kScreenId);
}

void ResetScreenHandler::Hide() {
}

void ResetScreenHandler::DeclareLocalizedValues(
    ::login::LocalizedValuesBuilder* builder) {
  builder->Add("resetScreenAccessibleTitle", IDS_RESET_SCREEN_TITLE);
  builder->Add("resetScreenIconTitle", IDS_RESET_SCREEN_ICON_TITLE);
  builder->Add("resetScreenIllustrationTitle",
               IDS_RESET_SCREEN_ILLUSTRATION_TITLE);
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
  builder->AddF("resetWarningTitle",
                IDS_RESET_SCREEN_WARNING_MSG,
                IDS_SHORT_PRODUCT_NAME);

  // Variants for screen message.
  builder->AddF("resetPowerwashWarningDetails",
                IDS_RESET_SCREEN_WARNING_POWERWASH_MSG,
                IDS_SHORT_PRODUCT_NAME);
  builder->AddF("resetPowerwashRollbackWarningDetails",
                IDS_RESET_SCREEN_WARNING_POWERWASH_AND_ROLLBACK_MSG,
                IDS_SHORT_PRODUCT_NAME);

  builder->Add("confirmPowerwashTitle", IDS_RESET_SCREEN_POPUP_POWERWASH_TITLE);
  builder->Add("confirmRollbackTitle", IDS_RESET_SCREEN_POPUP_ROLLBACK_TITLE);
  builder->Add("confirmPowerwashMessage",
               IDS_RESET_SCREEN_POPUP_POWERWASH_TEXT);
  builder->Add("confirmRollbackMessage", IDS_RESET_SCREEN_POPUP_ROLLBACK_TEXT);
  builder->Add("confirmResetButton", IDS_RESET_SCREEN_POPUP_CONFIRM_BUTTON);
}

void ResetScreenHandler::DeclareJSCallbacks() {
  AddCallback("ResetScreen.setTpmFirmwareUpdateChecked",
              &ResetScreenHandler::HandleSetTpmFirmwareUpdateChecked);
}

void ResetScreenHandler::Initialize() {
  if (!page_is_ready())
    return;

  if (show_on_init_) {
    Show();
    show_on_init_ = false;
  }
}

void ResetScreenHandler::SetIsRollbackAvailable(bool value) {
  is_rollback_available_ = value;
  CallJS("login.ResetScreen.setIsRollbackAvailable", value);
}

void ResetScreenHandler::SetIsRollbackChecked(bool value) {
  is_rollback_checked_ = value;
  CallJS("login.ResetScreen.setIsRollbackChecked", value);
}

void ResetScreenHandler::SetIsTpmFirmwareUpdateAvailable(bool value) {
  CallJS("login.ResetScreen.setIsTpmFirmwareUpdateAvailable", value);
}

void ResetScreenHandler::SetIsTpmFirmwareUpdateChecked(bool value) {
  is_tpm_firmware_update_checked_ = value;
  CallJS("login.ResetScreen.setIsTpmFirmwareUpdateChecked", value);
}

void ResetScreenHandler::SetIsTpmFirmwareUpdateEditable(bool value) {
  CallJS("login.ResetScreen.setIsTpmFirmwareUpdateEditable", value);
}

void ResetScreenHandler::SetTpmFirmwareUpdateMode(
    tpm_firmware_update::Mode value) {
  mode_ = value;
  CallJS("login.ResetScreen.setTpmFirmwareUpdateMode", static_cast<int>(value));
}

void ResetScreenHandler::SetIsConfirmational(bool value) {
  CallJS("login.ResetScreen.setIsConfirmational", value);
}

void ResetScreenHandler::SetIsOfficialBuild(bool value) {
  CallJS("login.ResetScreen.setIsOfficialBuild", value);
}

void ResetScreenHandler::SetScreenState(State value) {
  state_ = value;
  CallJS("login.ResetScreen.setScreenState", static_cast<int>(value));
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

bool ResetScreenHandler::GetIsRollbackChecked() {
  return is_rollback_checked_;
}

bool ResetScreenHandler::GetIsTpmFirmwareUpdateChecked() {
  return is_tpm_firmware_update_checked_;
}

void ResetScreenHandler::HandleSetTpmFirmwareUpdateChecked(bool value) {
  is_tpm_firmware_update_checked_ = value;
}

}  // namespace chromeos
