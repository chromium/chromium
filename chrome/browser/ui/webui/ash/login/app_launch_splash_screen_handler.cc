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
#include "chrome/browser/ash/login/screens/error_screen.h"
#include "chrome/browser/ash/login/screens/network_error.h"
#include "chrome/browser/ui/webui/ash/login/error_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/network_state_informer.h"
#include "chrome/browser/ui/webui/ash/login/oobe_ui.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/ash/components/network/network_state_handler.h"
#include "components/login/localized_values_builder.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image_skia.h"
#include "url/gurl.h"

namespace ash {

AppLaunchSplashScreenHandler::AppLaunchSplashScreenHandler()
    : BaseScreenHandler(kScreenId) {}

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

void AppLaunchSplashScreenHandler::Show(base::Value::Dict data) {
  ShowInWebUI(std::move(data));
}

void AppLaunchSplashScreenHandler::SetAppData(base::Value::Dict data) {
  CallExternalAPI("setAppData", std::move(data));
}

void AppLaunchSplashScreenHandler::ToggleNetworkConfig(bool visible) {
  CallExternalAPI("toggleNetworkConfig", visible);
}

void AppLaunchSplashScreenHandler::UpdateAppLaunchText(AppLaunchState state) {
  CallExternalAPI("updateMessage",
                  l10n_util::GetStringUTF8(GetProgressMessageFromState(state)));
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

base::WeakPtr<AppLaunchSplashScreenView>
AppLaunchSplashScreenHandler::AsWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

}  // namespace ash
