
// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSER_UI_DEVICE_LOCK_ANDROID_TEST_DEVICE_LOCK_BRIDGE_H_
#define COMPONENTS_BROWSER_UI_DEVICE_LOCK_ANDROID_TEST_DEVICE_LOCK_BRIDGE_H_

#include "components/browser_ui/device_lock/android/device_lock_bridge.h"

namespace autofill {
// TODO(b/316154125): Change other unit tests that reference
// `TestDeviceLockBridge` to use this class.
class TestDeviceLockBridge : public DeviceLockBridge {
 public:
  TestDeviceLockBridge();

  TestDeviceLockBridge(const TestDeviceLockBridge&) = delete;
  TestDeviceLockBridge& operator=(const TestDeviceLockBridge&) = delete;

  ~TestDeviceLockBridge() override;

  bool ShouldShowDeviceLockUi() override;

  bool RequiresDeviceLock() override;

  void LaunchDeviceLockUiBeforeRunningCallback(
      ui::WindowAndroid* window_android,
      DeviceLockConfirmedCallback callback) override;

  void SimulateDeviceLockComplete(bool is_device_lock_set);

  void SetShouldShowDeviceLockUi(bool should_show_device_lock_ui);

  bool device_lock_ui_was_shown();

 private:
  bool should_show_device_lock_ui_ = false;
  bool device_lock_ui_was_shown_ = false;
  DeviceLockConfirmedCallback callback_;
};

}  // namespace autofill

#endif  // COMPONENTS_BROWSER_UI_DEVICE_LOCK_ANDROID_TEST_DEVICE_LOCK_BRIDGE_H_
