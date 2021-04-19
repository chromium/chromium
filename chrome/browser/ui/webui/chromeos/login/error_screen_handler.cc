// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/login/error_screen_handler.h"

#include "base/time/time.h"
#include "chrome/browser/ash/login/screens/error_screen.h"
#include "chrome/browser/ui/webui/chromeos/network_element_localized_strings_provider.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/login/localized_values_builder.h"
#include "ui/chromeos/devicetype_utils.h"
#include "ui/strings/grit/ui_strings.h"

namespace chromeos {

constexpr StaticOobeScreenId ErrorScreenView::kScreenId;

ErrorScreenHandler::ErrorScreenHandler(JSCallsContainer* js_calls_container)
    : BaseScreenHandler(kScreenId, js_calls_container) {
  set_user_acted_method_path("login.ErrorMessageScreen.userActed");
}

ErrorScreenHandler::~ErrorScreenHandler() {
  if (screen_)
    screen_->OnViewDestroyed(this);
}

void ErrorScreenHandler::Show() {
  if (!page_is_ready()) {
    show_on_init_ = true;
    return;
  }
  BaseScreenHandler::ShowScreen(kScreenId);
  if (screen_)
    screen_->DoShow();
  showing_ = true;
}

void ErrorScreenHandler::Hide() {
  showing_ = false;
  show_on_init_ = false;
  if (screen_)
    screen_->DoHide();
}

void ErrorScreenHandler::Bind(ErrorScreen* screen) {
  screen_ = screen;
  BaseScreenHandler::SetBaseScreen(screen_);
}

void ErrorScreenHandler::Unbind() {
  screen_ = nullptr;
  BaseScreenHandler::SetBaseScreen(nullptr);
}

void ErrorScreenHandler::ShowOobeScreen(OobeScreenId screen) {
  ShowScreen(screen);
}

void ErrorScreenHandler::SetErrorStateCode(
    NetworkError::ErrorState error_state) {
  // TODO(crbug.com/1180291) - Remove once OOBE JS calls are fixed.
  if (IsSafeToCallJavascript()) {
    CallJS("login.ErrorMessageScreen.setErrorState",
           static_cast<int>(error_state));
  } else {
    LOG(ERROR) << "Silently dropping SetErrorStateNetwork request.";
  }
}

void ErrorScreenHandler::SetErrorStateNetwork(const std::string& network_name) {
  // TODO(crbug.com/1180291) - Remove once OOBE JS calls are fixed.
  if (IsSafeToCallJavascript()) {
    CallJS("login.ErrorMessageScreen.setErrorStateNetwork", network_name);
  } else {
    LOG(ERROR) << "Silently dropping SetErrorStateNetwork request.";
  }
}

void ErrorScreenHandler::SetGuestSigninAllowed(bool value) {
  // TODO(crbug.com/1180291) - Remove once OOBE JS calls are fixed.
  if (IsSafeToCallJavascript()) {
    CallJS("login.ErrorMessageScreen.allowGuestSignin", value);
  } else {
    LOG(ERROR) << "Silently dropping SetGuestSigninAllowed request.";
  }
}

void ErrorScreenHandler::SetOfflineSigninAllowed(bool value) {
  // TODO(crbug.com/1180291) - Remove once OOBE JS calls are fixed.
  if (IsSafeToCallJavascript()) {
    CallJS("login.ErrorMessageScreen.allowOfflineLogin", value);
  } else {
    LOG(ERROR) << "Silently dropping SetOfflineSigninAllowed request.";
  }
}

void ErrorScreenHandler::SetShowConnectingIndicator(bool value) {
  // TODO(crbug.com/1180291) - Remove once OOBE JS calls are fixed.
  if (IsSafeToCallJavascript()) {
    CallJS("login.ErrorMessageScreen.showConnectingIndicator", value);
  } else {
    LOG(ERROR) << "Silently dropping SetShowConnectingIndicator request.";
  }
}

void ErrorScreenHandler::SetIsPersistentError(bool is_persistent) {
  // TODO(crbug.com/1180291) - Remove once OOBE JS calls are fixed.
  if (IsSafeToCallJavascript()) {
    CallJS("login.ErrorMessageScreen.setIsPersistentError", is_persistent);
  } else {
    LOG(ERROR) << "Silently dropping SetIsPersistentError request.";
  }
}

void ErrorScreenHandler::SetUIState(NetworkError::UIState ui_state) {
  // TODO(crbug.com/1180291) - Remove once OOBE JS calls are fixed
  if (IsSafeToCallJavascript()) {
    CallJS("login.ErrorMessageScreen.setUIState", static_cast<int>(ui_state));
  } else {
    LOG(ERROR) << "Silently dropping SetUIState request.";
  }
}

// TODO (crbug.com/1168114): We need to handle that fully in C++ once
// all error screen logic is migrated to Screen object.
void ErrorScreenHandler::OnCancelButtonClicked() {
  // TODO(crbug.com/1180291) - Remove once OOBE JS calls are fixed.
  if (IsSafeToCallJavascript()) {
    CallJS("cr.ui.Oobe.showUserPods");
  } else {
    LOG(ERROR) << "Silently dropping OnCancelButtonClicked request.";
  }
}

void ErrorScreenHandler::OnReloadGaiaClicked() {
  // TODO(crbug.com/1180291) - Remove once OOBE JS calls are fixed.
  if (IsSafeToCallJavascript()) {
    CallJS("login.GaiaSigninScreen.doReload");
  } else {
    LOG(ERROR) << "Silently dropping OnReloadGaiaClicked request.";
  }
}

void ErrorScreenHandler::DeclareLocalizedValues(
    ::login::LocalizedValuesBuilder* builder) {
  builder->Add("deviceType", ui::GetChromeOSDeviceName());
  builder->Add("loginErrorTitle", IDS_LOGIN_ERROR_TITLE);
  builder->Add("rollbackErrorTitle", IDS_RESET_SCREEN_REVERT_ERROR);
  builder->Add("signinOfflineMessageBody",
               ui::SubstituteChromeOSDeviceType(IDS_LOGIN_OFFLINE_MESSAGE));
  builder->Add("kioskOfflineMessageBody", IDS_KIOSK_OFFLINE_MESSAGE);
  builder->Add("kioskOnlineTitle", IDS_LOGIN_NETWORK_RESTORED_TITLE);
  builder->Add("kioskOnlineMessageBody", IDS_KIOSK_ONLINE_MESSAGE);
  builder->Add("autoEnrollmentOfflineMessageBody",
               IDS_LOGIN_AUTO_ENROLLMENT_OFFLINE_MESSAGE);
  builder->AddF("rollbackErrorMessageBody",
               IDS_RESET_SCREEN_REVERT_ERROR_EXPLANATION,
               IDS_SHORT_PRODUCT_NAME);
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
  builder->AddF("localStateErrorText0", IDS_LOCAL_STATE_ERROR_TEXT_0,
                IDS_SHORT_PRODUCT_NAME);
  builder->Add("localStateErrorText1", IDS_LOCAL_STATE_ERROR_TEXT_1);
  builder->Add("localStateErrorPowerwashButton",
               IDS_LOCAL_STATE_ERROR_POWERWASH_BUTTON);
  builder->Add("connectingIndicatorText", IDS_LOGIN_CONNECTING_INDICATOR_TEXT);
  builder->Add("guestSigninFixNetwork", IDS_LOGIN_GUEST_SIGNIN_FIX_NETWORK);
  builder->Add("rebootButton", IDS_RELAUNCH_BUTTON);
  builder->Add("diagnoseButton", IDS_DIAGNOSE_BUTTON);
  builder->Add("configureCertsButton", IDS_MANAGE_CERTIFICATES);
  builder->Add("continueButton", IDS_WELCOME_SELECTION_CONTINUE_BUTTON);
  builder->Add("okButton", IDS_APP_OK);
  builder->Add("proxySettingsMenuName",
               IDS_NETWORK_PROXY_SETTINGS_LIST_ITEM_NAME);
  builder->Add("addWiFiNetworkMenuName", IDS_NETWORK_ADD_WI_FI_LIST_ITEM_NAME);
  network_element::AddLocalizedValuesToBuilder(builder);
}

void ErrorScreenHandler::Initialize() {
  if (!page_is_ready())
    return;

  if (show_on_init_) {
    // TODO(nkostylev): Check that context initial state is properly passed.
    Show();
    show_on_init_ = false;
  }
}

}  // namespace chromeos
