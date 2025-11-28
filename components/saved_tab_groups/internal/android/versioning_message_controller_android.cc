// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/saved_tab_groups/internal/android/versioning_message_controller_android.h"

#include "base/android/callback_android.h"
#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "components/saved_tab_groups/internal/jni_headers/VersioningMessageControllerImpl_jni.h"

using base::android::JavaParamRef;

namespace tab_groups {
using MessageType = VersioningMessageController::MessageType;

VersioningMessageControllerAndroid::VersioningMessageControllerAndroid(
    VersioningMessageController* versioning_message_controller)
    : versioning_message_controller_(versioning_message_controller) {
  JNIEnv* env = base::android::AttachCurrentThread();
  java_obj_.Reset(env, Java_VersioningMessageControllerImpl_create(
                           env, reinterpret_cast<int64_t>(this)));
}

VersioningMessageControllerAndroid::~VersioningMessageControllerAndroid() {
  Java_VersioningMessageControllerImpl_clearNativePtr(
      base::android::AttachCurrentThread(), java_obj_);
}

base::android::ScopedJavaLocalRef<jobject>
VersioningMessageControllerAndroid::GetJavaObject(JNIEnv* env) {
  return base::android::ScopedJavaLocalRef<jobject>(java_obj_);
}

bool VersioningMessageControllerAndroid::IsInitialized(
    JNIEnv* env,
    const JavaParamRef<jobject>& j_caller) {
  return versioning_message_controller_->IsInitialized();
}

bool VersioningMessageControllerAndroid::ShouldShowMessageUi(
    JNIEnv* env,
    const JavaParamRef<jobject>& j_caller,
    jint j_message_type) {
  MessageType message_type = static_cast<MessageType>(j_message_type);
  return versioning_message_controller_->ShouldShowMessageUi(message_type);
}

void VersioningMessageControllerAndroid::ShouldShowMessageUiAsync(
    JNIEnv* env,
    const JavaParamRef<jobject>& j_caller,
    jint j_message_type,
    const JavaParamRef<jobject>& j_callback) {
  MessageType message_type = static_cast<MessageType>(j_message_type);
  base::OnceCallback<void(bool)> callback = base::BindOnce(
      [](const base::android::JavaRef<jobject>& j_callback, bool result) {
        base::android::RunBooleanCallbackAndroid(j_callback, result);
      },
      base::android::ScopedJavaGlobalRef<jobject>(j_callback));
  versioning_message_controller_->ShouldShowMessageUiAsync(message_type,
                                                           std::move(callback));
}

void VersioningMessageControllerAndroid::OnMessageUiShown(
    JNIEnv* env,
    const JavaParamRef<jobject>& j_caller,
    jint j_message_type) {
  MessageType message_type = static_cast<MessageType>(j_message_type);
  versioning_message_controller_->OnMessageUiShown(message_type);
}

void VersioningMessageControllerAndroid::OnMessageUiDismissed(
    JNIEnv* env,
    const JavaParamRef<jobject>& j_caller,
    jint j_message_type) {
  MessageType message_type = static_cast<MessageType>(j_message_type);
  versioning_message_controller_->OnMessageUiDismissed(message_type);
}

}  // namespace tab_groups

DEFINE_JNI(VersioningMessageControllerImpl)
