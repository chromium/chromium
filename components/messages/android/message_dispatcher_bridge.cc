// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/messages/android/message_dispatcher_bridge.h"

#include <jni.h>

#include "base/android/jni_android.h"
#include "components/messages/android/jni_headers/MessageDispatcherBridge_jni.h"
#include "content/public/browser/web_contents.h"

namespace messages {

// static
void MessageDispatcherBridge::EnqueueMessage(
    const MessageWrapper& message,
    content::WebContents* web_contents) {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_MessageDispatcherBridge_enqueueMessage(
      env, message.GetJavaMessageWrapper(), web_contents->GetJavaWebContents());
}

// static
void MessageDispatcherBridge::DismissMessage(
    const MessageWrapper& message,
    content::WebContents* web_contents) {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_MessageDispatcherBridge_dismissMessage(
      env, message.GetJavaMessageWrapper(), web_contents->GetJavaWebContents());
}

}  // namespace messages