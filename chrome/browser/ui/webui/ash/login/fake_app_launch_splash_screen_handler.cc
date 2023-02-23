// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/login/fake_app_launch_splash_screen_handler.h"
#include "chrome/browser/ash/app_mode/kiosk_app_manager_base.h"

namespace ash {

void FakeAppLaunchSplashScreenHandler::Show(KioskAppManagerBase::App app_data) {
  last_app_data_ = app_data;
}

void FakeAppLaunchSplashScreenHandler::ShowErrorMessage(
    KioskAppLaunchError::Error error) {
  error_message_type_ = error;
}

bool FakeAppLaunchSplashScreenHandler::IsNetworkReady() {
  return network_ready_;
}

bool FakeAppLaunchSplashScreenHandler::IsNetworkRequired() const {
  return network_required_;
}

KioskAppLaunchError::Error
FakeAppLaunchSplashScreenHandler::GetErrorMessageType() const {
  return error_message_type_;
}

void FakeAppLaunchSplashScreenHandler::SetNetworkReady(bool ready) {
  network_ready_ = ready;
}

void FakeAppLaunchSplashScreenHandler::SetNetworkRequired() {
  network_required_ = true;
}

void FakeAppLaunchSplashScreenHandler::UpdateAppLaunchState(
    AppLaunchState state) {
  state_ = state;
}

AppLaunchSplashScreenHandler::AppLaunchState
FakeAppLaunchSplashScreenHandler::GetAppLaunchState() const {
  return state_;
}

}  // namespace ash
