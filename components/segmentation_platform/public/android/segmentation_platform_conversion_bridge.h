// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEGMENTATION_PLATFORM_PUBLIC_ANDROID_SEGMENTATION_PLATFORM_CONVERSION_BRIDGE_H_
#define COMPONENTS_SEGMENTATION_PLATFORM_PUBLIC_ANDROID_SEGMENTATION_PLATFORM_CONVERSION_BRIDGE_H_

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "components/segmentation_platform/public/prediction_options.h"
#include "components/segmentation_platform/public/result.h"
#include "components/segmentation_platform/public/segment_selection_result.h"

using base::android::ScopedJavaLocalRef;

namespace segmentation_platform {

// A helper class for creating Java objects required by the segmentation
// platform public API from their C++ counterparts.
class SegmentationPlatformConversionBridge {
 public:
  static ScopedJavaLocalRef<jobject> CreateJavaSegmentSelectionResult(
      JNIEnv* env,
      const SegmentSelectionResult& result);

  static ScopedJavaLocalRef<jobject> CreateJavaClassificationResult(
      JNIEnv* env,
      const ClassificationResult& result);
};

}  // namespace segmentation_platform

#endif  // COMPONENTS_SEGMENTATION_PLATFORM_PUBLIC_ANDROID_SEGMENTATION_PLATFORM_CONVERSION_BRIDGE_H_
