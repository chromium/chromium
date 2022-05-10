// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_DBUS_RGBKBD_FAKE_RGBKBD_CLIENT_H_
#define CHROMEOS_ASH_COMPONENTS_DBUS_RGBKBD_FAKE_RGBKBD_CLIENT_H_

#include <stdint.h>

#include "base/component_export.h"
#include "chromeos/ash/components/dbus/rgbkbd/rgbkbd_client.h"
#include "third_party/cros_system_api/dbus/rgbkbd/dbus-constants.h"

namespace ash {

using RgbColor = std::tuple<uint8_t, uint8_t, uint8_t>;

class COMPONENT_EXPORT(RGBKBD) FakeRgbkbdClient : public RgbkbdClient {
 public:
  FakeRgbkbdClient();
  FakeRgbkbdClient(const FakeRgbkbdClient&) = delete;
  FakeRgbkbdClient& operator=(const FakeRgbkbdClient&) = delete;
  ~FakeRgbkbdClient() override;

  void GetRgbKeyboardCapabilities(
      GetRgbKeyboardCapabilitiesCallback callback) override;

  void SetCapsLockState(bool enabled) override;

  void SetStaticBackgroundColor(uint8_t r, uint8_t g, uint8_t b) override;

  void SetRainbowMode() override;

  void SetAnimationMode(rgbkbd::RgbAnimationMode mode) override;

  void set_rgb_keyboard_capabilities(
      absl::optional<rgbkbd::RgbKeyboardCapabilities> capabilities) {
    capabilities_ = capabilities;
  }

  int get_rgb_keyboard_capabilities_call_count() const {
    return get_rgb_keyboard_capabilities_call_count_;
  }

  bool get_caps_lock_state() const { return caps_lock_state_; }

  bool is_rainbow_mode_set() const { return is_rainbow_mode_set_; }

  const RgbColor& recently_sent_rgb() const { return rgb_color_; }

  int animation_mode_call_count() const { return animation_mode_call_count_; }

  void ResetStoredRgbColors();

 private:
  absl::optional<rgbkbd::RgbKeyboardCapabilities> capabilities_;
  int get_rgb_keyboard_capabilities_call_count_ = 0;
  bool caps_lock_state_;
  bool is_rainbow_mode_set_ = false;
  RgbColor rgb_color_;
  int animation_mode_call_count_ = 0;
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_DBUS_RGBKBD_FAKE_RGBKBD_CLIENT_H_
