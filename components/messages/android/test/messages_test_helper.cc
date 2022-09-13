// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/messages/android/test/messages_test_helper.h"

#include "components/messages/android/test/jni_headers/MessagesTestHelper_jni.h"

namespace messages {

int messages::MessagesTestHelper::GetMessageCount(
    ui::WindowAndroid* window_android) {
  JNIEnv* env = base::android::AttachCurrentThread();
  return Java_MessagesTestHelper_getMessageCount(
      env, window_android->GetJavaObject());
}

int messages::MessagesTestHelper::GetMessageIdentifier(
    ui::WindowAndroid* window_android,
    int index) {
  JNIEnv* env = base::android::AttachCurrentThread();
  return Java_MessagesTestHelper_getMessageIdentifier(
      env, window_android->GetJavaObject(), index);
}

}  // namespace messages
