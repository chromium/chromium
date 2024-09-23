// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/browser/android/cast_content_window_android.h"

#include <memory>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "components/media_control/browser/media_blocker.h"
#include "content/public/browser/web_contents_observer.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chromecast/browser/android/jni_headers/CastContentWindowAndroid_jni.h"

namespace chromecast {

using base::android::ConvertUTF8ToJavaString;

namespace {

base::android::ScopedJavaLocalRef<jobject> CreateJavaWindow(
    jlong native_window,
    bool enable_touch_input,
    bool turn_on_screen,
    bool keep_screen_on,
    const std::string& session_id,
    const int display_id) {
  JNIEnv* env = base::android::AttachCurrentThread();
  return Java_CastContentWindowAndroid_create(
      env, native_window, enable_touch_input, turn_on_screen, keep_screen_on,
      ConvertUTF8ToJavaString(env, session_id), static_cast<jint>(display_id));
}

bool ShouldRequestAudioFocus(bool is_remote_control_mode,
                             const media_control::MediaBlocker* media_blocker) {
  if (is_remote_control_mode) {
    return false;
  }
  if (!media_blocker) {
    return true;
  }
  return !media_blocker->media_loading_blocked();
}

}  // namespace

CastContentWindowAndroid::CastContentWindowAndroid(
    mojom::CastWebViewParamsPtr params)
    : CastContentWindow(std::move(params)),
      web_contents_attached_(false),
      java_window_(CreateJavaWindow(reinterpret_cast<jlong>(this),
                                    params_->enable_touch_input,
                                    params_->turn_on_screen,
                                    params_->keep_screen_on,
                                    params_->session_id,
                                    params_->display_id)) {}

CastContentWindowAndroid::~CastContentWindowAndroid() {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_CastContentWindowAndroid_onNativeDestroyed(env, java_window_);
}

void CastContentWindowAndroid::CreateWindow(
    mojom::ZOrder /* z_order */,
    VisibilityPriority visibility_priority) {
  if (web_contents_attached_) {
    return;
  }
  JNIEnv* env = base::android::AttachCurrentThread();

  content::WebContentsObserver::Observe(cast_web_contents()->web_contents());

  base::android::ScopedJavaLocalRef<jobject> java_web_contents =
      cast_web_contents()->web_contents()->GetJavaWebContents();

  Java_CastContentWindowAndroid_createWindowForWebContents(
      env, java_window_, java_web_contents,
      ConvertUTF8ToJavaString(env, params_->activity_id),
      ShouldRequestAudioFocus(params_->is_remote_control_mode,
                              cast_web_contents()->media_blocker()));
  web_contents_attached_ = true;
  cast_web_contents()->web_contents()->Focus();
}

void CastContentWindowAndroid::GrantScreenAccess() {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_CastContentWindowAndroid_grantScreenAccess(env, java_window_);
}

void CastContentWindowAndroid::RevokeScreenAccess() {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_CastContentWindowAndroid_revokeScreenAccess(env, java_window_);
}

void CastContentWindowAndroid::EnableTouchInput(bool enabled) {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_CastContentWindowAndroid_enableTouchInput(
      env, java_window_, static_cast<jboolean>(enabled));
}

void CastContentWindowAndroid::MediaStartedPlaying(
    const content::WebContentsObserver::MediaPlayerInfo& video_type,
    const content::MediaPlayerId& id) {
  JNIEnv* env = base::android::AttachCurrentThread();
  if (video_type.has_video) {
    Java_CastContentWindowAndroid_setAllowPictureInPicture(
        env, java_window_, static_cast<jboolean>(true));
  }
  Java_CastContentWindowAndroid_setMediaPlaying(env, java_window_,
                                                static_cast<jboolean>(true));
}

void CastContentWindowAndroid::MediaStoppedPlaying(
    const content::WebContentsObserver::MediaPlayerInfo& video_type,
    const content::MediaPlayerId& id,
    content::WebContentsObserver::MediaStoppedReason reason) {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_CastContentWindowAndroid_setAllowPictureInPicture(
      env, java_window_, static_cast<jboolean>(false));
  Java_CastContentWindowAndroid_setMediaPlaying(env, java_window_,
                                                static_cast<jboolean>(false));
}

void CastContentWindowAndroid::OnActivityStopped(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jcaller) {
  for (auto& observer : observers_) {
    observer->OnWindowDestroyed();
  }
}

void CastContentWindowAndroid::RequestVisibility(
    VisibilityPriority visibility_priority) {}

void CastContentWindowAndroid::OnVisibilityChange(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jcaller,
    int visibility_type) {
  NotifyVisibilityChange(static_cast<VisibilityType>(visibility_type));
}

}  // namespace chromecast
