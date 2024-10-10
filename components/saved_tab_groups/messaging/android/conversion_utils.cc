// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/saved_tab_groups/messaging/android/conversion_utils.h"

#include <optional>

#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/uuid.h"
#include "components/data_sharing/public/android/conversion_utils.h"
#include "components/saved_tab_groups/messaging/activity_log.h"
#include "components/saved_tab_groups/messaging/message.h"
#include "components/saved_tab_groups/public/android/tab_group_sync_conversions_bridge.h"
#include "components/saved_tab_groups/public/android/tab_group_sync_conversions_utils.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "components/saved_tab_groups/messaging/android/jni_headers/ConversionUtils_jni.h"

using base::android::ConvertUTF8ToJavaString;
using base::android::ScopedJavaLocalRef;

namespace tab_groups::messaging::android {
namespace {

ScopedJavaLocalRef<jstring> OptionalUuidToLowercaseJavaString(
    JNIEnv* env,
    std::optional<base::Uuid> uuid) {
  if (!uuid.has_value()) {
    return ScopedJavaLocalRef<jstring>();
  }
  return ConvertUTF8ToJavaString(env, (*uuid).AsLowercaseString());
}

ScopedJavaLocalRef<jobject> MessageAttributionToJava(
    JNIEnv* env,
    const MessageAttribution& attribution) {
  ScopedJavaLocalRef<jobject> jaffected_user = nullptr;
  if (attribution.affected_user.has_value()) {
    jaffected_user = data_sharing::conversion::CreateJavaGroupMember(
        env, attribution.affected_user.value());
  }

  ScopedJavaLocalRef<jobject> jtriggering_user = nullptr;
  if (attribution.triggering_user.has_value()) {
    jtriggering_user = data_sharing::conversion::CreateJavaGroupMember(
        env, attribution.triggering_user.value());
  }

  return Java_ConversionUtils_createAttributionFrom(
      env, ConvertUTF8ToJavaString(env, attribution.collaboration_id.value()),
      TabGroupSyncConversionsBridge::ToJavaTabGroupId(
          env, attribution.local_tab_group_id),
      OptionalUuidToLowercaseJavaString(env, attribution.sync_tab_group_id),
      ToJavaTabId(attribution.local_tab_id),
      OptionalUuidToLowercaseJavaString(env, attribution.sync_tab_id),
      jaffected_user, jtriggering_user);
}

// Helper method to provide a consistent way to create a PersistentMessage
// across multiple entry points.
ScopedJavaLocalRef<jobject> CreatePersistentMessageAndMaybeAddToListHelper(
    JNIEnv* env,
    ScopedJavaLocalRef<jobject> jlist,
    const PersistentMessage& message) {
  ScopedJavaLocalRef<jobject> jmessage =
      Java_ConversionUtils_createPersistentMessageAndMaybeAddToList(
          env, jlist, MessageAttributionToJava(env, message.attribution),
          static_cast<int>(message.action), static_cast<int>(message.type));

  return jmessage;
}
}  // namespace

ScopedJavaLocalRef<jobject> PersistentMessageToJava(
    JNIEnv* env,
    const PersistentMessage& message) {
  return CreatePersistentMessageAndMaybeAddToListHelper(env, nullptr, message);
}

ScopedJavaLocalRef<jobject> PersistentMessagesToJava(
    JNIEnv* env,
    const std::vector<PersistentMessage>& messages) {
  ScopedJavaLocalRef<jobject> jlist =
      Java_ConversionUtils_createPersistentMessageList(env);

  for (const auto& message : messages) {
    CreatePersistentMessageAndMaybeAddToListHelper(env, jlist, message);
  }

  return jlist;
}

ScopedJavaLocalRef<jobject> InstantMessageToJava(
    JNIEnv* env,
    const InstantMessage& message) {
  return Java_ConversionUtils_createInstantMessage(
      env, MessageAttributionToJava(env, message.attribution),
      static_cast<int>(message.action), static_cast<int>(message.level),
      static_cast<int>(message.type));
}

ScopedJavaLocalRef<jobject> ActivityLogItemsToJava(
    JNIEnv* env,
    const std::vector<ActivityLogItem>& activity_log_items) {
  ScopedJavaLocalRef<jobject> jlist =
      Java_ConversionUtils_createActivityLogItemList(env);

  for (const auto& activity_log_item : activity_log_items) {
    Java_ConversionUtils_createActivityLogItemAndMaybeAddToList(
        env, jlist, static_cast<int>(activity_log_item.user_action_type),
        ConvertUTF8ToJavaString(env, activity_log_item.title_text),
        ConvertUTF8ToJavaString(env, activity_log_item.description_text),
        ConvertUTF8ToJavaString(env, activity_log_item.timestamp_text),
        MessageAttributionToJava(env, activity_log_item.activity_metadata));
  }

  return jlist;
}

}  // namespace tab_groups::messaging::android
