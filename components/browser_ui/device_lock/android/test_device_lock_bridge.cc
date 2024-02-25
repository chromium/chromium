// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browser_ui/device_lock/android/test_device_lock_bridge.h"

namespace autofill {

TestDeviceLockBridge::TestDeviceLockBridge() = default;
TestDeviceLockBridge::~TestDeviceLockBridge() = default;

void TestDeviceLockBridge::LaunchDeviceLockUiIfNeededBeforeRunningCallback(
    ui::WindowAndroid* window_android,
    DeviceLockRequirementMetCallback callback) {
  did_start_checking_device_lock_requirements_ = true;
  callback_ = std::move(callback);
}

void TestDeviceLockBridge::SimulateFinishedCheckingDeviceLockRequirements(
    bool are_device_lock_requirements_met) {
  std::move(callback_).Run(are_device_lock_requirements_met);
}

}  // namespace autofill
