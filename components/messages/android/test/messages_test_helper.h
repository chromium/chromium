// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MESSAGES_ANDROID_TEST_MESSAGES_TEST_HELPER_H_
#define COMPONENTS_MESSAGES_ANDROID_TEST_MESSAGES_TEST_HELPER_H_

#include <jni.h>

#include "ui/android/window_android.h"

namespace messages {

// |MessagesTestHelper| represents a helper class providing utility methods that
// are intended to be used in native tests using messages.
class MessagesTestHelper {
 public:
  int GetMessageCount(ui::WindowAndroid* window_android);
  int GetMessageIdentifier(ui::WindowAndroid* window_android, int index);
};

}  // namespace messages

#endif  // COMPONENTS_MESSAGES_ANDROID_TEST_MESSAGES_TEST_HELPER_H_
