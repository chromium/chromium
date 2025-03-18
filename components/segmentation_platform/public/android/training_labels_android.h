// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEGMENTATION_PLATFORM_PUBLIC_ANDROID_TRAINING_LABELS_ANDROID_H_
#define COMPONENTS_SEGMENTATION_PLATFORM_PUBLIC_ANDROID_TRAINING_LABELS_ANDROID_H_

#include "base/android/jni_android.h"
#include "components/segmentation_platform/public/segmentation_platform_service.h"

using base::android::JavaParamRef;

namespace segmentation_platform {

class TrainingLabelsAndroid {
 public:
  static TrainingLabels ToNativeTrainingLabels(
      JNIEnv* env,
      const base::android::JavaRef<jobject>& j_training_labels);
};

}  // namespace segmentation_platform

#endif  // COMPONENTS_SEGMENTATION_PLATFORM_PUBLIC_ANDROID_TRAINING_LABELS_ANDROID_H_
