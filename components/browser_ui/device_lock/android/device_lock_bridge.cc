// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browser_ui/device_lock/android/device_lock_bridge.h"

#include "base/android/callback_android.h"
#include "base/android/device_info.h"
#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "ui/android/window_android.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "components/browser_ui/device_lock/android/device_lock_bridge_jni_headers/DeviceLockBridge_jni.h"

using base::android::JavaRef;

DeviceLockBridge::DeviceLockBridge() = default;

DeviceLockBridge::~DeviceLockBridge() = default;

void DeviceLockBridge::LaunchDeviceLockUiIfNeededBeforeRunningCallback(
    ui::WindowAndroid* window_android,
    DeviceLockRequirementMetCallback callback) {
  if (!ShouldShowDeviceLockUi()) {
    std::move(callback).Run(/*device_lock_requirement_met=*/true);
    return;
  }

  if (!window_android) {
    std::move(callback).Run(/*device_lock_requirement_met=*/false);
    return;
  }

  CHECK(callback);
  auto* env = base::android::AttachCurrentThread();
  Java_DeviceLockBridge_launchDeviceLockUiBeforeRunningCallback(
      env, window_android->GetJavaObject(),
      base::android::ToJniCallback(env, std::move(callback)));
}

bool DeviceLockBridge::ShouldShowDeviceLockUi() {
  return RequiresDeviceLock() &&
         (!IsDeviceSecure() || !DeviceLockPageHasBeenPassed());
}

bool DeviceLockBridge::RequiresDeviceLock() {
  return base::android::device_info::is_automotive();
}

bool DeviceLockBridge::IsDeviceSecure() {
  return Java_DeviceLockBridge_isDeviceSecure(
      base::android::AttachCurrentThread());
}

bool DeviceLockBridge::DeviceLockPageHasBeenPassed() {
  return Java_DeviceLockBridge_deviceLockPageHasBeenPassed(
      base::android::AttachCurrentThread());
}

DEFINE_JNI(DeviceLockBridge)
