// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_DBUS_RGBKBD_FAKE_RGBKBD_CLIENT_H_
#define CHROMEOS_ASH_COMPONENTS_DBUS_RGBKBD_FAKE_RGBKBD_CLIENT_H_

#include "base/component_export.h"
#include "chromeos/ash/components/dbus/rgbkbd/rgbkbd_client.h"
#include "third_party/cros_system_api/dbus/rgbkbd/dbus-constants.h"

namespace ash {

class COMPONENT_EXPORT(RGBKBD) FakeRgbkbdClient : public RgbkbdClient {
 public:
  FakeRgbkbdClient();
  FakeRgbkbdClient(const FakeRgbkbdClient&) = delete;
  FakeRgbkbdClient& operator=(const FakeRgbkbdClient&) = delete;
  ~FakeRgbkbdClient() override;

  void GetRgbKeyboardCapabilities(
      GetRgbKeyboardCapabilitiesCallback callback) override;

  void SetCapsLockState(bool enabled) override;

  void set_rgb_keyboard_capabilities(
      absl::optional<rgbkbd::RgbKeyboardCapabilities> capabilities) {
    capabilities_ = capabilities;
  }

  int get_rgb_keyboard_capabilities_call_count() const {
    return get_rgb_keyboard_capabilities_call_count_;
  }

  bool get_caps_lock_state() const { return caps_lock_state_; }

 private:
  absl::optional<rgbkbd::RgbKeyboardCapabilities> capabilities_;
  int get_rgb_keyboard_capabilities_call_count_ = 0;
  bool caps_lock_state_;
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_DBUS_RGBKBD_FAKE_RGBKBD_CLIENT_H_
