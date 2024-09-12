// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/login/error_screen_handler.h"

#include "base/values.h"
#include "chrome/browser/ui/ash/login/login_display_host.h"
#include "chrome/grit/generated_resources.h"
#include "components/login/localized_values_builder.h"
#include "ui/chromeos/devicetype_utils.h"
#include "ui/chromeos/strings/network/network_element_localized_strings_provider.h"

namespace ash {

ErrorScreenHandler::ErrorScreenHandler() : BaseScreenHandler(kScreenId) {}

ErrorScreenHandler::~ErrorScreenHandler() = default;

void ErrorScreenHandler::ShowScreenWithParam(bool is_closeable) {
  ShowInWebUI(base::Value::Dict().Set("isCloseable", is_closeable));
}

void ErrorScreenHandler::ShowOobeScreen(OobeScreenId screen) {
  // TODO(https://crbug.com/1310191): Migrate off ShowScreenDeprecated.
  ShowScreenDeprecated(screen);
}

void ErrorScreenHandler::SetErrorStateCode(
    NetworkError::ErrorState error_state) {
  CallExternalAPI("setErrorState", static_cast<int>(error_state));
}

void ErrorScreenHandler::SetErrorStateNetwork(const std::string& network_name) {
  CallExternalAPI("setErrorStateNetwork", network_name);
}

void ErrorScreenHandler::SetGuestSigninAllowed(bool value) {
  CallExternalAPI("allowGuestSignin", value);
}

void ErrorScreenHandler::SetOfflineSigninAllowed(bool value) {
  CallExternalAPI("allowOfflineLogin", value);
}

void ErrorScreenHandler::SetShowConnectingIndicator(bool value) {
  CallExternalAPI("showConnectingIndicator", value);
}

void ErrorScreenHandler::SetUIState(NetworkError::UIState ui_state) {
  CallExternalAPI("setUiState", static_cast<int>(ui_state));
}

base::WeakPtr<ErrorScreenView> ErrorScreenHandler::AsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void ErrorScreenHandler::DeclareLocalizedValues(
    ::login::LocalizedValuesBuilder* builder) {
  builder->Add("deviceType", ui::GetChromeOSDeviceName());
  builder->Add("loginErrorTitle", IDS_LOGIN_ERROR_TITLE);
  builder->Add("signinOfflineMessageBody",
               ui::SubstituteChromeOSDeviceType(IDS_LOGIN_OFFLINE_MESSAGE));
  builder->Add("kioskOfflineMessageBody", IDS_KIOSK_OFFLINE_MESSAGE);
  builder->Add("kioskOnlineTitle", IDS_LOGIN_NETWORK_RESTORED_TITLE);
  builder->Add("kioskOnlineMessageBody", IDS_KIOSK_ONLINE_MESSAGE);
  builder->Add("autoEnrollmentOfflineMessageBody",
               IDS_LOGIN_AUTO_ENROLLMENT_OFFLINE_MESSAGE);
  builder->Add("captivePortalTitle", IDS_LOGIN_MAYBE_CAPTIVE_PORTAL_TITLE);
  builder->Add("captivePortalMessage", IDS_LOGIN_MAYBE_CAPTIVE_PORTAL);
  builder->Add("captivePortalProxyMessage",
               IDS_LOGIN_MAYBE_CAPTIVE_PORTAL_PROXY);
  builder->Add("captivePortalNetworkSelect",
               IDS_LOGIN_MAYBE_CAPTIVE_PORTAL_NETWORK_SELECT);
  builder->Add("signinProxyMessageText", IDS_LOGIN_PROXY_ERROR_MESSAGE);
  builder->Add("updateOfflineMessageBody",
               ui::SubstituteChromeOSDeviceType(IDS_UPDATE_OFFLINE_MESSAGE));
  builder->Add("updateProxyMessageText", IDS_UPDATE_PROXY_ERROR_MESSAGE);
  builder->Add("connectingIndicatorText", IDS_LOGIN_CONNECTING_INDICATOR_TEXT);
  builder->Add("guestSigninFixNetwork", IDS_LOGIN_GUEST_SIGNIN_FIX_NETWORK);
  builder->Add("rebootButton", IDS_RELAUNCH_BUTTON);
  builder->Add("diagnoseButton", IDS_DIAGNOSE_BUTTON);
  builder->Add("configureCertsButton", IDS_MANAGE_CERTIFICATES);
  builder->Add("continueButton", IDS_WELCOME_SELECTION_CONTINUE_BUTTON);
  builder->Add("proxySettingsMenuName",
               IDS_NETWORK_PROXY_SETTINGS_LIST_ITEM_NAME);
  builder->Add("addWiFiNetworkMenuName", IDS_NETWORK_ADD_WI_FI_LIST_ITEM_NAME);
  builder->Add("autoEnrollmentErrorMessageTitle", IDS_LOGIN_AUTO_ENROLLMENT_OFFLINE_TITLE);
  ui::network_element::AddLocalizedValuesToBuilder(builder);

  builder->Add("offlineLogin", IDS_OFFLINE_LOGIN_HTML);
}

}  // namespace ash
