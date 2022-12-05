// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/dbus/rgbkbd/fake_rgbkbd_client.h"

#include <utility>

namespace ash {

FakeRgbkbdClient::FakeRgbkbdClient() = default;
FakeRgbkbdClient::~FakeRgbkbdClient() = default;

void FakeRgbkbdClient::GetRgbKeyboardCapabilities(
    GetRgbKeyboardCapabilitiesCallback callback) {
  callback_ = std::move(callback);
  attempt_run_rgb_keyboard_capabilities_callback();
}

void FakeRgbkbdClient::SetCapsLockState(bool enabled) {
  caps_lock_state_ = enabled;
}

void FakeRgbkbdClient::SetStaticBackgroundColor(uint8_t r,
                                                uint8_t g,
                                                uint8_t b) {
  rgb_color_ = std::make_tuple(r, g, b);
  is_rainbow_mode_set_ = false;
}

void FakeRgbkbdClient::ResetStoredRgbColors() {
  rgb_color_ = std::make_tuple(0u, 0u, 0u);
}

void FakeRgbkbdClient::SetZoneColor(int zone, uint8_t r, uint8_t g, uint8_t b) {
  zone_colors_[zone] = std::make_tuple(r, g, b);
}

void FakeRgbkbdClient::SetRainbowMode() {
  is_rainbow_mode_set_ = true;
  ResetStoredRgbColors();
}
void FakeRgbkbdClient::SetAnimationMode(rgbkbd::RgbAnimationMode mode) {
  ++animation_mode_call_count_;
}
}  // namespace ash
