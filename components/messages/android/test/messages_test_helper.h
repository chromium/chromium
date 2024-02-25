// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MESSAGES_ANDROID_TEST_MESSAGES_TEST_HELPER_H_
#define COMPONENTS_MESSAGES_ANDROID_TEST_MESSAGES_TEST_HELPER_H_

#include <jni.h>

#include "base/functional/callback_forward.h"
#include "ui/android/window_android.h"

namespace messages {

// |MessagesTestHelper| represents a helper class providing utility methods that
// are intended to be used in native tests using messages.
class MessagesTestHelper {
 public:
  MessagesTestHelper();
  ~MessagesTestHelper();

  MessagesTestHelper(const MessagesTestHelper&) = delete;
  MessagesTestHelper& operator=(const MessagesTestHelper&) = delete;

  int GetMessageCount(ui::WindowAndroid* window_android);
  int GetMessageIdentifier(ui::WindowAndroid* window_android, int index);

  // Attach a test-only simplified message dispatcher to the window android.
  // This is required to listen to events like message enqueued.
  void AttachTestMessageDispatcherForTesting(ui::WindowAndroid* window_android);

  // Reset the dispatcher being set in |AttachTestMessageDispatcherForTesting|.
  void ResetMessageDispatcherForTesting();

  // Register a callback to be called when message is enqueued.
  void WaitForMessageEnqueued(base::OnceClosure on_changed_callback);

  // JNI method
  void OnMessageEnqueued(JNIEnv* env);

 private:
  base::android::ScopedJavaGlobalRef<jobject> jobj_;

  base::OnceClosure on_message_enqueued_callback_;
};

}  // namespace messages

#endif  // COMPONENTS_MESSAGES_ANDROID_TEST_MESSAGES_TEST_HELPER_H_
