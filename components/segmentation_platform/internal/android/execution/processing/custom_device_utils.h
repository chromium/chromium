// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_ANDROID_EXECUTION_PROCESSING_CUSTOM_DEVICE_UTILS_H_
#define COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_ANDROID_EXECUTION_PROCESSING_CUSTOM_DEVICE_UTILS_H_

#include "base/android/jni_android.h"

namespace segmentation_platform::processing {

// A bridge class that forwards calls to Java class.
class CustomDeviceUtils {
 public:
  static int GetDevicePPI();
};

}  // namespace segmentation_platform::processing

#endif  // COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_ANDROID_EXECUTION_PROCESSING_CUSTOM_DEVICE_UTILS_H_
