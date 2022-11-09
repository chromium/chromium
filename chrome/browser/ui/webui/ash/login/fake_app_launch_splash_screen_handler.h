// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_FAKE_APP_LAUNCH_SPLASH_SCREEN_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_FAKE_APP_LAUNCH_SPLASH_SCREEN_HANDLER_H_

#include "chrome/browser/ui/webui/ash/login/app_launch_splash_screen_handler.h"

namespace ash {

// Version of AppLaunchSplashScreenHandler used for tests.
class FakeAppLaunchSplashScreenHandler : public AppLaunchSplashScreenView {
 public:
  void SetDelegate(Delegate*) override {}
  void Show() override {}
  void Hide() override {}
  void UpdateAppLaunchState(AppLaunchState state) override;
  void ToggleNetworkConfig(bool) override {}
  void ShowNetworkConfigureUI() override {}
  void ShowErrorMessage(KioskAppLaunchError::Error error) override;
  bool IsNetworkReady() override;
  void ContinueAppLaunch() override {}

  KioskAppLaunchError::Error GetErrorMessageType() const;
  void SetNetworkReady(bool ready);
  AppLaunchState GetAppLaunchState() const;

 private:
  KioskAppLaunchError::Error error_message_type_ =
      KioskAppLaunchError::Error::kNone;
  bool network_ready_ = false;
  AppLaunchState state_ = AppLaunchState::kPreparingProfile;
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_FAKE_APP_LAUNCH_SPLASH_SCREEN_HANDLER_H_
