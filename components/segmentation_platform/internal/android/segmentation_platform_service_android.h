// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_ANDROID_SEGMENTATION_PLATFORM_SERVICE_ANDROID_H_
#define COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_ANDROID_SEGMENTATION_PLATFORM_SERVICE_ANDROID_H_

#include <jni.h>

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "base/memory/raw_ptr.h"
#include "base/supports_user_data.h"
#include "components/segmentation_platform/internal/jni_headers/SegmentationPlatformServiceImpl_shared_jni.h"
#include "components/segmentation_platform/public/segmentation_platform_service.h"

using base::android::JavaRef;
using base::android::ScopedJavaGlobalRef;
using base::android::ScopedJavaLocalRef;

namespace segmentation_platform {

// Helper class responsible for bridging the SegmentationPlatformService between
// C++ and Java.
class SegmentationPlatformServiceAndroid : public base::SupportsUserData::Data {
 public:
  explicit SegmentationPlatformServiceAndroid(
      SegmentationPlatformService* service);
  ~SegmentationPlatformServiceAndroid() override;

  void GetSelectedSegment(JNIEnv* env,
                          const JavaRef<jstring>& j_segmentation_key,
                          const JavaRef<jobject>& j_callback);

  void GetClassificationResult(JNIEnv* env,
                               const JavaRef<jstring>& j_segmentation_key,
                               const JavaRef<jobject>& j_prediction_options,
                               const JavaRef<jobject>& j_input_context,
                               const JavaRef<jobject>& j_callback);

  ScopedJavaLocalRef<jobject> GetCachedSegmentResult(
      JNIEnv* env,
      const JavaRef<jstring>& j_segmentation_key);

  void GetInputKeysForModel(JNIEnv* env,
                            const JavaRef<jstring>& j_segmentation_key,
                            const JavaRef<jobject>& j_callback);

  void CollectTrainingData(JNIEnv* env,
                           int32_t j_segment_id,
                           int64_t j_request_id,
                           int64_t j_ukm_source_id,
                           const JavaRef<jobject>& j_param,
                           const JavaRef<jobject>& j_callback);

  ScopedJavaLocalRef<jobject> GetJavaObject();

 private:
  // A reference to the Java counterpart of this class.  See
  // SegmentationPlatformServiceImpl.java.
  ScopedJavaGlobalRef<JSegmentationPlatformServiceImpl> java_obj_;

  // Not owned.
  raw_ptr<SegmentationPlatformService> segmentation_platform_service_;
};

}  // namespace segmentation_platform

#endif  // COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_ANDROID_SEGMENTATION_PLATFORM_SERVICE_ANDROID_H_
