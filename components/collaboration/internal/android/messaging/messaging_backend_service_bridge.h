// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COLLABORATION_INTERNAL_ANDROID_MESSAGING_MESSAGING_BACKEND_SERVICE_BRIDGE_H_
#define COMPONENTS_COLLABORATION_INTERNAL_ANDROID_MESSAGING_MESSAGING_BACKEND_SERVICE_BRIDGE_H_

#include <set>

#include "base/android/scoped_java_ref.h"
#include "base/supports_user_data.h"
#include "base/uuid.h"
#include "components/collaboration/public/messaging/messaging_backend_service.h"

namespace collaboration::messaging::android {

// Java bridge class responsible for marshalling calls between the C++
// MessagingBackendService to the Java MessagingBackendService shim layer.  This
// class and the Java class have no business logic and handle conversions and
// call marshalling.
class MessagingBackendServiceBridge
    : public MessagingBackendService::PersistentMessageObserver,
      public MessagingBackendService::InstantMessageDelegate,
      public base::SupportsUserData::Data {
 public:
  // Returns the Java object that can be used as a Java proxy for the passed in
  // MessagingBackendService.
  static base::android::ScopedJavaLocalRef<jobject>
  GetBridgeForMessagingBackendService(MessagingBackendService* service);

  static std::unique_ptr<MessagingBackendServiceBridge> CreateForTest(
      MessagingBackendService* service);

  MessagingBackendServiceBridge(const MessagingBackendServiceBridge&) = delete;
  MessagingBackendServiceBridge& operator=(
      const MessagingBackendServiceBridge&) = delete;

  ~MessagingBackendServiceBridge() override;

  // Returns the companion Java object for this bridge.
  base::android::ScopedJavaLocalRef<jobject> GetJavaObject();

  // Methods called from Java via JNI.
  bool IsInitialized(JNIEnv* env);
  base::android::ScopedJavaLocalRef<jobject> GetMessagesForTab(
      JNIEnv* env,
      jint j_local_tab_id,
      const base::android::JavaRef<jstring>& j_sync_tab_id,
      jint j_type);
  base::android::ScopedJavaLocalRef<jobject> GetMessagesForGroup(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& j_local_group_id,
      const base::android::JavaRef<jstring>& j_sync_group_id,
      jint j_type);
  base::android::ScopedJavaLocalRef<jobject> GetMessages(
      JNIEnv* env,
      jint j_type);
  base::android::ScopedJavaLocalRef<jobject> GetActivityLog(
      JNIEnv* env,
      const base::android::JavaRef<jstring>& j_collaboration_id);
  void ClearDirtyTabMessagesForGroup(
      JNIEnv* env,
      const base::android::JavaRef<jstring>& j_collaboration_id);
  void ClearPersistentMessage(
      JNIEnv* env,
      const base::android::JavaRef<jstring>& j_message_id,
      jint j_type);

  void RunInstantaneousMessageSuccessCallback(
      JNIEnv* env,
      jlong j_callback,
      jboolean j_result);

 private:
  friend class MessagingBackendServiceBridgeTest;
  explicit MessagingBackendServiceBridge(MessagingBackendService* service);

  // MessagingBackendService::PersistentMessageObserver implementation.
  void OnMessagingBackendServiceInitialized() override;
  void DisplayPersistentMessage(PersistentMessage message) override;
  void HidePersistentMessage(PersistentMessage message) override;

  // MessagingBackendService::InstantMessageDelegate implementation.
  void DisplayInstantaneousMessage(
      InstantMessage message,
      InstantMessageDelegate::SuccessCallback success_callback) override;
  void HideInstantaneousMessage(
      const std::set<base::Uuid>& message_ids) override;

  raw_ptr<MessagingBackendService> service_;

  // A reference to the Java counterpart of this class.  See
  // MessagingBackendService.java.
  base::android::ScopedJavaGlobalRef<jobject> java_ref_;
};

}  // namespace collaboration::messaging::android

#endif  // COMPONENTS_COLLABORATION_INTERNAL_ANDROID_MESSAGING_MESSAGING_BACKEND_SERVICE_BRIDGE_H_
