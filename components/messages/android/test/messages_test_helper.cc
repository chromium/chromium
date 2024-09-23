// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/messages/android/test/messages_test_helper.h"

#include "base/functional/callback_forward.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "components/messages/android/test/jni_headers/MessagesTestHelper_jni.h"

namespace messages {

MessagesTestHelper::MessagesTestHelper() {
  JNIEnv* env = jni_zero::AttachCurrentThread();
  jobj_ = Java_MessagesTestHelper_init(env, reinterpret_cast<int64_t>(this));
}

MessagesTestHelper::~MessagesTestHelper() = default;

int messages::MessagesTestHelper::GetMessageCount(
    ui::WindowAndroid* window_android) {
  JNIEnv* env = jni_zero::AttachCurrentThread();
  return Java_MessagesTestHelper_getMessageCount(
      env, window_android->GetJavaObject());
}

int MessagesTestHelper::GetMessageIdentifier(ui::WindowAndroid* window_android,
                                             int index) {
  JNIEnv* env = jni_zero::AttachCurrentThread();
  return Java_MessagesTestHelper_getMessageIdentifier(
      env, window_android->GetJavaObject(), index);
}

void MessagesTestHelper::AttachTestMessageDispatcherForTesting(
    ui::WindowAndroid* window_android) {
  JNIEnv* env = jni_zero::AttachCurrentThread();
  Java_MessagesTestHelper_attachTestMessageDispatcherForTesting(
      env, jobj_, window_android->GetJavaObject());
}

void MessagesTestHelper::ResetMessageDispatcherForTesting() {
  JNIEnv* env = jni_zero::AttachCurrentThread();
  Java_MessagesTestHelper_resetMessageDispatcherForTesting(env, jobj_);
}

void MessagesTestHelper::WaitForMessageEnqueued(base::OnceClosure callback) {
  on_message_enqueued_callback_ = std::move(callback);
}

void MessagesTestHelper::OnMessageEnqueued(JNIEnv* env) {
  if (on_message_enqueued_callback_) {
    std::move(on_message_enqueued_callback_).Run();
  }
}

}  // namespace messages
