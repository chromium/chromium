// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_items_collection/core/android/offline_item_bridge.h"

#include "base/android/jni_string.h"
#include "components/offline_items_collection/core/jni_headers/OfflineItemBridge_jni.h"

using base::android::ConvertUTF8ToJavaString;
using base::android::ScopedJavaLocalRef;

namespace offline_items_collection {
namespace android {

namespace {

// Helper method to unify the OfflineItem conversion argument list to a single
// place.  This is meant to reduce code churn from OfflineItem member
// modification.  The behavior is as follows:
// - The method always returns the new Java OfflineItem instance.
// - If |jlist| is specified (an ArrayList<OfflineItem>), the item is added to
//   that list.  |jlist| can also be null, in which case the item isn't added to
//   anything.
ScopedJavaLocalRef<jobject>
JNI_OfflineItemBridge_createOfflineItemAndMaybeAddToList(
    JNIEnv* env,
    ScopedJavaLocalRef<jobject> jlist,
    const OfflineItem& item) {
  return Java_OfflineItemBridge_createOfflineItemAndMaybeAddToList(
      env, jlist, ConvertUTF8ToJavaString(env, item.id.name_space),
      ConvertUTF8ToJavaString(env, item.id.id),
      ConvertUTF8ToJavaString(env, item.title),
      ConvertUTF8ToJavaString(env, item.description),
      static_cast<jint>(item.filter), item.is_transient, item.is_suggested,
      item.is_accelerated, item.promote_origin, item.total_size_bytes,
      item.externally_removed, item.creation_time.ToJavaTime(),
      item.completion_time.ToJavaTime(), item.last_accessed_time.ToJavaTime(),
      item.is_openable, ConvertUTF8ToJavaString(env, item.file_path.value()),
      ConvertUTF8ToJavaString(env, item.mime_type),
      ConvertUTF8ToJavaString(env, item.page_url.spec()),
      ConvertUTF8ToJavaString(env, item.original_url.spec()),
      item.is_off_the_record, static_cast<jint>(item.state),
      static_cast<jint>(item.fail_state), static_cast<jint>(item.pending_state),
      item.is_resumable, item.allow_metered, item.received_bytes,
      item.progress.value, item.progress.max.value_or(-1),
      static_cast<jint>(item.progress.unit), item.time_remaining_ms,
      item.is_dangerous, item.can_rename, item.ignore_visuals,
      item.content_quality_score);
}

}  // namespace

// static
ScopedJavaLocalRef<jobject> OfflineItemBridge::CreateOfflineItem(
    JNIEnv* env,
    const OfflineItem& item) {
  return JNI_OfflineItemBridge_createOfflineItemAndMaybeAddToList(env, nullptr,
                                                                  item);
}

// static
ScopedJavaLocalRef<jobject> OfflineItemBridge::CreateOfflineItemList(
    JNIEnv* env,
    const std::vector<OfflineItem>& items) {
  ScopedJavaLocalRef<jobject> jlist =
      Java_OfflineItemBridge_createArrayList(env);
  for (const auto& item : items)
    JNI_OfflineItemBridge_createOfflineItemAndMaybeAddToList(env, jlist, item);
  return jlist;
}

// static
ScopedJavaLocalRef<jobject> OfflineItemBridge::CreateUpdateDelta(
    JNIEnv* env,
    const base::Optional<UpdateDelta>& update_delta) {
  if (!update_delta.has_value())
    return ScopedJavaLocalRef<jobject>();

  return Java_OfflineItemBridge_createUpdateDelta(
      env, update_delta.value().state_changed,
      update_delta.value().visuals_changed);
}

OfflineItemBridge::OfflineItemBridge() = default;

}  // namespace android
}  // namespace offline_items_collection
