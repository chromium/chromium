// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_DBUS_RGBKBD_FAKE_RGBKBD_CLIENT_H_
#define CHROMEOS_ASH_COMPONENTS_DBUS_RGBKBD_FAKE_RGBKBD_CLIENT_H_

#include <stdint.h>

#include <optional>

#include "base/component_export.h"
#include "base/containers/flat_map.h"
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

  void SetZoneColor(int zone, uint8_t r, uint8_t g, uint8_t b) override;

  void SetRainbowMode() override;

  void SetAnimationMode(rgbkbd::RgbAnimationMode mode) override;

  void set_rgb_keyboard_capabilities(
      std::optional<rgbkbd::RgbKeyboardCapabilities> capabilities) {
    capabilities_ = capabilities;
  }

  std::optional<rgbkbd::RgbKeyboardCapabilities> get_rgb_keyboard_capabilities()
      const {
    return capabilities_;
  }

  bool get_caps_lock_state() const { return caps_lock_state_; }

  bool is_rainbow_mode_set() const { return is_rainbow_mode_set_; }

  const RgbColor& recently_sent_rgb() const { return rgb_color_; }

  const base::flat_map<int, RgbColor>& get_zone_colors() const {
    return zone_colors_;
  }

  void attempt_run_rgb_keyboard_capabilities_callback() {
    if (callback_.is_null() || !should_run_callback_)
      return;
    std::move(callback_).Run(capabilities_);
  }

  void set_should_run_rgb_keyboard_capabilities_callback(
      bool should_run_callback) {
    should_run_callback_ = should_run_callback;
  }

  int animation_mode_call_count() const { return animation_mode_call_count_; }

  void ResetStoredRgbColors();

 private:
  std::optional<rgbkbd::RgbKeyboardCapabilities> capabilities_;
  bool caps_lock_state_ = false;
  bool is_rainbow_mode_set_ = false;
  RgbColor rgb_color_;
  base::flat_map<int, RgbColor> zone_colors_;
  int animation_mode_call_count_ = 0;
  GetRgbKeyboardCapabilitiesCallback callback_;

  // Set if the the `GetRgbKeyboardCapabilitiesCallback` should be ran right
  // when `GetRgbKeyboardCapabilities` is called or if it should be delayed to
  // be manual executed for testing.
  bool should_run_callback_ = true;
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_DBUS_RGBKBD_FAKE_RGBKBD_CLIENT_H_
