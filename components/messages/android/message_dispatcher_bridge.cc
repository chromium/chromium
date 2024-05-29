// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/messages/android/message_dispatcher_bridge.h"

#include <jni.h>

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "base/no_destructor.h"
#include "content/public/browser/web_contents.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "components/messages/android/jni_headers/MessageDispatcherBridge_jni.h"

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

MessageDispatcherBridge::MessageDispatcherBridge() = default;
MessageDispatcherBridge::~MessageDispatcherBridge() = default;

bool MessageDispatcherBridge::EnqueueMessage(MessageWrapper* message,
                                             content::WebContents* web_contents,
                                             MessageScopeType scope_type,
                                             MessagePriority priority) {
  JNIEnv* env = base::android::AttachCurrentThread();
  if (Java_MessageDispatcherBridge_enqueueMessage(
          env, message->GetJavaMessageWrapper(),
          web_contents->GetJavaWebContents(), static_cast<int>(scope_type),
          priority == MessagePriority::kUrgent) == JNI_TRUE) {
    message->SetMessageEnqueued(
        web_contents->GetTopLevelNativeWindow()->GetJavaObject());
    return true;
  }
  return false;
}

bool MessageDispatcherBridge::EnqueueWindowScopedMessage(
    MessageWrapper* message,
    ui::WindowAndroid* window_android,
    MessagePriority priority) {
  JNIEnv* env = base::android::AttachCurrentThread();
  if (Java_MessageDispatcherBridge_enqueueWindowScopedMessage(
          env, message->GetJavaMessageWrapper(),
          window_android->GetJavaObject(),
          priority == MessagePriority::kUrgent) == JNI_TRUE) {
    message->SetMessageEnqueued(window_android->GetJavaObject());
    return true;
  }
  return false;
}

void MessageDispatcherBridge::DismissMessage(MessageWrapper* message,
                                             DismissReason dismiss_reason) {
  JNIEnv* env = base::android::AttachCurrentThread();
  base::android::ScopedJavaLocalRef<jobject> jmessage(
      message->GetJavaMessageWrapper());
  base::android::ScopedJavaLocalRef<jobject> java_window_android(
      message->java_window_android());
  message->HandleDismissCallback(env, static_cast<int>(dismiss_reason));
  Java_MessageDispatcherBridge_dismissMessage(
      env, jmessage, java_window_android, static_cast<int>(dismiss_reason));
}

int MessageDispatcherBridge::MapToJavaDrawableId(int resource_id) {
  DCHECK(resource_id_mapper_);
  return resource_id_mapper_.Run(resource_id);
}

void MessageDispatcherBridge::Initialize(ResourceIdMapper resource_id_mapper) {
  resource_id_mapper_ = std::move(resource_id_mapper);
  // resource_id_mapper_ will only be initialized in an embedder that supports
  // the Messages UI.
  messages_enabled_for_embedder_ = true;
}

}  // namespace messages
