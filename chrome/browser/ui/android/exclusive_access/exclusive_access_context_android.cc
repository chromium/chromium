// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "exclusive_access_context_android.h"

#include "base/functional/callback.h"
#include "base/notreached.h"
#include "chrome/android/chrome_jni_headers/ExclusiveAccessContext_jni.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/web_contents.h"

ExclusiveAccessContextAndroid::ExclusiveAccessContextAndroid(
    JNIEnv* env,
    const jni_zero::JavaRef<jobject>& j_fullscreen_manager,
    const jni_zero::JavaRef<jobject>& j_activity_tab_provider) {
  java_context_.Reset(Java_ExclusiveAccessContext_create(
      env, j_fullscreen_manager, j_activity_tab_provider));
}

ExclusiveAccessContextAndroid::~ExclusiveAccessContextAndroid() = default;

void ExclusiveAccessContextAndroid::Destroy(JNIEnv* env) {
  Java_ExclusiveAccessContext_destroy(env, java_context_);
}

Profile* ExclusiveAccessContextAndroid::GetProfile() {
  JNIEnv* env = jni_zero::AttachCurrentThread();
  auto java_profile =
      Java_ExclusiveAccessContext_getProfile(env, java_context_);
  return Profile::FromJavaObject(java_profile);
}

bool ExclusiveAccessContextAndroid::IsFullscreen() const {
  JNIEnv* env = jni_zero::AttachCurrentThread();
  return Java_ExclusiveAccessContext_isFullscreen(env, java_context_);
}

void ExclusiveAccessContextAndroid::EnterFullscreen(
    const url::Origin& origin,
    ExclusiveAccessBubbleType bubble_type,
    const int64_t display_id) {
  JNIEnv* env = jni_zero::AttachCurrentThread();
  Java_ExclusiveAccessContext_enterFullscreenModeForTab(env, java_context_);
}

void ExclusiveAccessContextAndroid::ExitFullscreen() {
  JNIEnv* env = jni_zero::AttachCurrentThread();
  Java_ExclusiveAccessContext_exitFullscreenModeForTab(env, java_context_);
}

void ExclusiveAccessContextAndroid::UpdateExclusiveAccessBubble(
    const ExclusiveAccessBubbleParams& params,
    ExclusiveAccessBubbleHideCallback first_hide_callback) {}

bool ExclusiveAccessContextAndroid::IsExclusiveAccessBubbleDisplayed() const {
  return false;
}

void ExclusiveAccessContextAndroid::OnExclusiveAccessUserInput() {}

content::WebContents*
ExclusiveAccessContextAndroid::GetWebContentsForExclusiveAccess() {
  JNIEnv* env = jni_zero::AttachCurrentThread();
  jni_zero::ScopedJavaLocalRef<jobject> j_web_contents =
      Java_ExclusiveAccessContext_getWebContentsForExclusiveAccess(
          env, java_context_);

  return content::WebContents::FromJavaWebContents(j_web_contents);
}

bool ExclusiveAccessContextAndroid::CanUserEnterFullscreen() const {
  return true;
}

bool ExclusiveAccessContextAndroid::CanUserExitFullscreen() const {
  return true;
}
