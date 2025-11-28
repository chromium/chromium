// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "exclusive_access_context_android.h"

#include "base/functional/callback.h"
#include "base/notreached.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/android/chrome_jni_headers/ExclusiveAccessContext_jni.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/android/exclusive_access/exclusive_access_bubble_android.h"
#include "content/public/browser/web_contents.h"

ExclusiveAccessContextAndroid::ExclusiveAccessContextAndroid(
    JNIEnv* env,
    const jni_zero::JavaRef<jobject>& j_context,
    const jni_zero::JavaRef<jobject>& j_fullscreen_manager,
    const jni_zero::JavaRef<jobject>& j_activity_tab_provider) {
  java_context_.Reset(Java_ExclusiveAccessContext_create(
      env, j_context, j_fullscreen_manager, j_activity_tab_provider));
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
    FullscreenTabParams fullscreen_tab_params) {
  JNIEnv* env = jni_zero::AttachCurrentThread();
  Java_ExclusiveAccessContext_enterFullscreenModeForTab(
      env, java_context_, fullscreen_tab_params.display_id,
      fullscreen_tab_params.prefers_navigation_bar,
      fullscreen_tab_params.prefers_status_bar);

  auto params = ExclusiveAccessBubbleParams{origin, bubble_type};
  if (!exclusive_access_bubble_) {
    exclusive_access_bubble_ = std::make_unique<ExclusiveAccessBubbleAndroid>(
        params, base::NullCallback(), java_context_);
  }
  {
    exclusive_access_bubble_->Update(params, base::NullCallback());
  }
}

void ExclusiveAccessContextAndroid::ExitFullscreen() {
  JNIEnv* env = jni_zero::AttachCurrentThread();
  Java_ExclusiveAccessContext_exitFullscreenModeForTab(env, java_context_);
}

void ExclusiveAccessContextAndroid::UpdateExclusiveAccessBubble(
    const ExclusiveAccessBubbleParams& params,
    ExclusiveAccessBubbleHideCallback first_hide_callback) {
  if (!params.has_download &&
      params.type == EXCLUSIVE_ACCESS_BUBBLE_TYPE_NONE) {
    if (first_hide_callback) {
      std::move(first_hide_callback)
          .Run(ExclusiveAccessBubbleHideReason::kNotShown);
    }

    // If we intend to close the bubble but it has already been deleted no
    // action is needed.
    if (!exclusive_access_bubble_) {
      return;
    }
    // Exit if we've already queued up a task to close the bubble.
    if (exclusive_access_bubble_destruction_task_id_) {
      return;
    }
    // `HideImmediately()` will trigger a callback for the current bubble with
    // `ExclusiveAccessBubbleHideReason::kInterrupted` if available.
    exclusive_access_bubble_->HideImmediately();

    // Perform the destroy async. State updates in the exclusive access bubble
    // view may call back into this method. This otherwise results in premature
    // deletion of the bubble view and UAFs. See crbug.com/1426521.
    exclusive_access_bubble_destruction_task_id_ =
        exclusive_access_bubble_cancelable_task_tracker_.PostTask(
            base::SingleThreadTaskRunner::GetCurrentDefault().get(), FROM_HERE,
            base::BindOnce(
                &ExclusiveAccessContextAndroid::DestroyAnyExclusiveAccessBubble,
                GetAsWeakPtr()));
    return;
  }

  if (exclusive_access_bubble_) {
    if (exclusive_access_bubble_destruction_task_id_) {
      // We previously posted a destruction task, but now we want to reuse the
      // bubble. Cancel the destruction task.
      exclusive_access_bubble_cancelable_task_tracker_.TryCancel(
          exclusive_access_bubble_destruction_task_id_.value());
      exclusive_access_bubble_destruction_task_id_.reset();
    }
    exclusive_access_bubble_->Update(params, std::move(first_hide_callback));
    return;
  }

  exclusive_access_bubble_ = std::make_unique<ExclusiveAccessBubbleAndroid>(
      params, std::move(first_hide_callback), java_context_);
}

void ExclusiveAccessContextAndroid::DestroyAnyExclusiveAccessBubble() {
  exclusive_access_bubble_.reset();
  exclusive_access_bubble_destruction_task_id_.reset();
}

bool ExclusiveAccessContextAndroid::IsExclusiveAccessBubbleDisplayed() const {
  return exclusive_access_bubble_ && exclusive_access_bubble_->IsVisible();
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

void ExclusiveAccessContextAndroid::ForceActiveTab(
    JNIEnv* env,
    const jni_zero::JavaRef<jobject>& j_tab) {
  Java_ExclusiveAccessContext_forceActiveTab(env, java_context_, j_tab);
}

DEFINE_JNI(ExclusiveAccessContext)
