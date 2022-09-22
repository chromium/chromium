// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/public/android/segmentation_platform_conversion_bridge.h"

#include "components/segmentation_platform/public/jni_headers/SegmentationPlatformConversionBridge_jni.h"
#include "components/segmentation_platform/public/segment_selection_result.h"

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

}  // namespace segmentation_platform
