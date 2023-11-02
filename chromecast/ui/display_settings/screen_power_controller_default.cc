// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/ui/display_settings/screen_power_controller_default.h"

#include "base/logging.h"
#include "base/time/time.h"

namespace chromecast {

namespace {

constexpr base::TimeDelta kScreenOnOffDuration = base::Milliseconds(200);
}

std::unique_ptr<ScreenPowerController> ScreenPowerController::Create(
    Delegate* delegate) {
  return std::make_unique<ScreenPowerControllerDefault>(delegate);
}

ScreenPowerControllerDefault::ScreenPowerControllerDefault(
    ScreenPowerController::Delegate* delegate)
    : screen_on_(true), delegate_(delegate) {}

ScreenPowerControllerDefault::~ScreenPowerControllerDefault() = default;

void ScreenPowerControllerDefault::SetScreenOn() {
  if (screen_on_) {
    return;
  }

  LOG(INFO) << "Setting screen_on to true";
  screen_on_ = true;
  delegate_->SetScreenBrightnessOn(true, kScreenOnOffDuration);
}

void ScreenPowerControllerDefault::SetScreenOff() {
  if (!screen_on_) {
    return;
  }
  LOG(INFO) << "Setting screen_on to false";
  screen_on_ = false;
  delegate_->SetScreenBrightnessOn(false, kScreenOnOffDuration);
}

void ScreenPowerControllerDefault::SetAllowScreenPowerOff(
    bool allow_power_off) {
  LOG(WARNING)
      << "Screen is not allowed to be powered off. Please enable AURA.";
}

bool ScreenPowerControllerDefault::IsScreenOn() const {
  return screen_on_;
}

}  // namespace chromecast
