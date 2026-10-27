// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/device_signals/core/browser/android/os_security_info_provider.h"

#include "base/android/jni_android.h"
#include "base/check.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "components/device_signals/core/browser/android/jni_headers/DeviceSignalsUtils_jni.h"

namespace device_signals {

OsSecurityInfoProvider::OsSecurityInfoProvider() = default;

OsSecurityInfoProvider::~OsSecurityInfoProvider() = default;

bool OsSecurityInfoProvider::IsDeviceSecure() {
  JNIEnv* env = base::android::AttachCurrentThread();
  return Java_DeviceSignalsUtils_isDeviceSecure(env);
}

SettingValue OsSecurityInfoProvider::GetDiskEncryption() {
  JNIEnv* env = base::android::AttachCurrentThread();
  const int value = Java_DeviceSignalsUtils_getDiskEncryption(env);
  // The Java side only returns values of the generated SettingValue enum.
  CHECK(value >= static_cast<int>(SettingValue::UNKNOWN) &&
        value <= static_cast<int>(SettingValue::ENABLED));
  return static_cast<SettingValue>(value);
}

}  // namespace device_signals
