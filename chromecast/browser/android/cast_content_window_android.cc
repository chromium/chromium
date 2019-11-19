// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/browser/android/cast_content_window_android.h"

#include <memory>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "chromecast/browser/jni_headers/CastContentWindowAndroid_jni.h"
#include "content/public/browser/web_contents.h"
#include "ui/events/keycodes/keyboard_code_conversion_android.h"

namespace chromecast {

using base::android::ConvertUTF8ToJavaString;

namespace {

base::android::ScopedJavaLocalRef<jobject> CreateJavaWindow(
    jlong native_window,
    bool is_headless,
    bool enable_touch_input,
    bool is_remote_control_mode,
    bool turn_on_screen,
    const std::string& session_id) {
  JNIEnv* env = base::android::AttachCurrentThread();
  return Java_CastContentWindowAndroid_create(
      env, native_window, is_headless, enable_touch_input,
      is_remote_control_mode, turn_on_screen,
      ConvertUTF8ToJavaString(env, session_id));
}

}  // namespace

CastContentWindowAndroid::CastContentWindowAndroid(
    const CastContentWindow::CreateParams& params)
    : CastContentWindow(params),
      activity_id_(delegate_->GetId()),
      java_window_(CreateJavaWindow(reinterpret_cast<jlong>(this),
                                    params.is_headless,
                                    params.enable_touch_input,
                                    params.is_remote_control_mode,
                                    params.turn_on_screen,
                                    params.session_id)) {}

CastContentWindowAndroid::~CastContentWindowAndroid() {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_CastContentWindowAndroid_onNativeDestroyed(env, java_window_);
}

void CastContentWindowAndroid::CreateWindowForWebContents(
    CastWebContents* cast_web_contents,
    mojom::ZOrder /* z_order */,
    VisibilityPriority visibility_priority) {
  DCHECK(cast_web_contents);
  JNIEnv* env = base::android::AttachCurrentThread();
  base::android::ScopedJavaLocalRef<jobject> java_web_contents =
      cast_web_contents->web_contents()->GetJavaWebContents();

  Java_CastContentWindowAndroid_createWindowForWebContents(
      env, java_window_, java_web_contents,
      static_cast<int>(visibility_priority));
}

void CastContentWindowAndroid::GrantScreenAccess() {
  NOTIMPLEMENTED();
}

void CastContentWindowAndroid::RevokeScreenAccess() {
  NOTIMPLEMENTED();
}

void CastContentWindowAndroid::EnableTouchInput(bool enabled) {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_CastContentWindowAndroid_enableTouchInput(
      env, java_window_, static_cast<jboolean>(enabled));
}

void CastContentWindowAndroid::OnActivityStopped(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jcaller) {
  if (delegate_) {
    delegate_->OnWindowDestroyed();
  }
}

void CastContentWindowAndroid::RequestVisibility(
    VisibilityPriority visibility_priority) {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_CastContentWindowAndroid_requestVisibilityPriority(
      env, java_window_, static_cast<int>(visibility_priority));
}

void CastContentWindowAndroid::SetActivityContext(
    base::Value activity_context) {}

void CastContentWindowAndroid::SetHostContext(base::Value host_context) {}

void CastContentWindowAndroid::NotifyVisibilityChange(
    VisibilityType visibility_type) {
  if (delegate_) {
    delegate_->OnVisibilityChange(visibility_type);
  }
  for (auto& observer : observer_list_) {
    observer.OnVisibilityChange(visibility_type);
  }
}

void CastContentWindowAndroid::RequestMoveOut() {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_CastContentWindowAndroid_requestMoveOut(env, java_window_);
}

bool CastContentWindowAndroid::ConsumeGesture(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jcaller,
    int gesture_type) {
  if (delegate_) {
    return delegate_->ConsumeGesture(static_cast<GestureType>(gesture_type));
  }
  return false;
}

void CastContentWindowAndroid::OnVisibilityChange(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jcaller,
    int visibility_type) {
  NotifyVisibilityChange(static_cast<VisibilityType>(visibility_type));
}

base::android::ScopedJavaLocalRef<jstring> CastContentWindowAndroid::GetId(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jcaller) {
  return ConvertUTF8ToJavaString(env, activity_id_);
}

}  // namespace chromecast
