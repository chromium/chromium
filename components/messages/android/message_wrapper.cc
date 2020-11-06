// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/messages/android/message_wrapper.h"

#include "base/android/jni_string.h"
#include "base/logging.h"
#include "components/messages/android/jni_headers/MessageWrapper_jni.h"
#include "content/public/browser/web_contents.h"

namespace messages {

MessageWrapper::MessageWrapper(base::OnceClosure action_callback,
                               base::OnceClosure dismiss_callback)
    : action_callback_(std::move(action_callback)),
      dismiss_callback_(std::move(dismiss_callback)),
      message_dismissed_(false) {
  JNIEnv* env = base::android::AttachCurrentThread();
  java_message_wrapper_ =
      Java_MessageWrapper_create(env, reinterpret_cast<int64_t>(this));
}

MessageWrapper::~MessageWrapper() {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_MessageWrapper_clearNativePtr(env, java_message_wrapper_);
  CHECK(message_dismissed_);
}

void MessageWrapper::SetTitle(const base::string16& title) {
  JNIEnv* env = base::android::AttachCurrentThread();
  base::android::ScopedJavaLocalRef<jstring> jtitle =
      base::android::ConvertUTF16ToJavaString(env, title);
  Java_MessageWrapper_setTitle(env, java_message_wrapper_, jtitle);
}

void MessageWrapper::SetDescription(const base::string16& description) {
  JNIEnv* env = base::android::AttachCurrentThread();
  base::android::ScopedJavaLocalRef<jstring> jdescription =
      base::android::ConvertUTF16ToJavaString(env, description);
  Java_MessageWrapper_setDescription(env, java_message_wrapper_, jdescription);
}

void MessageWrapper::SetPrimaryButtonText(
    const base::string16& primary_button_text) {
  JNIEnv* env = base::android::AttachCurrentThread();
  base::android::ScopedJavaLocalRef<jstring> jprimary_button_text =
      base::android::ConvertUTF16ToJavaString(env, primary_button_text);
  Java_MessageWrapper_setPrimaryButtonText(env, java_message_wrapper_,
                                           jprimary_button_text);
}

void MessageWrapper::SetIconResourceId(int resource_id) {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_MessageWrapper_setIconResourceId(env, java_message_wrapper_,
                                        resource_id);
}

void MessageWrapper::HandleActionClick(JNIEnv* env) {
  if (!action_callback_.is_null())
    std::move(action_callback_).Run();
}

void MessageWrapper::HandleDismissCallback(JNIEnv* env) {
  message_dismissed_ = true;
  if (!dismiss_callback_.is_null())
    std::move(dismiss_callback_).Run();
}

const base::android::JavaRef<jobject>& MessageWrapper::GetJavaMessageWrapper()
    const {
  return java_message_wrapper_;
}

}  // namespace messages