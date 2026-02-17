// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ANDROID_EXTENSIONS_EXTENSION_TEST_MESSAGE_LISTENER_ANDROID_H_
#define CHROME_BROWSER_UI_ANDROID_EXTENSIONS_EXTENSION_TEST_MESSAGE_LISTENER_ANDROID_H_

#include <memory>
#include <string>

#include "extensions/test/extension_test_message_listener.h"
#include "third_party/jni_zero/jni_zero.h"

namespace extensions {

class ExtensionTestMessageListenerAndroid {
 public:
  ExtensionTestMessageListenerAndroid(const std::string& expected_message,
                                      bool will_reply);
  ExtensionTestMessageListenerAndroid(
      const ExtensionTestMessageListenerAndroid&) = delete;
  ExtensionTestMessageListenerAndroid& operator=(
      const ExtensionTestMessageListenerAndroid&) = delete;
  ~ExtensionTestMessageListenerAndroid();

  void Destroy(JNIEnv* env);
  bool WasSatisfied(JNIEnv* env);
  void Reply(JNIEnv* env, const std::string& message);
  std::string GetMessage(JNIEnv* env);

 private:
  std::unique_ptr<ExtensionTestMessageListener> listener_;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_UI_ANDROID_EXTENSIONS_EXTENSION_TEST_MESSAGE_LISTENER_ANDROID_H_
