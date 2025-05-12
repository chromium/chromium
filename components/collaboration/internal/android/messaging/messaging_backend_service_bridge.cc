// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/collaboration/internal/android/messaging/messaging_backend_service_bridge.h"

#include <memory>
#include <optional>
#include <set>

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/functional/callback.h"
#include "base/memory/ptr_util.h"
#include "base/notreached.h"
#include "base/uuid.h"
#include "components/collaboration/internal/android/messaging/conversion_utils.h"
#include "components/collaboration/public/messaging/activity_log.h"
#include "components/collaboration/public/messaging/message.h"
#include "components/saved_tab_groups/public/android/tab_group_sync_conversions_bridge.h"
#include "components/saved_tab_groups/public/android/tab_group_sync_conversions_utils.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "components/collaboration/internal/messaging_jni_headers/MessagingBackendServiceBridge_jni.h"

using base::android::ConvertJavaStringToUTF16;

namespace collaboration::messaging::android {
namespace {
const char kMessagingBackendServiceBridgeUserDataKey[] =
    "messaging_backend_service";
constexpr int32_t kInvalidTabId = -1;
}  // namespace

// static
base::android::ScopedJavaLocalRef<jobject>
MessagingBackendServiceBridge::GetBridgeForMessagingBackendService(
    MessagingBackendService* service) {
  if (!service->GetUserData(kMessagingBackendServiceBridgeUserDataKey)) {
    service->SetUserData(
        kMessagingBackendServiceBridgeUserDataKey,
        base::WrapUnique(new MessagingBackendServiceBridge(service)));
  }

  MessagingBackendServiceBridge* bridge =
      static_cast<MessagingBackendServiceBridge*>(
          service->GetUserData(kMessagingBackendServiceBridgeUserDataKey));
  return bridge->GetJavaObject();
}

// static
std::unique_ptr<MessagingBackendServiceBridge>
MessagingBackendServiceBridge::CreateForTest(MessagingBackendService* service) {
  MessagingBackendServiceBridge* bridge =
      new MessagingBackendServiceBridge(service);
  return base::WrapUnique(bridge);
}

MessagingBackendServiceBridge::MessagingBackendServiceBridge(
    MessagingBackendService* service)
    : service_(service) {
  java_ref_.Reset(Java_MessagingBackendServiceBridge_create(
      base::android::AttachCurrentThread(), reinterpret_cast<intptr_t>(this)));

  service_->AddPersistentMessageObserver(this);
  service_->SetInstantMessageDelegate(this);
}

MessagingBackendServiceBridge::~MessagingBackendServiceBridge() {
  service_->SetInstantMessageDelegate(nullptr);
  service_->RemovePersistentMessageObserver(this);

  Java_MessagingBackendServiceBridge_onNativeDestroyed(
      base::android::AttachCurrentThread(), java_ref_);
}

base::android::ScopedJavaLocalRef<jobject>
MessagingBackendServiceBridge::GetJavaObject() {
  return base::android::ScopedJavaLocalRef<jobject>(java_ref_);
}

bool MessagingBackendServiceBridge::IsInitialized(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& j_caller) {
  return service_->IsInitialized();
}

base::android::ScopedJavaLocalRef<jobject>
MessagingBackendServiceBridge::GetMessagesForTab(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& j_caller,
    jint j_local_tab_id,
    const base::android::JavaParamRef<jstring>& j_sync_tab_id,
    jint j_type) {
  auto type = static_cast<PersistentNotificationType>(j_type);
  std::optional<PersistentNotificationType> type_opt = std::make_optional(type);
  if (type == PersistentNotificationType::UNDEFINED) {
    type_opt = std::nullopt;
  }

  if (j_local_tab_id != kInvalidTabId) {
    CHECK(!j_sync_tab_id);
    tab_groups::LocalTabID tab_id = tab_groups::FromJavaTabId(j_local_tab_id);
    auto messages = service_->GetMessagesForTab(tab_id, type_opt);
    return PersistentMessagesToJava(env, messages);
  }
  if (j_sync_tab_id) {
    CHECK(j_local_tab_id == kInvalidTabId);
    std::string sync_tab_id_str =
        base::android::ConvertJavaStringToUTF8(env, j_sync_tab_id);
    auto tab_id = base::Uuid::ParseLowercase(sync_tab_id_str);
    auto messages = service_->GetMessagesForTab(tab_id, type_opt);
    return PersistentMessagesToJava(env, messages);
  }

  NOTREACHED();
}

base::android::ScopedJavaLocalRef<jobject>
MessagingBackendServiceBridge::GetMessagesForGroup(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& j_caller,
    const base::android::JavaParamRef<jobject>& j_local_group_id,
    const base::android::JavaParamRef<jstring>& j_sync_group_id,
    jint j_type) {
  auto type = static_cast<PersistentNotificationType>(j_type);
  std::optional<PersistentNotificationType> type_opt = std::make_optional(type);
  if (type == PersistentNotificationType::UNDEFINED) {
    type_opt = std::nullopt;
  }

  if (j_local_group_id) {
    CHECK(!j_sync_group_id);
    auto group_id =
        tab_groups::TabGroupSyncConversionsBridge::FromJavaTabGroupId(
            env, j_local_group_id);
    auto messages = service_->GetMessagesForGroup(group_id, type_opt);
    return PersistentMessagesToJava(env, messages);
  }
  if (j_sync_group_id) {
    CHECK(!j_local_group_id);
    std::string sync_group_id_str =
        base::android::ConvertJavaStringToUTF8(env, j_sync_group_id);
    auto group_id = base::Uuid::ParseLowercase(sync_group_id_str);
    auto messages = service_->GetMessagesForGroup(group_id, type_opt);
    return PersistentMessagesToJava(env, messages);
  }

  NOTREACHED();
}

base::android::ScopedJavaLocalRef<jobject>
MessagingBackendServiceBridge::GetMessages(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& j_caller,
    jint j_type) {
  auto type = static_cast<PersistentNotificationType>(j_type);
  std::optional<PersistentNotificationType> type_opt = std::make_optional(type);
  if (type == PersistentNotificationType::UNDEFINED) {
    type_opt = std::nullopt;
  }
  auto messages = service_->GetMessages(type_opt);
  return PersistentMessagesToJava(env, messages);
}

base::android::ScopedJavaLocalRef<jobject>
MessagingBackendServiceBridge::GetActivityLog(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& j_caller,
    jstring j_collaboration_id) {
  ActivityLogQueryParams query_params;
  query_params.collaboration_id = data_sharing::GroupId(
      base::android::ConvertJavaStringToUTF8(env, j_collaboration_id));
  auto activity_log_items = service_->GetActivityLog(query_params);
  return ActivityLogItemsToJava(env, activity_log_items);
}

void MessagingBackendServiceBridge::ClearDirtyTabMessagesForGroup(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& j_caller,
    const base::android::JavaParamRef<jstring>& j_collaboration_id) {
  auto collaboration_id = data_sharing::GroupId(
      base::android::ConvertJavaStringToUTF8(env, j_collaboration_id));
  service_->ClearDirtyTabMessagesForGroup(collaboration_id);
}

void MessagingBackendServiceBridge::ClearPersistentMessage(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& j_caller,
    const base::android::JavaParamRef<jstring>& j_message_id,
    jint j_type) {
  CHECK(j_message_id);
  auto message_id = base::Uuid::ParseLowercase(
      base::android::ConvertJavaStringToUTF8(env, j_message_id));
  auto type = static_cast<PersistentNotificationType>(j_type);
  std::optional<PersistentNotificationType> type_opt = std::make_optional(type);
  if (type == PersistentNotificationType::UNDEFINED) {
    type_opt = std::nullopt;
  }

  service_->ClearPersistentMessage(message_id, type_opt);
}

void MessagingBackendServiceBridge::RunInstantaneousMessageSuccessCallback(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& j_caller,
    jlong j_callback,
    jboolean j_result) {
  CHECK(j_callback);
  std::unique_ptr<
      MessagingBackendService::InstantMessageDelegate::SuccessCallback>
      callback_ptr(
          reinterpret_cast<MessagingBackendService::InstantMessageDelegate::
                               SuccessCallback*>(j_callback));
  std::move(*callback_ptr).Run(j_result);
}

void MessagingBackendServiceBridge::OnMessagingBackendServiceInitialized() {
  if (java_ref_.is_null()) {
    return;
  }

  Java_MessagingBackendServiceBridge_onMessagingBackendServiceInitialized(
      base::android::AttachCurrentThread(), java_ref_);
}

void MessagingBackendServiceBridge::DisplayPersistentMessage(
    PersistentMessage message) {
  if (java_ref_.is_null()) {
    return;
  }
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_MessagingBackendServiceBridge_displayPersistentMessage(
      env, java_ref_, PersistentMessageToJava(env, message));
}
void MessagingBackendServiceBridge::HidePersistentMessage(
    PersistentMessage message) {
  if (java_ref_.is_null()) {
    return;
  }

  JNIEnv* env = base::android::AttachCurrentThread();
  Java_MessagingBackendServiceBridge_hidePersistentMessage(
      env, java_ref_, PersistentMessageToJava(env, message));
}

void MessagingBackendServiceBridge::DisplayInstantaneousMessage(
    InstantMessage message,
    InstantMessageDelegate::SuccessCallback success_callback) {
  if (java_ref_.is_null()) {
    // We definitely failed to display the message.
    std::move(success_callback).Run(false);
    return;
  }

  JNIEnv* env = base::android::AttachCurrentThread();

  // We expect Java to always call us back through
  // MessagingBackendServiceBridge::RunInstantaneousMessageSuccessCallback,
  // Copy |callback| on the heap to pass the pointer through JNI. This callback
  // will be deleted when it's run.
  CHECK(!success_callback.is_null());
  jlong j_native_ptr = reinterpret_cast<jlong>(
      new MessagingBackendService::InstantMessageDelegate::SuccessCallback(
          std::move(success_callback)));

  Java_MessagingBackendServiceBridge_displayInstantaneousMessage(
      env, java_ref_, InstantMessageToJava(env, message), j_native_ptr);
}

void MessagingBackendServiceBridge::HideInstantaneousMessage(
    const std::set<base::Uuid>& message_ids) {
  if (java_ref_.is_null()) {
    return;
  }

  JNIEnv* env = base::android::AttachCurrentThread();
  Java_MessagingBackendServiceBridge_hideInstantaneousMessage(
      env, java_ref_, UuidSetToJavaStringSet(env, message_ids));
}

}  // namespace collaboration::messaging::android
