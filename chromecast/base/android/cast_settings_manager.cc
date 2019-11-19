// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/base/android/cast_settings_manager.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "chromecast/base/jni_headers/CastSettingsManager_jni.h"

namespace chromecast {

bool CastSettingsManager::UpdateGlobalDeviceName(
    const std::string& deviceName) {
  JNIEnv* env = base::android::AttachCurrentThread();
  return Java_CastSettingsManager_updateGlobalDeviceName(
      env, base::android::ConvertUTF8ToJavaString(env, deviceName));
}

bool CastSettingsManager::HasWriteSecureSettingsPermission() {
  JNIEnv* env = base::android::AttachCurrentThread();
  return Java_CastSettingsManager_hasWriteSecureSettingsPermission(env);
}

}  // namespace chromecast
