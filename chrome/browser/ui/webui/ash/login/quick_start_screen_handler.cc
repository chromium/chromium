// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/login/quick_start_screen_handler.h"

#include "base/values.h"
#include "chrome/grit/generated_resources.h"
#include "components/login/localized_values_builder.h"

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

void QuickStartScreenHandler::SetQRCode(base::Value::List blob) {
  CallExternalAPI("setQRCode", std::move(blob));
}

void QuickStartScreenHandler::SetDiscoverableName(
    const std::string& discoverable_name) {
  CallExternalAPI("setDiscoverableName", discoverable_name);
}

void QuickStartScreenHandler::ShowConnectingToWifi() {
  CallExternalAPI("showConnectingToWifi");
}

void QuickStartScreenHandler::ShowConnectedToWifi(
    std::string ssid,
    absl::optional<std::string> password) {
  if (password.has_value()) {
    CallExternalAPI("showConnectedToWifi", ssid, password.value());
  } else {
    CallExternalAPI("showConnectedToWifi", ssid);
  }
}

void QuickStartScreenHandler::ShowTransferringGaiaCredentials() {
  CallExternalAPI("showTransferringGaiaCredentials");
}

void QuickStartScreenHandler::ShowFidoAssertionReceived(std::string email) {
  CallExternalAPI("showFidoAssertionReceived", email);
}

void QuickStartScreenHandler::DeclareLocalizedValues(
    ::login::LocalizedValuesBuilder* builder) {
  builder->Add("quickStartSetupTitle", IDS_LOGIN_QUICK_START_SETUP_TITLE);
  builder->Add("quickStartSetupSubtitleQrCode",
               IDS_LOGIN_QUICK_START_SETUP_SUBTITLE_QR_CODE);
  builder->Add("quickStartSetupSubtitlePinCode",
               IDS_LOGIN_QUICK_START_SETUP_SUBTITLE_PIN_CODE);
  builder->Add("quickStartWifiTransferTitle",
               IDS_LOGIN_QUICK_START_WIFI_TRANSFER_TITLE);
  builder->Add("quickStartWifiTransferSubtitle",
               IDS_LOGIN_QUICK_START_WIFI_TRANSFER_SUBTITLE);
  builder->Add("quickStartNetworkNeededSubtitle",
               IDS_LOGIN_QUICK_START_NETWORK_NEEDED_SUBTITLE);
  builder->Add("quickStartStartAfterResumeTitle",
               IDS_LOGIN_QUICK_START_RESUME_AFTER_REBOOT_TITLE);
  builder->Add("quickStartStartAfterResumeSubtitle",
               IDS_LOGIN_QUICK_START_RESUME_AFTER_REBOOT_SUBTITLE);
  builder->Add("quickStartAccountTransferTitle",
               IDS_LOGIN_QUICK_START_ACCOUNT_TRANSFER_STEP_TITLE);
  builder->Add("quickStartAccountTransferSubtitle",
               IDS_LOGIN_QUICK_START_ACCOUNT_TRANSFER_STEP_SUBTITLE);
  builder->Add("quickStartSetupFromSigninTitle",
               IDS_LOGIN_QUICK_START_SETUP_FROM_SIGNIN_SCREEN_TITLE);
  builder->Add("quickStartSetupFromSigninSubtitle",
               IDS_LOGIN_QUICK_START_SETUP_FROM_SIGNIN_SCREEN_SUBTITLE);
}

}  // namespace ash
