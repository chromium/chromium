// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/public/android/training_labels_android.h"

#include <jni.h>

#include "base/android/jni_string.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "components/segmentation_platform/public/jni_headers/TrainingLabels_jni.h"

namespace segmentation_platform {

// static
TrainingLabels TrainingLabelsAndroid::ToNativeTrainingLabels(
    JNIEnv* env,
    const base::android::JavaRef<jobject>& j_training_labels) {
  segmentation_platform::TrainingLabels training_labels;
  std::string output_metric_name =
      Java_TrainingLabels_getOutputMetricName(env, j_training_labels);
  base::HistogramBase::Sample32 output_metric_sample =
      Java_TrainingLabels_getOutputMetricSample(env, j_training_labels);
  training_labels.output_metric =
      std::make_pair(output_metric_name, output_metric_sample);

  return training_labels;
}

}  // namespace segmentation_platform

DEFINE_JNI(TrainingLabels)
