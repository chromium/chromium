// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/saved_tab_groups/messaging/android/conversion_utils.h"

#include <optional>

#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/uuid.h"
#include "components/data_sharing/public/android/conversion_utils.h"
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

// Helper method to provide a consistent way to create a PersistentMessage
// across multiple entry points.
// TODO(dtrainor): Probably worth constructing a MessageAttribution separately
// and isolating that (common) code.
ScopedJavaLocalRef<jobject> CreatePersistentMessageAndMaybeAddToListHelper(
    JNIEnv* env,
    ScopedJavaLocalRef<jobject> jlist,
    PersistentMessage message) {
  ScopedJavaLocalRef<jobject> jaffected_user = nullptr;
  if (message.attribution.affected_user.has_value()) {
    jaffected_user = data_sharing::conversion::CreateJavaGroupMember(
        env, message.attribution.affected_user.value());
  }

  ScopedJavaLocalRef<jobject> jtriggering_user = nullptr;
  if (message.attribution.triggering_user.has_value()) {
    jtriggering_user = data_sharing::conversion::CreateJavaGroupMember(
        env, message.attribution.triggering_user.value());
  }

  ScopedJavaLocalRef<jobject> jmessage =
      Java_ConversionUtils_createPersistentMessageAndMaybeAddToList(
          env, jlist,
          ConvertUTF8ToJavaString(env,
                                  message.attribution.collaboration_id.value()),
          TabGroupSyncConversionsBridge::ToJavaTabGroupId(
              env, message.attribution.local_tab_group_id),
          OptionalUuidToLowercaseJavaString(
              env, message.attribution.sync_tab_group_id),
          ToJavaTabId(message.attribution.local_tab_id),
          OptionalUuidToLowercaseJavaString(env,
                                            message.attribution.sync_tab_id),
          jaffected_user, jtriggering_user, static_cast<int>(message.action),
          static_cast<int>(message.type));

  return jmessage;
}
}  // namespace

ScopedJavaLocalRef<jobject> PersistentMessageToJava(JNIEnv* env,
                                                    PersistentMessage message) {
  return CreatePersistentMessageAndMaybeAddToListHelper(env, nullptr, message);
}

ScopedJavaLocalRef<jobject> PersistentMessagesToJava(
    JNIEnv* env,
    std::vector<PersistentMessage> messages) {
  ScopedJavaLocalRef<jobject> jlist =
      Java_ConversionUtils_createPersistentMessageList(env);

  for (const auto& message : messages) {
    CreatePersistentMessageAndMaybeAddToListHelper(env, jlist, message);
  }

  return jlist;
}

ScopedJavaLocalRef<jobject> InstantMessageToJava(JNIEnv* env,
                                                 InstantMessage message) {
  ScopedJavaLocalRef<jobject> jaffected_user = nullptr;
  if (message.attribution.affected_user.has_value()) {
    jaffected_user = data_sharing::conversion::CreateJavaGroupMember(
        env, message.attribution.affected_user.value());
  }

  ScopedJavaLocalRef<jobject> jtriggering_user = nullptr;
  if (message.attribution.triggering_user.has_value()) {
    jtriggering_user = data_sharing::conversion::CreateJavaGroupMember(
        env, message.attribution.triggering_user.value());
  }

  return Java_ConversionUtils_createInstantMessage(
      env,
      ConvertUTF8ToJavaString(env,
                              message.attribution.collaboration_id.value()),
      TabGroupSyncConversionsBridge::ToJavaTabGroupId(
          env, message.attribution.local_tab_group_id),
      OptionalUuidToLowercaseJavaString(env,
                                        message.attribution.sync_tab_group_id),
      ToJavaTabId(message.attribution.local_tab_id),
      OptionalUuidToLowercaseJavaString(env, message.attribution.sync_tab_id),
      jaffected_user, jtriggering_user, static_cast<int>(message.action),
      static_cast<int>(message.level), static_cast<int>(message.type));
}

}  // namespace tab_groups::messaging::android
