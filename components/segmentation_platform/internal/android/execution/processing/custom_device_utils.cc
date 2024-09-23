// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/android/execution/processing/custom_device_utils.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "components/segmentation_platform/internal/jni_headers/CustomDeviceUtils_jni.h"

namespace segmentation_platform::processing {

// static
int CustomDeviceUtils::GetDevicePPI() {
  JNIEnv* env = base::android::AttachCurrentThread();
  return Java_CustomDeviceUtils_getDevicePPI(env);
}

}  // namespace segmentation_platform::processing
