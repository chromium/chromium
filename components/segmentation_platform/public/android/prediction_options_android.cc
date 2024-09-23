// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/public/android/prediction_options_android.h"

#include "base/memory/scoped_refptr.h"
#include "components/segmentation_platform/public/prediction_options.h"

#include <jni.h>

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "components/segmentation_platform/public/jni_headers/PredictionOptions_jni.h"

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
    const jboolean on_demand_execution,
    const jboolean can_update_cache_for_future_requests,
    const jboolean fallback_allowed) {
  PredictionOptions* prediction_options =
      reinterpret_cast<PredictionOptions*>(target);

  prediction_options->on_demand_execution = on_demand_execution;
  prediction_options->can_update_cache_for_future_requests =
      can_update_cache_for_future_requests;
  prediction_options->fallback_allowed = fallback_allowed;
}

static void JNI_PredictionOptions_FillNative(
    JNIEnv* env,
    const jlong prediction_options_ptr,
    const jboolean on_demand_execution,
    const jboolean can_update_cache_for_future_requests,
    const jboolean fallback_allowed) {
  segmentation_platform::PredictionOptionsAndroid::FromJavaParams(
      env, prediction_options_ptr, on_demand_execution,
      can_update_cache_for_future_requests, fallback_allowed);
}

}  // namespace segmentation_platform
