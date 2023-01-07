// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/assistant/test_support/scoped_device_actions.h"

#include <utility>

namespace ash::assistant {

void ScopedDeviceActions::GetScreenBrightnessLevel(
    GetScreenBrightnessLevelCallback callback) {
  std::move(callback).Run(/*success=*/true, current_brightness_);
}

bool ScopedDeviceActions::OpenAndroidApp(const AndroidAppInfo& app_info) {
  return true;
}

AppStatus ScopedDeviceActions::GetAndroidAppStatus(
    const AndroidAppInfo& app_info) {
  return AppStatus::kAvailable;
}

}  // namespace ash::assistant
