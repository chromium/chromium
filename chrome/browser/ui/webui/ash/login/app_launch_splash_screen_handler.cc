// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/login/app_launch_splash_screen_handler.h"

#include <string>
#include <utility>

#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "chrome/browser/ash/login/oobe_screen.h"
#include "chrome/browser/ui/webui/ash/login/base_screen_handler.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/ash/components/network/network_state_handler.h"
#include "components/login/localized_values_builder.h"
#include "ui/base/l10n/l10n_util.h"

namespace ash {

AppLaunchSplashScreenHandler::AppLaunchSplashScreenHandler()
    : BaseScreenHandler(kScreenId) {}

AppLaunchSplashScreenHandler::~AppLaunchSplashScreenHandler() = default;

void AppLaunchSplashScreenHandler::DeclareLocalizedValues(
    ::login::LocalizedValuesBuilder* builder) {
  builder->Add("appStartMessage", IDS_APP_START_NETWORK_WAIT_MESSAGE);

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

void AppLaunchSplashScreenHandler::UpdateAppLaunchText(AppLaunchState state) {
  CallExternalAPI("updateMessage",
                  l10n_util::GetStringUTF8(GetProgressMessageFromState(state)));
}

void AppLaunchSplashScreenHandler::HideThrobber() {
  CallExternalAPI("hideThrobber");
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
    case AppLaunchState::kChromeAppDeprecated:
      return IDS_KIOSK_APP_ERROR_UNABLE_TO_LAUNCH_CHROME_APP;
    case AppLaunchState::kIsolatedAppNotAllowed:
      return IDS_KIOSK_APP_ERROR_IWA_UNSUPPORTED;
  }
}

base::WeakPtr<AppLaunchSplashScreenView>
AppLaunchSplashScreenHandler::AsWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

}  // namespace ash
