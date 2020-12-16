// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_FAKE_APP_LAUNCH_SPLASH_SCREEN_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_FAKE_APP_LAUNCH_SPLASH_SCREEN_HANDLER_H_

#include "chrome/browser/ui/webui/chromeos/login/app_launch_splash_screen_handler.h"

namespace chromeos {

// Version of AppLaunchSplashScreenHandler used for tests.
class FakeAppLaunchSplashScreenHandler : public AppLaunchSplashScreenView {
 public:
  void SetDelegate(Delegate*) override {}
  void Show() override {}
  void Hide() override {}
  void UpdateAppLaunchState(AppLaunchState state) override;
  void ToggleNetworkConfig(bool) override {}
  void ShowNetworkConfigureUI() override {}

  bool IsNetworkReady() override;
  void SetNetworkReady(bool ready);
  AppLaunchState GetAppLaunchState();

 private:
  bool network_ready_ = false;
  AppLaunchState state_ = APP_LAUNCH_STATE_PREPARING_PROFILE;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_FAKE_APP_LAUNCH_SPLASH_SCREEN_HANDLER_H_
