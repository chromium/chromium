// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/messages/android/message_utils_bridge.h"

#include <jni.h>

#include "base/android/jni_android.h"
#include "components/messages/android/jni_headers/MessageUtilsBridge_jni.h"

namespace messages {

// static
bool MessageUtilsBridge::IsA11yEnabled() {
  JNIEnv* env = base::android::AttachCurrentThread();
  return Java_MessageUtilsBridge_isA11yEnabled(env);
}

}  // namespace messages