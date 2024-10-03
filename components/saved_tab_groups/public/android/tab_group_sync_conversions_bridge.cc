// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/saved_tab_groups/public/android/tab_group_sync_conversions_bridge.h"

#include <optional>
#include <vector>

#include "base/android/jni_string.h"
#include "base/android/token_android.h"
#include "base/uuid.h"
#include "components/saved_tab_groups/public/android/tab_group_sync_conversions_utils.h"
#include "components/saved_tab_groups/public/saved_tab_group.h"
#include "components/saved_tab_groups/public/saved_tab_group_tab.h"
#include "url/android/gurl_android.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "components/saved_tab_groups/public/conversion_utils_jni_headers/TabGroupSyncConversionsBridge_jni.h"

using base::android::ConvertUTF16ToJavaString;
using base::android::ConvertUTF8ToJavaString;
using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;
using base::android::TokenAndroid;

namespace tab_groups {
namespace {

// Unspecified tab position is represented as -1 in Java.
int kInvalidTabPosition = -1;

// Converts collaboration group ID to Java. If the collaboration group ID is
// not present, null is returned.
ScopedJavaLocalRef<jstring> ToJavaCollaborationId(
    JNIEnv* env,
    const std::optional<std::string>& collaboration_id) {
  return collaboration_id.has_value()
             ? ConvertUTF8ToJavaString(env, collaboration_id.value())
             : ScopedJavaLocalRef<jstring>();
}

// Helper method to create a Java SavedTabGroupTab, and optionally add it to
// a group if a non-null |j_tab_group| is specified.
ScopedJavaLocalRef<jobject>
JNI_TabGroupSyncConversionsBridge_createTabAndMaybeAddToGroup(
    JNIEnv* env,
    const SavedTabGroupTab& tab,
    ScopedJavaLocalRef<jobject>& j_tab_group) {
  auto j_sync_tab_id = UuidToJavaString(env, tab.saved_tab_guid());
  auto j_local_tab_id = ToJavaTabId(tab.local_tab_id());
  auto j_sync_group_id = UuidToJavaString(env, tab.saved_group_guid());
  auto j_url = url::GURLAndroid::FromNativeGURL(env, tab.url());
  auto j_creator_cache_guid = ConvertUTF8ToJavaString(
      env, tab.creator_cache_guid().value_or(std::string()));
  auto j_last_updater_cache_guid = ConvertUTF8ToJavaString(
      env, tab.last_updater_cache_guid().value_or(std::string()));

  return Java_TabGroupSyncConversionsBridge_createTabAndMaybeAddToGroup(
      env, j_sync_tab_id, j_local_tab_id, j_sync_group_id,
      static_cast<int32_t>(tab.position().value_or(kInvalidTabPosition)), j_url,
      ConvertUTF16ToJavaString(env, tab.title()),
      tab.creation_time_windows_epoch_micros().InMillisecondsSinceUnixEpoch(),
      tab.update_time_windows_epoch_micros().InMillisecondsSinceUnixEpoch(),
      j_creator_cache_guid, j_last_updater_cache_guid, j_tab_group);
}

// Helper method to create a Java SavedTabGroup. This doesn't include the tabs.
ScopedJavaLocalRef<jobject> JNI_TabGroupSyncConversionsBridge_createGroup(
    JNIEnv* env,
    const SavedTabGroup& group) {
  auto j_sync_id = UuidToJavaString(env, group.saved_guid());
  auto j_local_id = TabGroupSyncConversionsBridge::ToJavaTabGroupId(
      env, group.local_group_id());
  auto j_creator_cache_guid = ConvertUTF8ToJavaString(
      env, group.creator_cache_guid().value_or(std::string()));
  auto j_last_updater_cache_guid = ConvertUTF8ToJavaString(
      env, group.last_updater_cache_guid().value_or(std::string()));
  auto j_collaboration_id =
      ToJavaCollaborationId(env, group.collaboration_id());

  return Java_TabGroupSyncConversionsBridge_createGroup(
      env, j_sync_id, j_local_id, ConvertUTF16ToJavaString(env, group.title()),
      static_cast<int32_t>(group.color()),
      group.creation_time_windows_epoch_micros().InMillisecondsSinceUnixEpoch(),
      group.update_time_windows_epoch_micros().InMillisecondsSinceUnixEpoch(),
      j_creator_cache_guid, j_last_updater_cache_guid, j_collaboration_id);
}

}  // namespace

// static
ScopedJavaLocalRef<jobject> TabGroupSyncConversionsBridge::CreateGroup(
    JNIEnv* env,
    const SavedTabGroup& group) {
  auto j_tab_group = JNI_TabGroupSyncConversionsBridge_createGroup(env, group);
  for (const auto& tab : group.saved_tabs()) {
    JNI_TabGroupSyncConversionsBridge_createTabAndMaybeAddToGroup(env, tab,
                                                                  j_tab_group);
  }

  return j_tab_group;
}

// static
LocalTabGroupID TabGroupSyncConversionsBridge::FromJavaTabGroupId(
    JNIEnv* env,
    const JavaParamRef<jobject>& j_group_id) {
  auto j_token =
      Java_TabGroupSyncConversionsBridge_getNativeTabGroupId(env, j_group_id);
  return TokenAndroid::FromJavaToken(env, j_token);
}

// static
ScopedJavaLocalRef<jobject> TabGroupSyncConversionsBridge::ToJavaTabGroupId(
    JNIEnv* env,
    const std::optional<LocalTabGroupID>& group_id) {
  return group_id.has_value()
             ? Java_TabGroupSyncConversionsBridge_createJavaTabGroupId(
                   env, TokenAndroid::Create(env, group_id.value()))
             : ScopedJavaLocalRef<jobject>();
}

}  // namespace tab_groups
