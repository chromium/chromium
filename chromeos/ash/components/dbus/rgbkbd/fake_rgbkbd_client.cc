// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/dbus/rgbkbd/fake_rgbkbd_client.h"

#include <utility>

namespace ash {

FakeRgbkbdClient::FakeRgbkbdClient() = default;
FakeRgbkbdClient::~FakeRgbkbdClient() = default;

void FakeRgbkbdClient::GetRgbKeyboardCapabilities(
    GetRgbKeyboardCapabilitiesCallback callback) {
  get_rgb_keyboard_capabilities_call_count_++;
  std::move(callback).Run(capabilities_);
}

void FakeRgbkbdClient::SetCapsLockState(bool enabled) {
  caps_lock_state_ = enabled;
}

}  // namespace ash
