// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/video_tutorials/internal/android/video_tutorial_service_bridge.h"

#include <memory>
#include <vector>

#include "base/android/callback_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/functional/bind.h"
#include "chrome/browser/video_tutorials/internal/android/tutorial_conversion_bridge.h"
#include "chrome/browser/video_tutorials/internal/jni_headers/VideoTutorialServiceBridge_jni.h"
#include "chrome/browser/video_tutorials/tutorial.h"

using base::android::AttachCurrentThread;

namespace video_tutorials {
namespace {

const char kVideoTutorialServiceBridgeKey[] = "video_tutorial_service_bridge";

void RunGetMultipleTutorialsCallback(const JavaRef<jobject>& j_callback,
                                     std::vector<Tutorial> tutorials) {
  JNIEnv* env = AttachCurrentThread();
  RunObjectCallbackAndroid(
      j_callback,
      TutorialConversionBridge::CreateJavaTutorials(env, std::move(tutorials)));
}

void RunGetSingleTutorialCallback(const JavaRef<jobject>& j_callback,
                                  absl::optional<Tutorial> tutorial) {
  JNIEnv* env = AttachCurrentThread();
  RunObjectCallbackAndroid(
      j_callback, TutorialConversionBridge::CreateJavaTutorial(env, tutorial));
}

}  // namespace

// static
ScopedJavaLocalRef<jobject>
VideoTutorialServiceBridge::GetBridgeForVideoTutorialService(
    VideoTutorialService* video_tutorial_service) {
  if (!video_tutorial_service->GetUserData(kVideoTutorialServiceBridgeKey)) {
    video_tutorial_service->SetUserData(
        kVideoTutorialServiceBridgeKey,
        std::make_unique<VideoTutorialServiceBridge>(video_tutorial_service));
  }

  VideoTutorialServiceBridge* bridge = static_cast<VideoTutorialServiceBridge*>(
      video_tutorial_service->GetUserData(kVideoTutorialServiceBridgeKey));

  return ScopedJavaLocalRef<jobject>(bridge->java_obj_);
}

VideoTutorialServiceBridge::VideoTutorialServiceBridge(
    VideoTutorialService* video_tutorial_service)
    : video_tutorial_service_(video_tutorial_service) {
  DCHECK(video_tutorial_service_);
  JNIEnv* env = base::android::AttachCurrentThread();
  java_obj_.Reset(env, Java_VideoTutorialServiceBridge_create(
                           env, reinterpret_cast<int64_t>(this))
                           .obj());
}

VideoTutorialServiceBridge::~VideoTutorialServiceBridge() {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_VideoTutorialServiceBridge_clearNativePtr(env, java_obj_);
}

void VideoTutorialServiceBridge::GetTutorials(
    JNIEnv* env,
    const JavaParamRef<jobject>& jcaller,
    const JavaParamRef<jobject>& jcallback) {
  video_tutorial_service_->GetTutorials(
      base::BindOnce(&RunGetMultipleTutorialsCallback,
                     ScopedJavaGlobalRef<jobject>(jcallback)));
}

void VideoTutorialServiceBridge::GetTutorial(
    JNIEnv* env,
    const JavaParamRef<jobject>& jcaller,
    jint j_feature,
    const JavaParamRef<jobject>& jcallback) {
  video_tutorial_service_->GetTutorial(
      static_cast<FeatureType>(j_feature),
      base::BindOnce(&RunGetSingleTutorialCallback,
                     ScopedJavaGlobalRef<jobject>(jcallback)));
}

ScopedJavaLocalRef<jobjectArray>
VideoTutorialServiceBridge::GetSupportedLanguages(
    JNIEnv* env,
    const JavaParamRef<jobject>& jcaller) {
  return base::android::ToJavaArrayOfStrings(
      env, video_tutorial_service_->GetSupportedLanguages());
}

ScopedJavaLocalRef<jobjectArray>
VideoTutorialServiceBridge::GetAvailableLanguagesForTutorial(
    JNIEnv* env,
    const JavaParamRef<jobject>& jcaller,
    jint j_feature) {
  return base::android::ToJavaArrayOfStrings(
      env, video_tutorial_service_->GetAvailableLanguagesForTutorial(
               static_cast<FeatureType>(j_feature)));
}

ScopedJavaLocalRef<jstring> VideoTutorialServiceBridge::GetPreferredLocale(
    JNIEnv* env,
    const JavaParamRef<jobject>& jcaller) {
  absl::optional<std::string> locale =
      video_tutorial_service_->GetPreferredLocale();
  return locale.has_value()
             ? base::android::ConvertUTF8ToJavaString(env, locale.value())
             : ScopedJavaLocalRef<jstring>();
}

void VideoTutorialServiceBridge::SetPreferredLocale(
    JNIEnv* env,
    const JavaParamRef<jobject>& jcaller,
    jstring j_locale) {
  std::string locale = base::android::ConvertJavaStringToUTF8(env, j_locale);
  video_tutorial_service_->SetPreferredLocale(locale);
}

}  // namespace video_tutorials
