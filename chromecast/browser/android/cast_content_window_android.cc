// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/browser/android/cast_content_window_android.h"

#include <memory>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/logging.h"
#include "chromecast/browser/jni_headers/CastContentWindowAndroid_jni.h"
#include "content/public/browser/web_contents.h"
#include "ui/events/keycodes/keyboard_code_conversion_android.h"

namespace chromecast {

using base::android::ConvertUTF8ToJavaString;

namespace {

base::android::ScopedJavaLocalRef<jobject> CreateJavaWindow(
    jlong native_window,
    bool enable_touch_input,
    bool is_remote_control_mode,
    bool turn_on_screen,
    const std::string& session_id) {
  JNIEnv* env = base::android::AttachCurrentThread();
  return Java_CastContentWindowAndroid_create(
      env, native_window, enable_touch_input, is_remote_control_mode,
      turn_on_screen, ConvertUTF8ToJavaString(env, session_id));
}

constexpr char kContextInteractionId[] = "interactionId";
constexpr char kContextConversationId[] = "conversationId";
constexpr char kGestureConsumedCallbackClassName[] =
    "org/chromium/chromecast/shell/"
    "CastWebContentsComponent$GestureHandledCallback";

// Wraps the JNI gesture consumption handled callback for invocation from C++.
class GestureConsumedCallbackWrapper {
 public:
  GestureConsumedCallbackWrapper(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& callback)
      : env_(env), callback_(callback) {
    class_ = base::android::ScopedJavaGlobalRef<jclass>(
        base::android::GetClass(env_, kGestureConsumedCallbackClassName));
    callback_method_id_ =
        base::android::MethodID::Get<base::android::MethodID::TYPE_INSTANCE>(
            env_, class_.obj(), "invoke",
            "(Z"  // boolean handled
            ")V");
  }

  GestureConsumedCallbackWrapper& operator=(
      const GestureConsumedCallbackWrapper&) = delete;
  GestureConsumedCallbackWrapper(const GestureConsumedCallbackWrapper&) =
      delete;

  void Invoke(bool handled) {
    jboolean handled_value(handled);
    env_->CallVoidMethod(callback_.obj(), callback_method_id_, handled_value);
  }

 private:
  JNIEnv* env_;
  const base::android::ScopedJavaGlobalRef<jobject> callback_;
  base::android::ScopedJavaGlobalRef<jclass> class_;
  jmethodID callback_method_id_;
};

}  // namespace

CastContentWindowAndroid::CastContentWindowAndroid(
    mojom::CastWebViewParamsPtr params)
    : CastContentWindow(std::move(params)),
      web_contents_attached_(false),
      java_window_(CreateJavaWindow(reinterpret_cast<jlong>(this),
                                    params_->enable_touch_input,
                                    params_->is_remote_control_mode,
                                    params_->turn_on_screen,
                                    params_->session_id)) {}

CastContentWindowAndroid::~CastContentWindowAndroid() {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_CastContentWindowAndroid_onNativeDestroyed(env, java_window_);
}

void CastContentWindowAndroid::CreateWindow(
    mojom::ZOrder /* z_order */,
    VisibilityPriority visibility_priority) {
  if (web_contents_attached_) {
    RequestVisibility(visibility_priority);
    return;
  }
  JNIEnv* env = base::android::AttachCurrentThread();
  base::android::ScopedJavaLocalRef<jobject> java_web_contents =
      cast_web_contents()->web_contents()->GetJavaWebContents();

  Java_CastContentWindowAndroid_createWindowForWebContents(
      env, java_window_, java_web_contents,
      ConvertUTF8ToJavaString(env, params_->activity_id),
      static_cast<int>(visibility_priority));
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

void CastContentWindowAndroid::OnActivityStopped(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jcaller) {
  for (auto& observer : observers_) {
    observer->OnWindowDestroyed();
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

void CastContentWindowAndroid::SetHostContext(base::Value host_context) {
  auto* found_interaction_id = host_context.FindKey(kContextInteractionId);
  auto* found_conversation_id = host_context.FindKey(kContextConversationId);
  if (found_interaction_id && found_conversation_id) {
    int interaction_id = found_interaction_id->GetInt();
    std::string& conversation_id = found_conversation_id->GetString();
    JNIEnv* env = base::android::AttachCurrentThread();
    Java_CastContentWindowAndroid_setHostContext(
        env, java_window_, static_cast<int>(interaction_id),
        ConvertUTF8ToJavaString(env, conversation_id));
  } else {
    LOG(ERROR) << "Interaction ID or Conversation ID is not found";
  }
}

void CastContentWindowAndroid::RequestMoveOut() {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_CastContentWindowAndroid_requestMoveOut(env, java_window_);
}

void CastContentWindowAndroid::ConsumeGesture(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jcaller,
    int gesture_type,
    const base::android::JavaParamRef<jobject>& callback) {
  auto wrapper =
      std::make_unique<GestureConsumedCallbackWrapper>(env, callback);
  if (gesture_router()) {
    gesture_router()->ConsumeGesture(
        static_cast<GestureType>(gesture_type),
        base::BindOnce(&GestureConsumedCallbackWrapper::Invoke,
                       std::move(wrapper)));
    return;
  }
  wrapper->Invoke(false);
}

void CastContentWindowAndroid::OnVisibilityChange(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jcaller,
    int visibility_type) {
  NotifyVisibilityChange(static_cast<VisibilityType>(visibility_type));
}

}  // namespace chromecast
