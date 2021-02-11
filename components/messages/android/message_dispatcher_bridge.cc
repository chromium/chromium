// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/messages/android/message_dispatcher_bridge.h"

#include <jni.h>

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "components/messages/android/jni_headers/MessageDispatcherBridge_jni.h"
#include "content/public/browser/web_contents.h"

namespace messages {

// static
void MessageDispatcherBridge::EnqueueMessage(
    MessageWrapper* message,
    content::WebContents* web_contents) {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_MessageDispatcherBridge_enqueueMessage(
      env, message->GetJavaMessageWrapper(),
      web_contents->GetJavaWebContents());
}

// static
void MessageDispatcherBridge::DismissMessage(
    MessageWrapper* message,
    content::WebContents* web_contents) {
  base::android::ScopedJavaLocalRef<jobject> jmessage(
      message->GetJavaMessageWrapper());
  JNIEnv* env = base::android::AttachCurrentThread();
  message->HandleDismissCallback(env);
  // DismissMessage can be called in the process of WebContents destruction.
  // In this case WebContentsAndroid is already torn down. We shouldn't call
  // GetJavaWebContents() because it recreates WebContentsAndroid.
  if (!web_contents->IsBeingDestroyed()) {
    Java_MessageDispatcherBridge_dismissMessage(
        env, jmessage, web_contents->GetJavaWebContents());
  }
}

}  // namespace messages