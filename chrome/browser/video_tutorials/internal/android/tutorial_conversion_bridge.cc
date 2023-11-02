// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/video_tutorials/internal/android/tutorial_conversion_bridge.h"

#include <memory>
#include <string>

#include "base/android/jni_string.h"
#include "chrome/browser/video_tutorials/internal/jni_headers/TutorialConversionBridge_jni.h"

namespace video_tutorials {

using base::android::ConvertUTF8ToJavaString;

ScopedJavaLocalRef<jobject> CreateJavaTutorialAndMaybeAddToList(
    JNIEnv* env,
    ScopedJavaLocalRef<jobject> jlist,
    const Tutorial& tutorial) {
  return Java_TutorialConversionBridge_createTutorialAndMaybeAddToList(
      env, jlist, static_cast<int>(tutorial.feature),
      ConvertUTF8ToJavaString(env, tutorial.title),
      ConvertUTF8ToJavaString(env, tutorial.video_url.spec()),
      ConvertUTF8ToJavaString(env, tutorial.poster_url.spec()),
      ConvertUTF8ToJavaString(env, tutorial.animated_gif_url.spec()),
      ConvertUTF8ToJavaString(env, tutorial.thumbnail_url.spec()),
      ConvertUTF8ToJavaString(env, tutorial.caption_url.spec()),
      ConvertUTF8ToJavaString(env, tutorial.share_url.spec()),
      tutorial.video_length);
}

ScopedJavaLocalRef<jobject> TutorialConversionBridge::CreateJavaTutorials(
    JNIEnv* env,
    const std::vector<Tutorial>& tutorials) {
  ScopedJavaLocalRef<jobject> jlist =
      Java_TutorialConversionBridge_createTutorialList(env);

  for (const auto& tutorial : tutorials)
    CreateJavaTutorialAndMaybeAddToList(env, jlist, tutorial);

  return jlist;
}

ScopedJavaLocalRef<jobject> TutorialConversionBridge::CreateJavaTutorial(
    JNIEnv* env,
    absl::optional<Tutorial> tutorial) {
  ScopedJavaLocalRef<jobject> jobj;
  if (tutorial.has_value()) {
    jobj = CreateJavaTutorialAndMaybeAddToList(
        env, ScopedJavaLocalRef<jobject>(), tutorial.value());
  }

  return jobj;
}

}  // namespace video_tutorials
