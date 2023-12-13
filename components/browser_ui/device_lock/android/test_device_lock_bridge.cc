// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browser_ui/device_lock/android/test_device_lock_bridge.h"

namespace autofill {

TestDeviceLockBridge::TestDeviceLockBridge() = default;
TestDeviceLockBridge::~TestDeviceLockBridge() = default;

bool TestDeviceLockBridge::ShouldShowDeviceLockUi() {
  return should_show_device_lock_ui_;
}

bool TestDeviceLockBridge::RequiresDeviceLock() {
  return should_show_device_lock_ui_;
}

void TestDeviceLockBridge::LaunchDeviceLockUiBeforeRunningCallback(
    ui::WindowAndroid* window_android,
    DeviceLockConfirmedCallback callback) {
  callback_ = std::move(callback);
  device_lock_ui_was_shown_ = true;
}

void TestDeviceLockBridge::SimulateDeviceLockComplete(bool is_device_lock_set) {
  std::move(callback_).Run(is_device_lock_set);
}

void TestDeviceLockBridge::SetShouldShowDeviceLockUi(
    bool should_show_device_lock_ui) {
  should_show_device_lock_ui_ = should_show_device_lock_ui;
}

bool TestDeviceLockBridge::device_lock_ui_was_shown() {
  return device_lock_ui_was_shown_;
}

}  // namespace autofill
