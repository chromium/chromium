// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/android/segmentation_platform_service_android.h"

#include <memory>

#include "base/android/callback_android.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/functional/bind.h"
#include "base/memory/scoped_refptr.h"
#include "components/segmentation_platform/public/android/input_context_android.h"
#include "components/segmentation_platform/public/android/prediction_options_android.h"
#include "components/segmentation_platform/public/android/segmentation_platform_conversion_bridge.h"
#include "components/segmentation_platform/public/input_context.h"
#include "components/segmentation_platform/public/prediction_options.h"
#include "components/segmentation_platform/public/result.h"
#include "components/segmentation_platform/public/segment_selection_result.h"
#include "components/segmentation_platform/public/segmentation_platform_service.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "components/segmentation_platform/internal/jni_headers/SegmentationPlatformServiceImpl_jni.h"

using base::android::AttachCurrentThread;
using base::android::ConvertJavaStringToUTF8;
using base::android::JavaParamRef;

namespace segmentation_platform {
namespace {

const char kSegmentationPlatformServiceBridgeKey[] =
    "segmentation_platform_service_bridge";

void RunGetSelectedSegmentCallback(const JavaRef<jobject>& j_callback,
                                   const SegmentSelectionResult& result) {
  JNIEnv* env = AttachCurrentThread();
  base::android::RunObjectCallbackAndroid(
      j_callback,
      SegmentationPlatformConversionBridge::CreateJavaSegmentSelectionResult(
          env, result));
}

void RunGetClassificationResultCallback(const JavaRef<jobject>& j_callback,
                                        const ClassificationResult& result) {
  JNIEnv* env = AttachCurrentThread();
  base::android::RunObjectCallbackAndroid(
      j_callback,
      SegmentationPlatformConversionBridge::CreateJavaClassificationResult(
          env, result));
}

}  // namespace

// This function is declared in segmentation_platform_service.h and
// should be linked in to any binary using
// SegmentationPlatformService::GetJavaObject. static
ScopedJavaLocalRef<jobject> SegmentationPlatformService::GetJavaObject(
    SegmentationPlatformService* service) {
  if (!service->GetUserData(kSegmentationPlatformServiceBridgeKey)) {
    service->SetUserData(
        kSegmentationPlatformServiceBridgeKey,
        std::make_unique<SegmentationPlatformServiceAndroid>(service));
  }

  SegmentationPlatformServiceAndroid* bridge =
      static_cast<SegmentationPlatformServiceAndroid*>(
          service->GetUserData(kSegmentationPlatformServiceBridgeKey));

  return bridge->GetJavaObject();
}

SegmentationPlatformServiceAndroid::SegmentationPlatformServiceAndroid(
    SegmentationPlatformService* segmentation_platform_service)
    : segmentation_platform_service_(segmentation_platform_service) {
  DCHECK(segmentation_platform_service_);
  JNIEnv* env = base::android::AttachCurrentThread();
  java_obj_.Reset(env, Java_SegmentationPlatformServiceImpl_create(
                           env, reinterpret_cast<int64_t>(this))
                           .obj());
}

SegmentationPlatformServiceAndroid::~SegmentationPlatformServiceAndroid() {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_SegmentationPlatformServiceImpl_clearNativePtr(env, java_obj_);
}

void SegmentationPlatformServiceAndroid::GetSelectedSegment(
    JNIEnv* env,
    const JavaParamRef<jobject>& jcaller,
    const JavaParamRef<jstring>& j_segmentation_key,
    const JavaParamRef<jobject>& jcallback) {
  segmentation_platform_service_->GetSelectedSegment(
      ConvertJavaStringToUTF8(env, j_segmentation_key),
      base::BindOnce(&RunGetSelectedSegmentCallback,
                     ScopedJavaGlobalRef<jobject>(jcallback)));
}

void SegmentationPlatformServiceAndroid::GetClassificationResult(
    JNIEnv* env,
    const JavaParamRef<jobject>& j_caller,
    const JavaParamRef<jstring>& j_segmentation_key,
    const JavaParamRef<jobject>& j_prediction_options,
    const JavaParamRef<jobject>& j_input_context,
    const JavaParamRef<jobject>& j_callback) {
  scoped_refptr<InputContext> native_input_context =
      InputContextAndroid::ToNativeInputContext(env, j_input_context);
  PredictionOptions native_prediction_options =
      PredictionOptionsAndroid::ToNativePredictionOptions(env,
                                                          j_prediction_options);

  segmentation_platform_service_->GetClassificationResult(
      ConvertJavaStringToUTF8(env, j_segmentation_key),
      native_prediction_options, native_input_context,
      base::BindOnce(&RunGetClassificationResultCallback,
                     ScopedJavaGlobalRef<jobject>(j_callback)));
}

ScopedJavaLocalRef<jobject>
SegmentationPlatformServiceAndroid::GetCachedSegmentResult(
    JNIEnv* env,
    const JavaParamRef<jobject>& jcaller,
    const JavaParamRef<jstring>& j_segmentation_key) {
  return SegmentationPlatformConversionBridge::CreateJavaSegmentSelectionResult(
      env, segmentation_platform_service_->GetCachedSegmentResult(
               ConvertJavaStringToUTF8(env, j_segmentation_key)));
}

ScopedJavaLocalRef<jobject>
SegmentationPlatformServiceAndroid::GetJavaObject() {
  return ScopedJavaLocalRef<jobject>(java_obj_);
}

}  // namespace segmentation_platform
