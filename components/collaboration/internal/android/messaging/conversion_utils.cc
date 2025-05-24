// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/collaboration/internal/android/messaging/conversion_utils.h"

#include <optional>

#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/uuid.h"
#include "components/collaboration/public/messaging/activity_log.h"
#include "components/collaboration/public/messaging/message.h"
#include "components/data_sharing/public/android/conversion_utils.h"
#include "components/saved_tab_groups/public/android/tab_group_sync_conversions_bridge.h"
#include "components/saved_tab_groups/public/android/tab_group_sync_conversions_utils.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "components/collaboration/internal/messaging_jni_headers/ConversionUtils_jni.h"

using base::android::ConvertUTF16ToJavaString;
using base::android::ConvertUTF8ToJavaString;
using base::android::ScopedJavaLocalRef;

namespace collaboration::messaging::android {
namespace {
ScopedJavaLocalRef<jstring> JavaStringOrNullFromOptionalString(
    JNIEnv* env,
    std::optional<std::string> str) {
  if (!str.has_value()) {
    return ScopedJavaLocalRef<jstring>();
  }
  return ConvertUTF8ToJavaString(env, (*str));
}

ScopedJavaLocalRef<jstring> OptionalUuidToLowercaseJavaString(
    JNIEnv* env,
    std::optional<base::Uuid> uuid) {
  if (!uuid.has_value()) {
    return ScopedJavaLocalRef<jstring>();
  }
  return ConvertUTF8ToJavaString(env, (*uuid).AsLowercaseString());
}

// Helper method to provide a consistent way to create a MessageAttribution
// across multiple entry points.
ScopedJavaLocalRef<jobject> MessageAttributionToJava(
    JNIEnv* env,
    const MessageAttribution& attribution) {
  ScopedJavaLocalRef<jstring> j_collaboration_id =
      ConvertUTF8ToJavaString(env, attribution.collaboration_id.value());
  ScopedJavaLocalRef<jobject> j_local_tab_group_id = nullptr;
  ScopedJavaLocalRef<jstring> j_sync_tab_group_id = nullptr;
  ScopedJavaLocalRef<jstring> j_last_known_tab_group_title = nullptr;

  ScopedJavaLocalRef<jstring> j_id =
      OptionalUuidToLowercaseJavaString(env, attribution.id);

  jint j_last_known_tab_group_color = -1;
  if (attribution.tab_group_metadata.has_value()) {
    j_local_tab_group_id =
        tab_groups::TabGroupSyncConversionsBridge::ToJavaTabGroupId(
            env, attribution.tab_group_metadata->local_tab_group_id);
    j_sync_tab_group_id = OptionalUuidToLowercaseJavaString(
        env, attribution.tab_group_metadata->sync_tab_group_id);
    j_last_known_tab_group_title = JavaStringOrNullFromOptionalString(
        env, attribution.tab_group_metadata->last_known_title);
    if (attribution.tab_group_metadata->last_known_color.has_value()) {
      j_last_known_tab_group_color =
          static_cast<jint>(*attribution.tab_group_metadata->last_known_color);
    }
  }

  jint j_local_tab_id = -1;
  ScopedJavaLocalRef<jstring> j_sync_tab_id = nullptr;
  ScopedJavaLocalRef<jstring> j_last_known_tab_url = nullptr;
  ScopedJavaLocalRef<jstring> j_last_known_tab_title = nullptr;
  ScopedJavaLocalRef<jstring> j_previous_tab_url = nullptr;
  if (attribution.tab_metadata.has_value()) {
    j_local_tab_id =
        tab_groups::ToJavaTabId((*attribution.tab_metadata).local_tab_id);
    j_sync_tab_id = OptionalUuidToLowercaseJavaString(
        env, attribution.tab_metadata->sync_tab_id);

    j_last_known_tab_url = JavaStringOrNullFromOptionalString(
        env, attribution.tab_metadata->last_known_url);
    j_last_known_tab_title = JavaStringOrNullFromOptionalString(
        env, attribution.tab_metadata->last_known_title);

    j_previous_tab_url = JavaStringOrNullFromOptionalString(
        env, attribution.tab_metadata->previous_url);
  }

  ScopedJavaLocalRef<jobject> j_affected_user = nullptr;
  if (attribution.affected_user.has_value()) {
    j_affected_user = data_sharing::conversion::CreateJavaGroupMember(
        env, attribution.affected_user.value());
  }

  ScopedJavaLocalRef<jobject> j_triggering_user = nullptr;
  if (attribution.triggering_user.has_value()) {
    j_triggering_user = data_sharing::conversion::CreateJavaGroupMember(
        env, attribution.triggering_user.value());
  }

  return Java_ConversionUtils_createAttributionFrom(
      env, j_id, j_collaboration_id, j_local_tab_group_id, j_sync_tab_group_id,
      j_last_known_tab_group_title, j_last_known_tab_group_color,
      j_local_tab_id, j_sync_tab_id, j_last_known_tab_title,
      j_last_known_tab_url, j_previous_tab_url, j_affected_user,
      attribution.affected_user_is_self, j_triggering_user,
      attribution.triggering_user_is_self);
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
          static_cast<int>(message.collaboration_event),
          static_cast<int>(message.type));

  return jmessage;
}

ScopedJavaLocalRef<jobject> AttributionListToJava(
    JNIEnv* env,
    const std::vector<MessageAttribution>& attributions) {
  ScopedJavaLocalRef<jobject> j_attribution_list;

  for (const auto& attribution : attributions) {
    auto j_attribution = MessageAttributionToJava(env, attribution);
    j_attribution_list = Java_ConversionUtils_addAttributionToList(
        env, j_attribution_list, j_attribution);
  }

  return j_attribution_list;
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
  ScopedJavaLocalRef<jobject> j_attribution_list =
      AttributionListToJava(env, message.attributions);
  return Java_ConversionUtils_createInstantMessage(
      env, static_cast<int>(message.collaboration_event),
      static_cast<int>(message.level), static_cast<int>(message.type),
      ConvertUTF16ToJavaString(env, message.localized_message),
      j_attribution_list);
}

ScopedJavaLocalRef<jobject> UuidSetToJavaStringSet(
    JNIEnv* env,
    const std::set<base::Uuid>& uuids) {
  ScopedJavaLocalRef<jobject> j_string_set =
      Java_ConversionUtils_createStringSet(env);

  for (const auto& uuid : uuids) {
    Java_ConversionUtils_addStringToStringSet(
        env, j_string_set,
        ConvertUTF8ToJavaString(env, uuid.AsLowercaseString()));
  }
  return j_string_set;
}

ScopedJavaLocalRef<jobject> ActivityLogItemsToJava(
    JNIEnv* env,
    const std::vector<ActivityLogItem>& activity_log_items) {
  ScopedJavaLocalRef<jobject> jlist =
      Java_ConversionUtils_createActivityLogItemList(env);

  for (const auto& activity_log_item : activity_log_items) {
    Java_ConversionUtils_createActivityLogItemAndMaybeAddToList(
        env, jlist, static_cast<int>(activity_log_item.collaboration_event),
        ConvertUTF16ToJavaString(env, activity_log_item.title_text),
        ConvertUTF16ToJavaString(env, activity_log_item.description_text),
        ConvertUTF16ToJavaString(env, activity_log_item.time_delta_text),
        activity_log_item.show_favicon,
        static_cast<int>(activity_log_item.action),
        MessageAttributionToJava(env, activity_log_item.activity_metadata));
  }

  return jlist;
}

}  // namespace collaboration::messaging::android
