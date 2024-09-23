// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/login/quick_start_screen_handler.h"

#include "base/values.h"
#include "chrome/grit/generated_resources.h"
#include "components/login/localized_values_builder.h"
#include "quick_start_screen_handler.h"

namespace ash {

QuickStartScreenHandler::QuickStartScreenHandler()
    : BaseScreenHandler(kScreenId) {}

QuickStartScreenHandler::~QuickStartScreenHandler() = default;

void QuickStartScreenHandler::Show() {
  ShowInWebUI();
}

void QuickStartScreenHandler::SetPIN(const std::string pin) {
  CallExternalAPI("setPin", pin);
}

void QuickStartScreenHandler::SetQRCode(base::Value::List blob,
                                        const std::string url) {
  CallExternalAPI("setQRCode", std::move(blob), url);
}

void QuickStartScreenHandler::ShowInitialUiStep() {
  CallExternalAPI("showInitialUiStep");
}

void QuickStartScreenHandler::ShowBluetoothDialog() {
  CallExternalAPI("showBluetoothDialog");
}

void QuickStartScreenHandler::ShowConnectingToPhoneStep() {
  CallExternalAPI("showConnectingToPhoneStep");
}

void QuickStartScreenHandler::ShowConnectingToWifi() {
  CallExternalAPI("showConnectingToWifi");
}

void QuickStartScreenHandler::ShowConfirmGoogleAccount() {
  CallExternalAPI("showConfirmGoogleAccount");
}

void QuickStartScreenHandler::ShowSigningInStep() {
  CallExternalAPI("showSigningInStep");
}

void QuickStartScreenHandler::ShowCreatingAccountStep() {
  CallExternalAPI("showCreatingAccountStep");
}

void QuickStartScreenHandler::ShowSetupCompleteStep(
    const bool did_transfer_wifi) {
  CallExternalAPI("showSetupCompleteStep", did_transfer_wifi);
}

void QuickStartScreenHandler::SetUserEmail(const std::string email) {
  CallExternalAPI("setUserEmail", email);
}

void QuickStartScreenHandler::SetUserFullName(const std::string full_name) {
  CallExternalAPI("setUserFullName", full_name);
}

void QuickStartScreenHandler::SetUserAvatar(const std::string avatar_url) {
  CallExternalAPI("setUserAvatarUrl", avatar_url);
}

void QuickStartScreenHandler::SetWillRequestWiFi(const bool will_request_wifi) {
  CallExternalAPI("setWillRequestWiFi", will_request_wifi);
}

base::WeakPtr<QuickStartView> QuickStartScreenHandler::AsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void QuickStartScreenHandler::DeclareLocalizedValues(
    ::login::LocalizedValuesBuilder* builder) {
  builder->Add("quickStartSetupQrTitle", IDS_LOGIN_QUICK_START_SETUP_QR_TITLE);
  builder->Add("quickStartSetupPinTitle",
               IDS_LOGIN_QUICK_START_SETUP_PIN_TITLE);
  builder->Add("quickStartSetupSubtitle", IDS_LOGIN_QUICK_START_SETUP_SUBTITLE);
  builder->Add("quickStartSetupSubtitleAccountOnly",
               IDS_LOGIN_QUICK_START_SETUP_SUBTITLE_ACCOUNT_ONLY);
  builder->Add("quickStartSetupContentFooterTurnOnWifi",
               IDS_LOGIN_QUICK_START_SETUP_CONTENT_FOOTER_TURN_ON_WIFI_AND_BLT);
  builder->Add(
      "quickStartSetupContentFooterFollowInstructionsQr",
      IDS_LOGIN_QUICK_START_SETUP_CONTENT_FOOTER_FOLLOW_INSTRUCTIONS_QR);
  builder->Add(
      "quickStartSetupContentFooterFollowInstructionsPin",
      IDS_LOGIN_QUICK_START_SETUP_CONTENT_FOOTER_FOLLOW_INSTRUCTIONS_PIN);
  builder->Add("quickStartWifiTransferTitle",
               IDS_LOGIN_QUICK_START_WIFI_TRANSFER_TITLE);
  builder->Add("quickStartWifiTransferSubtitle",
               IDS_LOGIN_QUICK_START_WIFI_TRANSFER_SUBTITLE);
  builder->Add("quickStartStartAfterResumeTitle",
               IDS_LOGIN_QUICK_START_RESUME_AFTER_REBOOT_TITLE);
  builder->Add("quickStartStartAfterResumeSubtitle",
               IDS_LOGIN_QUICK_START_RESUME_AFTER_REBOOT_SUBTITLE);
  builder->Add("quickStartAccountTransferTitle",
               IDS_LOGIN_QUICK_START_ACCOUNT_TRANSFER_STEP_TITLE);
  builder->Add("quickStartAccountTransferSubtitle",
               IDS_LOGIN_QUICK_START_ACCOUNT_TRANSFER_STEP_SUBTITLE);
  builder->Add("quickStartSetupCompleteTitle",
               IDS_LOGIN_QUICK_START_SETUP_COMPLETE_STEP_TITLE);
  builder->Add("quickStartSetupCompleteSubtitleBoth",
               IDS_LOGIN_QUICK_START_SETUP_COMPLETE_STEP_SUBTITLE_BOTH);
  builder->Add("quickStartSetupCompleteSubtitleSignedIn",
               IDS_LOGIN_QUICK_START_SETUP_COMPLETE_STEP_SUBTITLE_SIGNED_IN);
  builder->Add("quickStartSetupFromSigninTitle",
               IDS_LOGIN_QUICK_START_SETUP_FROM_SIGNIN_SCREEN_TITLE);
  builder->Add("quickStartSetupFromSigninSubtitle",
               IDS_LOGIN_QUICK_START_SETUP_FROM_SIGNIN_SCREEN_SUBTITLE);
  builder->Add("quickStartBluetoothTitle",
               IDS_LOGIN_QUICK_START_BLUETOOTH_DIALOG_TITLE);
  builder->Add("quickStartBluetoothContent",
               IDS_LOGIN_QUICK_START_BLUETOOTH_DIALOG_CONTENT);
  builder->Add("quickStartBluetoothCancelButton",
               IDS_LOGIN_QUICK_START_BLUETOOTH_DIALOG_CANCEL);
  builder->Add("quickStartBluetoothEnableButton",
               IDS_LOGIN_QUICK_START_BLUETOOTH_DIALOG_ENABLE);
  builder->Add("quickStartConfirmAccountTitle",
               IDS_LOGIN_QUICK_START_CONFIRM_ACCOUNT_TITLE);
  builder->Add("quickStartConfirmAccountSubtitle",
               IDS_LOGIN_QUICK_START_CONFIRM_ACCOUNT_SUBTITLE);
}

}  // namespace ash
