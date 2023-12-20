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

  void LaunchDeviceLockUiIfNeededBeforeRunningCallback(
      ui::WindowAndroid* window_android,
      DeviceLockRequirementMetCallback callback) override;

  void SimulateFinishedCheckingDeviceLockRequirements(
      bool are_device_lock_requirements_met);

  bool did_start_checking_device_lock_requirements() {
    return did_start_checking_device_lock_requirements_;
  }

 private:
  bool did_start_checking_device_lock_requirements_ = false;
  DeviceLockRequirementMetCallback callback_;
};

}  // namespace autofill

#endif  // COMPONENTS_BROWSER_UI_DEVICE_LOCK_ANDROID_TEST_DEVICE_LOCK_BRIDGE_H_
