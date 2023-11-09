// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/login/app_launch_splash_screen_handler.h"

#include <string_view>
#include <utility>

#include "ash/public/cpp/login_screen.h"
#include "base/values.h"
#include "chrome/browser/ash/app_mode/kiosk_chrome_app_manager.h"
#include "chrome/browser/ash/login/oobe_screen.h"
#include "chrome/browser/ash/login/screens/network_error.h"
#include "chrome/browser/ui/webui/ash/login/error_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/network_state_informer.h"
#include "chrome/browser/ui/webui/ash/login/oobe_ui.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/chrome_unscaled_resources.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/ash/components/network/network_state_handler.h"
#include "components/login/localized_values_builder.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/webui/web_ui_util.h"
#include "ui/gfx/image/image_skia.h"
#include "url/gurl.h"

namespace ash {

namespace {

std::string NameOrDefault(std::string_view name) {
  return name.empty() ? l10n_util::GetStringUTF8(IDS_SHORT_PRODUCT_NAME)
                      : std::string(name);
}

gfx::ImageSkia IconOrDefault(gfx::ImageSkia icon) {
  return icon.isNull()
             ? *ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
                   IDR_PRODUCT_LOGO_128)
             : icon;
}

}  // namespace

AppLaunchSplashScreenView::Data::Data(std::string_view name,
                                      gfx::ImageSkia icon,
                                      const GURL& url)
    : name(NameOrDefault(name)),
      icon(IconOrDefault(icon)),
      url(url.DeprecatedGetOriginAsURL()) {}
AppLaunchSplashScreenView::Data::Data(Data&&) = default;
AppLaunchSplashScreenView::Data& AppLaunchSplashScreenView::Data::operator=(
    Data&&) = default;
AppLaunchSplashScreenView::Data::~Data() = default;

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

void AppLaunchSplashScreenHandler::Show(Data data) {
  is_shown_ = true;

  base::Value::Dict dict =
      base::Value::Dict()
          .Set("shortcutEnabled",
               !KioskChromeAppManager::Get()->GetDisableBailoutShortcut())
          .Set("appInfo",
               base::Value::Dict()
                   .Set("name", data.name)
                   .Set("iconURL", webui::GetBitmapDataUrl(*data.icon.bitmap()))
                   .Set("url", data.url.spec()));

  SetLaunchText(l10n_util::GetStringUTF8(GetProgressMessageFromState(state_)));
  ShowInWebUI(std::move(dict));
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
