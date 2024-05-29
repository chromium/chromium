// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browser_ui/device_lock/android/device_lock_bridge.h"

#include "base/android/build_info.h"
#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "ui/android/window_android.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "components/browser_ui/device_lock/android/device_lock_bridge_jni_headers/DeviceLockBridge_jni.h"

using base::android::JavaParamRef;

DeviceLockBridge::DeviceLockBridge() {
  java_object_ = Java_DeviceLockBridge_create(
      base::android::AttachCurrentThread(), reinterpret_cast<intptr_t>(this));
}

DeviceLockBridge::~DeviceLockBridge() {
  Java_DeviceLockBridge_clearNativePointer(base::android::AttachCurrentThread(),
                                           java_object_);
}

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
  device_lock_confirmed_callback_ = std::move(callback);
  Java_DeviceLockBridge_launchDeviceLockUiBeforeRunningCallback(
      base::android::AttachCurrentThread(), java_object_,
      window_android->GetJavaObject());
}

void DeviceLockBridge::OnDeviceLockUiFinished(JNIEnv* env,
                                              bool is_device_lock_set) {
  std::move(device_lock_confirmed_callback_)
      .Run(/*device_lock_requirement_met=*/is_device_lock_set);
}

bool DeviceLockBridge::ShouldShowDeviceLockUi() {
  return RequiresDeviceLock() &&
         (!IsDeviceSecure() || !DeviceLockPageHasBeenPassed());
}

bool DeviceLockBridge::RequiresDeviceLock() {
  return base::android::BuildInfo::GetInstance()->is_automotive();
}

bool DeviceLockBridge::IsDeviceSecure() {
  return Java_DeviceLockBridge_isDeviceSecure(
      base::android::AttachCurrentThread());
}

bool DeviceLockBridge::DeviceLockPageHasBeenPassed() {
  return Java_DeviceLockBridge_deviceLockPageHasBeenPassed(
      base::android::AttachCurrentThread());
}
