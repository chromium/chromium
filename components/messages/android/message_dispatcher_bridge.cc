// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/messages/android/message_dispatcher_bridge.h"

#include <jni.h>

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "base/no_destructor.h"
#include "components/messages/android/jni_headers/MessageDispatcherBridge_jni.h"
#include "content/public/browser/web_contents.h"

namespace messages {

namespace {

MessageDispatcherBridge* g_message_dospatcher_bridge_for_testing = nullptr;

}  // namespace

// static
MessageDispatcherBridge* MessageDispatcherBridge::Get() {
  if (g_message_dospatcher_bridge_for_testing)
    return g_message_dospatcher_bridge_for_testing;
  static base::NoDestructor<MessageDispatcherBridge> instance;
  return instance.get();
}

// static
void MessageDispatcherBridge::SetInstanceForTesting(
    MessageDispatcherBridge* instance) {
  g_message_dospatcher_bridge_for_testing = instance;
}

bool MessageDispatcherBridge::EnqueueMessage(MessageWrapper* message,
                                             content::WebContents* web_contents,
                                             MessageScopeType scope_type,
                                             MessagePriority priority) {
  JNIEnv* env = base::android::AttachCurrentThread();
  if (Java_MessageDispatcherBridge_enqueueMessage(
          env, message->GetJavaMessageWrapper(),
          web_contents->GetJavaWebContents(), static_cast<int>(scope_type),
          priority == MessagePriority::kUrgent) == JNI_TRUE) {
    message->SetMessageEnqueued();
    return true;
  }
  return false;
}

void MessageDispatcherBridge::DismissMessage(MessageWrapper* message,
                                             content::WebContents* web_contents,
                                             DismissReason dismiss_reason) {
  base::android::ScopedJavaLocalRef<jobject> jmessage(
      message->GetJavaMessageWrapper());
  JNIEnv* env = base::android::AttachCurrentThread();
  message->HandleDismissCallback(env, static_cast<int>(dismiss_reason));
  // DismissMessage can be called in the process of WebContents destruction.
  // In this case WebContentsAndroid is already torn down. We shouldn't call
  // GetJavaWebContents() because it recreates WebContentsAndroid.
  if (!web_contents->IsBeingDestroyed()) {
    Java_MessageDispatcherBridge_dismissMessage(
        env, jmessage, web_contents->GetJavaWebContents(),
        static_cast<int>(dismiss_reason));
  }
}

}  // namespace messages
