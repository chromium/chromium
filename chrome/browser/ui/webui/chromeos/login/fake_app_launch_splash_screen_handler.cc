// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <chrome/browser/ui/webui/chromeos/login/fake_app_launch_splash_screen_handler.h>

namespace chromeos {

bool FakeAppLaunchSplashScreenHandler::IsNetworkReady() {
  return network_ready_;
}

void FakeAppLaunchSplashScreenHandler::SetNetworkReady(bool ready) {
  network_ready_ = ready;
}

void FakeAppLaunchSplashScreenHandler::UpdateAppLaunchState(
    AppLaunchState state) {
  state_ = state;
}

AppLaunchSplashScreenHandler::AppLaunchState
FakeAppLaunchSplashScreenHandler::GetAppLaunchState() {
  return state_;
}

}  // namespace chromeos
