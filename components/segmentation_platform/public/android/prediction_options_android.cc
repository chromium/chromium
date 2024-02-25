// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/public/android/prediction_options_android.h"

#include "base/memory/scoped_refptr.h"
#include "components/segmentation_platform/public/jni_headers/PredictionOptions_jni.h"
#include "components/segmentation_platform/public/prediction_options.h"

#include <jni.h>

namespace segmentation_platform {

// static
PredictionOptions PredictionOptionsAndroid::ToNativePredictionOptions(
    JNIEnv* env,
    const base::android::JavaRef<jobject>& j_prediction_options) {
  PredictionOptions prediction_options;
  Java_PredictionOptions_fillNativePredictionOptions(
      env, j_prediction_options,
      reinterpret_cast<intptr_t>(&prediction_options));

  return prediction_options;
}

void PredictionOptionsAndroid::FromJavaParams(
    JNIEnv* env,
    const jlong target,
    const jboolean on_demand_execution) {
  PredictionOptions* prediction_options =
      reinterpret_cast<PredictionOptions*>(target);

  prediction_options->on_demand_execution = on_demand_execution;
}

static void JNI_PredictionOptions_FillNative(
    JNIEnv* env,
    const jlong prediction_options_ptr,
    const jboolean on_demand_execution) {
  segmentation_platform::PredictionOptionsAndroid::FromJavaParams(
      env, prediction_options_ptr, on_demand_execution);
}

}  // namespace segmentation_platform
