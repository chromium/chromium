// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/extensions/extension_test_message_listener_android.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "chrome/browser/ui/android/extensions/test_support_jni_headers/ExtensionTestMessageListener_jni.h"

namespace extensions {

ExtensionTestMessageListenerAndroid::ExtensionTestMessageListenerAndroid(
    const std::string& expected_message,
    bool will_reply) {
  if (expected_message.empty()) {
    listener_ = std::make_unique<ExtensionTestMessageListener>(
        will_reply ? ReplyBehavior::kWillReply : ReplyBehavior::kWontReply);
  } else {
    listener_ = std::make_unique<ExtensionTestMessageListener>(
        expected_message,
        will_reply ? ReplyBehavior::kWillReply : ReplyBehavior::kWontReply);
  }
}

ExtensionTestMessageListenerAndroid::~ExtensionTestMessageListenerAndroid() =
    default;

void ExtensionTestMessageListenerAndroid::Destroy(JNIEnv* env) {
  delete this;
}

bool ExtensionTestMessageListenerAndroid::WasSatisfied(JNIEnv* env) {
  return listener_->was_satisfied();
}

void ExtensionTestMessageListenerAndroid::Reply(JNIEnv* env,
                                                const std::string& message) {
  listener_->Reply(message);
}

std::string ExtensionTestMessageListenerAndroid::GetMessage(JNIEnv* env) {
  return listener_->message();
}

int64_t JNI_ExtensionTestMessageListener_Create(
    JNIEnv* env,
    const std::string& expected_message,
    bool will_reply) {
  return reinterpret_cast<int64_t>(
      new ExtensionTestMessageListenerAndroid(expected_message, will_reply));
}

}  // namespace extensions

DEFINE_JNI(ExtensionTestMessageListener)
