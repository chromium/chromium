// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/login/app_launch_splash_screen_handler.h"

#include <memory>
#include <utility>

#include "ash/public/cpp/login_screen.h"
#include "base/values.h"
#include "chrome/browser/ash/app_mode/kiosk_app_manager.h"
#include "chrome/browser/ash/login/oobe_screen.h"
#include "chrome/browser/ash/login/screens/network_error.h"
#include "chrome/browser/ui/webui/chromeos/login/error_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/oobe_ui.h"
#include "chrome/grit/chrome_unscaled_resources.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/network/network_state.h"
#include "chromeos/network/network_state_handler.h"
#include "components/login/localized_values_builder.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/webui/web_ui_util.h"

namespace {

// Returns network name by service path.
std::string GetNetworkName(const std::string& service_path) {
  const chromeos::NetworkState* network =
      chromeos::NetworkHandler::Get()->network_state_handler()->GetNetworkState(
          service_path);
  if (!network)
    return std::string();
  return network->name();
}

}  // namespace

namespace chromeos {

constexpr StaticOobeScreenId AppLaunchSplashScreenView::kScreenId;

AppLaunchSplashScreenHandler::AppLaunchSplashScreenHandler(
    const scoped_refptr<NetworkStateInformer>& network_state_informer,
    ErrorScreen* error_screen)
    : BaseScreenHandler(kScreenId),
      network_state_informer_(network_state_informer),
      error_screen_(error_screen) {
  network_state_informer_->AddObserver(this);
}

AppLaunchSplashScreenHandler::~AppLaunchSplashScreenHandler() {
  network_state_informer_->RemoveObserver(this);
  if (delegate_)
    delegate_->OnDeletingSplashScreenView();
}

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

void AppLaunchSplashScreenHandler::InitializeDeprecated() {
  if (show_on_init_) {
    show_on_init_ = false;
    Show();
  }
}

void AppLaunchSplashScreenHandler::Show() {
  if (!IsJavascriptAllowed()) {
    show_on_init_ = true;
    return;
  }

  is_shown_ = true;

  base::Value::Dict data;
  data.Set("shortcutEnabled",
           !KioskAppManager::Get()->GetDisableBailoutShortcut());

  base::DictionaryValue app_info;
  PopulateAppInfo(&app_info);
  data.Set("appInfo", std::move(app_info));

  SetLaunchText(l10n_util::GetStringUTF8(GetProgressMessageFromState(state_)));
  ShowInWebUI(std::move(data));
  if (toggle_network_config_on_show_.has_value()) {
    DoToggleNetworkConfig(toggle_network_config_on_show_.value());
    toggle_network_config_on_show_.reset();
  }
  if (network_config_shown_)
    ShowNetworkConfigureUI();
}

void AppLaunchSplashScreenHandler::RegisterMessages() {
  AddCallback("configureNetwork",
              &AppLaunchSplashScreenHandler::HandleConfigureNetwork);
  AddCallback("continueAppLaunch",
              &AppLaunchSplashScreenHandler::HandleContinueAppLaunch);
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
  if (state == state_)
    return;

  state_ = state;
  if (IsJavascriptAllowed()) {
    SetLaunchText(
        l10n_util::GetStringUTF8(GetProgressMessageFromState(state_)));
  }

  UpdateState(NetworkError::ERROR_REASON_UPDATE);
}

void AppLaunchSplashScreenHandler::SetDelegate(Delegate* delegate) {
  delegate_ = delegate;
}

void AppLaunchSplashScreenHandler::ShowNetworkConfigureUI() {
  network_config_shown_ = true;

  NetworkStateInformer::State state = network_state_informer_->state();

  // We should not block users when the network was not required by the
  // controller.
  if (!delegate_->IsNetworkRequired()) {
    state = NetworkStateInformer::ONLINE;
  }

  const std::string network_path = network_state_informer_->network_path();
  const std::string network_name = GetNetworkName(network_path);

  error_screen_->SetUIState(NetworkError::UI_STATE_KIOSK_MODE);
  error_screen_->AllowGuestSignin(false);
  error_screen_->AllowOfflineLogin(false);

  switch (state) {
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
    case NetworkStateInformer::OFFLINE: {
      error_screen_->SetErrorState(NetworkError::ERROR_STATE_OFFLINE,
                                   network_name);
      break;
    }
    case NetworkStateInformer::ONLINE: {
      error_screen_->SetErrorState(NetworkError::ERROR_STATE_KIOSK_ONLINE,
                                   network_name);
      break;
    }
    default:
      error_screen_->SetErrorState(NetworkError::ERROR_STATE_OFFLINE,
                                   network_name);
      NOTREACHED();
      break;
  }

  if (GetCurrentScreen() != ErrorScreenView::kScreenId)
    error_screen_->SetParentScreen(kScreenId);
  error_screen_->Show(nullptr);
}

void AppLaunchSplashScreenHandler::ShowErrorMessage(
    KioskAppLaunchError::Error error) {
  ash::LoginScreen::Get()->ShowKioskAppError(
      KioskAppLaunchError::GetErrorMessage(error));
}

bool AppLaunchSplashScreenHandler::IsNetworkReady() {
  return network_state_informer_->state() == NetworkStateInformer::ONLINE;
}

void AppLaunchSplashScreenHandler::OnNetworkReady() {
  // Purposely leave blank because the online case is handled in UpdateState
  // call below.
}

void AppLaunchSplashScreenHandler::UpdateState(
    NetworkError::ErrorReason reason) {
  if (!delegate_)
    return;
  bool new_online_state =
      network_state_informer_->state() == NetworkStateInformer::ONLINE;
  delegate_->OnNetworkStateChanged(new_online_state);

  // Redraw network configure UI when the network state changes.
  if (network_config_shown_) {
    ShowNetworkConfigureUI();
  }
}

void AppLaunchSplashScreenHandler::PopulateAppInfo(
    base::DictionaryValue* out_info) {
  DCHECK(delegate_);
  KioskAppManagerBase::App app = delegate_->GetAppData();

  if (app.name.empty())
    app.name = l10n_util::GetStringUTF8(IDS_SHORT_PRODUCT_NAME);

  if (app.icon.isNull()) {
    app.icon = *ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
        IDR_PRODUCT_LOGO_128);
  }

  // Display app domain if present.
  if (!app.url.is_empty()) {
    app.url = app.url.DeprecatedGetOriginAsURL();
  }

  out_info->SetStringKey("name", app.name);
  out_info->SetStringKey("iconURL",
                         webui::GetBitmapDataUrl(*app.icon.bitmap()));
  out_info->SetStringKey("url", app.url.spec());
}

void AppLaunchSplashScreenHandler::SetLaunchText(const std::string& text) {
  CallJS("login.AppLaunchSplashScreen.updateMessage", text);
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
  if (delegate_)
    delegate_->OnConfigureNetwork();
  else
    LOG(WARNING) << "No delegate set to handle network configuration.";
}

void AppLaunchSplashScreenHandler::HandleContinueAppLaunch() {
  if (!delegate_)
    return;

  network_config_shown_ = false;
  delegate_->OnNetworkConfigFinished();
  Show();
}

void AppLaunchSplashScreenHandler::DoToggleNetworkConfig(bool visible) {
  CallJS("login.AppLaunchSplashScreen.toggleNetworkConfig", visible);
}

}  // namespace chromeos
