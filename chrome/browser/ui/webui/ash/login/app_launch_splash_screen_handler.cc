// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/login/app_launch_splash_screen_handler.h"

#include <memory>
#include <utility>

#include "ash/public/cpp/login_screen.h"
#include "base/values.h"
#include "chrome/browser/ash/app_mode/kiosk_app_manager.h"
#include "chrome/browser/ash/app_mode/kiosk_app_manager_base.h"
#include "chrome/browser/ash/login/oobe_screen.h"
#include "chrome/browser/ash/login/screens/network_error.h"
#include "chrome/browser/ui/webui/ash/login/error_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/network_state_informer.h"
#include "chrome/browser/ui/webui/ash/login/oobe_ui.h"
#include "chrome/grit/chrome_unscaled_resources.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/ash/components/network/network_state_handler.h"
#include "components/login/localized_values_builder.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/webui/web_ui_util.h"

namespace ash {

namespace {

base::Value::Dict ConvertAppToDict(KioskAppManagerBase::App app) {
  base::Value::Dict out_info;

  if (app.name.empty()) {
    app.name = l10n_util::GetStringUTF8(IDS_SHORT_PRODUCT_NAME);
  }

  if (app.icon.isNull()) {
    app.icon = *ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
        IDR_PRODUCT_LOGO_128);
  }

  // Display app domain if present.
  if (!app.url.is_empty()) {
    app.url = app.url.DeprecatedGetOriginAsURL();
  }

  out_info.Set("name", app.name);
  out_info.Set("iconURL", webui::GetBitmapDataUrl(*app.icon.bitmap()));
  out_info.Set("url", app.url.spec());
  return out_info;
}

}  // namespace

AppLaunchSplashScreenHandler::AppLaunchSplashScreenHandler(
    const scoped_refptr<NetworkStateInformer>& network_state_informer,
    ErrorScreen* error_screen)
    : BaseScreenHandler(kScreenId), error_screen_(error_screen) {}

AppLaunchSplashScreenHandler::~AppLaunchSplashScreenHandler() = default;

void AppLaunchSplashScreenHandler::DeclareLocalizedValues(
    ::login::LocalizedValuesBuilder* builder) {
  builder->Add("appStartMessage", IDS_APP_START_NETWORK_WAIT_MESSAGE);
  builder->Add("configureNetwork", IDS_APP_START_CONFIGURE_NETWORK);

  const std::u16string product_os_name =
      l10n_util::GetStringUTF16(IDS_SHORT_PRODUCT_OS_NAME);
  builder->Add("shortcutInfo",
               l10n_util::GetStringFUTF16(IDS_APP_START_BAILOUT_SHORTCUT_FORMAT,
                                          product_os_name));
}

void AppLaunchSplashScreenHandler::Show(KioskAppManagerBase::App app_data) {
  is_shown_ = true;

  base::Value::Dict data;
  data.Set("shortcutEnabled",
           !KioskAppManager::Get()->GetDisableBailoutShortcut());

  data.Set("appInfo", ConvertAppToDict(app_data));

  SetLaunchText(l10n_util::GetStringUTF8(GetProgressMessageFromState(state_)));
  ShowInWebUI(std::move(data));
  if (toggle_network_config_on_show_.has_value()) {
    DoToggleNetworkConfig(toggle_network_config_on_show_.value());
    toggle_network_config_on_show_.reset();
  }
}

void AppLaunchSplashScreenHandler::DeclareJSCallbacks() {
  AddCallback("configureNetwork",
              &AppLaunchSplashScreenHandler::HandleConfigureNetwork);
}

void AppLaunchSplashScreenHandler::Hide() {
  is_shown_ = false;
}

void AppLaunchSplashScreenHandler::ToggleNetworkConfig(bool visible) {
  if (!is_shown_) {
    toggle_network_config_on_show_ = visible;
    return;
  }
  DoToggleNetworkConfig(visible);
}

void AppLaunchSplashScreenHandler::UpdateAppLaunchState(AppLaunchState state) {
  if (state == state_) {
    return;
  }

  state_ = state;
  SetLaunchText(l10n_util::GetStringUTF8(GetProgressMessageFromState(state_)));
}

void AppLaunchSplashScreenHandler::SetDelegate(Delegate* delegate) {
  delegate_ = delegate;
}

void AppLaunchSplashScreenHandler::ShowNetworkConfigureUI(
    NetworkStateInformer::State network_state,
    const std::string& network_name) {
  network_config_shown_ = true;
  error_screen_->SetUIState(NetworkError::UI_STATE_KIOSK_MODE);
  error_screen_->SetIsPersistentError(true);
  error_screen_->AllowGuestSignin(false);
  error_screen_->AllowOfflineLogin(false);
  switch (network_state) {
    case NetworkStateInformer::CAPTIVE_PORTAL: {
      error_screen_->SetErrorState(NetworkError::ERROR_STATE_PORTAL,
                                   network_name);
      error_screen_->FixCaptivePortal();

      break;
    }
    case NetworkStateInformer::PROXY_AUTH_REQUIRED: {
      error_screen_->SetErrorState(NetworkError::ERROR_STATE_PROXY,
                                   network_name);
      break;
    }
    case NetworkStateInformer::ONLINE: {
      error_screen_->SetErrorState(NetworkError::ERROR_STATE_KIOSK_ONLINE,
                                   network_name);
      break;
    }
    case NetworkStateInformer::OFFLINE:
    case NetworkStateInformer::CONNECTING:
    case NetworkStateInformer::UNKNOWN:
      error_screen_->SetErrorState(NetworkError::ERROR_STATE_OFFLINE,
                                   network_name);
      break;
  }

  if (GetCurrentScreen() != ErrorScreenView::kScreenId) {
    error_screen_->SetParentScreen(kScreenId);
    error_screen_->Show(/*context=*/nullptr);
  }
}

void AppLaunchSplashScreenHandler::ShowErrorMessage(
    KioskAppLaunchError::Error error) {
  LoginScreen::Get()->ShowKioskAppError(
      KioskAppLaunchError::GetErrorMessage(error));
}

void AppLaunchSplashScreenHandler::SetLaunchText(const std::string& text) {
  CallExternalAPI("updateMessage", text);
}

int AppLaunchSplashScreenHandler::GetProgressMessageFromState(
    AppLaunchState state) {
  switch (state) {
    case AppLaunchState::kPreparingProfile:
      return IDS_APP_START_PREPARING_PROFILE_MESSAGE;
    case AppLaunchState::kPreparingNetwork:
      return IDS_APP_START_NETWORK_WAIT_MESSAGE;
    case AppLaunchState::kInstallingApplication:
      return IDS_APP_START_APP_WAIT_MESSAGE;
    case AppLaunchState::kInstallingExtension:
      return IDS_APP_START_EXTENSION_WAIT_MESSAGE;
    case AppLaunchState::kWaitingAppWindow:
      return IDS_APP_START_WAIT_FOR_APP_WINDOW_MESSAGE;
    case AppLaunchState::kWaitingAppWindowInstallFailed:
      return IDS_APP_START_WAIT_FOR_APP_WINDOW_INSTALL_FAILED_MESSAGE;
    case AppLaunchState::kNetworkWaitTimeout:
      return IDS_APP_START_NETWORK_WAIT_TIMEOUT_MESSAGE;
    case AppLaunchState::kShowingNetworkConfigureUI:
      return IDS_APP_START_SHOWING_NETWORK_CONFIGURE_UI_MESSAGE;
  }
}

void AppLaunchSplashScreenHandler::HandleConfigureNetwork() {
  if (delegate_) {
    delegate_->OnConfigureNetwork();
  } else {
    LOG(WARNING) << "No delegate set to handle network configuration.";
  }
}

void AppLaunchSplashScreenHandler::ContinueAppLaunch() {
  if (!delegate_) {
    return;
  }

  network_config_shown_ = false;
  delegate_->OnNetworkConfigFinished();

  // Reset ErrorScreen state to default. We don't update other parameters such
  // as SetUIState/SetErrorState as those should be updated by the next caller
  // of the ErrorScreen.
  error_screen_->SetParentScreen(OOBE_SCREEN_UNKNOWN);
  error_screen_->SetIsPersistentError(false);
}

void AppLaunchSplashScreenHandler::DoToggleNetworkConfig(bool visible) {
  CallExternalAPI("toggleNetworkConfig", visible);
}

}  // namespace ash
