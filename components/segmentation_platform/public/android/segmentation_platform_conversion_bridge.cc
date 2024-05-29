// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/public/android/segmentation_platform_conversion_bridge.h"

#include "base/android/jni_array.h"
#include "base/android/scoped_java_ref.h"
#include "base/memory/scoped_refptr.h"
#include "components/segmentation_platform/public/prediction_options.h"
#include "components/segmentation_platform/public/segment_selection_result.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "components/segmentation_platform/public/jni_headers/SegmentationPlatformConversionBridge_jni.h"

namespace segmentation_platform {

// static
ScopedJavaLocalRef<jobject>
SegmentationPlatformConversionBridge::CreateJavaSegmentSelectionResult(
    JNIEnv* env,
    const SegmentSelectionResult& result) {
  int selected_segment = result.segment.has_value()
                             ? result.segment.value()
                             : proto::SegmentId::OPTIMIZATION_TARGET_UNKNOWN;
  return Java_SegmentationPlatformConversionBridge_createSegmentSelectionResult(
      env, result.is_ready, selected_segment, result.rank.has_value(),
      result.rank.has_value() ? *result.rank : 0);
}

// static
ScopedJavaLocalRef<jobject>
SegmentationPlatformConversionBridge::CreateJavaClassificationResult(
    JNIEnv* env,
    const ClassificationResult& result) {
  return Java_SegmentationPlatformConversionBridge_createClassificationResult(
      env, static_cast<int>(result.status),
      base::android::ToJavaArrayOfStrings(env, result.ordered_labels));
}

}  // namespace segmentation_platform
