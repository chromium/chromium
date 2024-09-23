// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEGMENTATION_PLATFORM_PUBLIC_ANDROID_PREDICTION_OPTIONS_ANDROID_H_
#define COMPONENTS_SEGMENTATION_PLATFORM_PUBLIC_ANDROID_PREDICTION_OPTIONS_ANDROID_H_

#include "base/android/jni_android.h"
#include "components/segmentation_platform/public/prediction_options.h"

using base::android::JavaParamRef;

namespace segmentation_platform {

class PredictionOptionsAndroid {
 public:
  static PredictionOptions ToNativePredictionOptions(
      JNIEnv* env,
      const base::android::JavaRef<jobject>& j_prediction_options);

  static void FromJavaParams(
      JNIEnv* env,
      const jlong target,
      const jboolean on_demand_execution,
      const jboolean can_update_cache_for_future_requests,
      const jboolean fallback_allowed);
};

}  // namespace segmentation_platform

#endif  // COMPONENTS_SEGMENTATION_PLATFORM_PUBLIC_ANDROID_PREDICTION_OPTIONS_ANDROID_H_
