// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/saved_tab_groups/android/tab_group_sync_service_android.h"

#include <memory>

#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/memory/scoped_refptr.h"
#include "components/saved_tab_groups/android/tab_group_sync_conversions_bridge.h"
#include "components/saved_tab_groups/android/tab_group_sync_conversions_utils.h"
#include "components/saved_tab_groups/jni_headers/TabGroupSyncServiceImpl_jni.h"
#include "components/saved_tab_groups/tab_group_sync_service.h"
#include "url/android/gurl_android.h"

using base::android::AttachCurrentThread;
using base::android::ConvertJavaStringToUTF16;
using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;

namespace tab_groups {
namespace {

const char kTabGroupSyncServiceBridgeKey[] = "tab_group_sync_service_bridge";

}  // namespace

// This function is declared in tab_group_sync_service.h and
// should be linked in to any binary using TabGroupSyncService::GetJavaObject.
// static
ScopedJavaLocalRef<jobject> TabGroupSyncService::GetJavaObject(
    TabGroupSyncService* service) {
  if (!service->GetUserData(kTabGroupSyncServiceBridgeKey)) {
    service->SetUserData(kTabGroupSyncServiceBridgeKey,
                         std::make_unique<TabGroupSyncServiceAndroid>(service));
  }

  TabGroupSyncServiceAndroid* bridge = static_cast<TabGroupSyncServiceAndroid*>(
      service->GetUserData(kTabGroupSyncServiceBridgeKey));

  return bridge->GetJavaObject();
}

TabGroupSyncServiceAndroid::TabGroupSyncServiceAndroid(
    TabGroupSyncService* tab_group_sync_service)
    : tab_group_sync_service_(tab_group_sync_service) {
  DCHECK(tab_group_sync_service_);
  JNIEnv* env = base::android::AttachCurrentThread();
  java_obj_.Reset(env, Java_TabGroupSyncServiceImpl_create(
                           env, reinterpret_cast<int64_t>(this))
                           .obj());
  tab_group_sync_service_->AddObserver(this);
}

TabGroupSyncServiceAndroid::~TabGroupSyncServiceAndroid() {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_TabGroupSyncServiceImpl_clearNativePtr(env, java_obj_);
  tab_group_sync_service_->RemoveObserver(this);
}

ScopedJavaLocalRef<jobject> TabGroupSyncServiceAndroid::GetJavaObject() {
  return ScopedJavaLocalRef<jobject>(java_obj_);
}

void TabGroupSyncServiceAndroid::OnInitialized() {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_TabGroupSyncServiceImpl_onInitialized(env, java_obj_);
}

void TabGroupSyncServiceAndroid::OnTabGroupAdded(const SavedTabGroup& group,
                                                 TriggerSource source) {
  JNIEnv* env = base::android::AttachCurrentThread();
  auto j_group = TabGroupSyncConversionsBridge::CreateGroup(env, group);
  Java_TabGroupSyncServiceImpl_onTabGroupAdded(env, java_obj_, j_group,
                                               static_cast<jint>(source));
}

void TabGroupSyncServiceAndroid::OnTabGroupUpdated(const SavedTabGroup& group,
                                                   TriggerSource source) {
  JNIEnv* env = base::android::AttachCurrentThread();
  auto j_group = TabGroupSyncConversionsBridge::CreateGroup(env, group);
  Java_TabGroupSyncServiceImpl_onTabGroupUpdated(env, java_obj_, j_group,
                                                 static_cast<jint>(source));
}

void TabGroupSyncServiceAndroid::OnTabGroupRemoved(
    const LocalTabGroupID& local_id,
    TriggerSource source) {
  JNIEnv* env = base::android::AttachCurrentThread();
  auto j_group_id =
      TabGroupSyncConversionsBridge::ToJavaTabGroupId(env, local_id);
  Java_TabGroupSyncServiceImpl_onTabGroupRemovedWithLocalId(
      env, java_obj_, j_group_id, static_cast<jint>(source));
}

void TabGroupSyncServiceAndroid::OnTabGroupRemoved(const base::Uuid& sync_id,
                                                   TriggerSource source) {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_TabGroupSyncServiceImpl_onTabGroupRemovedWithSyncId(
      env, java_obj_, UuidToJavaString(env, sync_id),
      static_cast<jint>(source));
}

ScopedJavaLocalRef<jstring> TabGroupSyncServiceAndroid::CreateGroup(
    JNIEnv* env,
    const JavaParamRef<jobject>& j_caller,
    const JavaParamRef<jobject>& j_group_id) {
  auto group_id =
      TabGroupSyncConversionsBridge::FromJavaTabGroupId(env, j_group_id);

  SavedTabGroup group(std::u16string(), tab_groups::TabGroupColorId::kGrey,
                      std::vector<SavedTabGroupTab>(), std::nullopt,
                      std::nullopt, group_id);
  tab_group_sync_service_->AddGroup(group);
  return UuidToJavaString(env, group.saved_guid());
}

void TabGroupSyncServiceAndroid::RemoveGroupByLocalId(
    JNIEnv* env,
    const JavaParamRef<jobject>& j_caller,
    const JavaParamRef<jobject>& j_local_group_id) {
  auto group_id =
      TabGroupSyncConversionsBridge::FromJavaTabGroupId(env, j_local_group_id);
  tab_group_sync_service_->RemoveGroup(group_id);
}

void TabGroupSyncServiceAndroid::RemoveGroupBySyncId(
    JNIEnv* env,
    const JavaParamRef<jobject>& j_caller,
    const JavaParamRef<jstring>& j_sync_group_id) {
  auto sync_group_id = JavaStringToUuid(env, j_sync_group_id);
  tab_group_sync_service_->RemoveGroup(sync_group_id);
}

void TabGroupSyncServiceAndroid::UpdateVisualData(
    JNIEnv* env,
    const JavaParamRef<jobject>& j_caller,
    const JavaParamRef<jobject>& j_group_id,
    const JavaParamRef<jstring>& j_title,
    jint j_color) {
  auto group_id =
      TabGroupSyncConversionsBridge::FromJavaTabGroupId(env, j_group_id);
  auto title = ConvertJavaStringToUTF16(env, j_title);
  auto color = static_cast<tab_groups::TabGroupColorId>(j_color);
  TabGroupVisualData visual_data(title, color, /*is_collapsed=*/false);
  tab_group_sync_service_->UpdateVisualData(group_id, &visual_data);
}

void TabGroupSyncServiceAndroid::AddTab(JNIEnv* env,
                                        const JavaParamRef<jobject>& j_caller,
                                        const JavaParamRef<jobject>& j_group_id,
                                        jint j_tab_id,
                                        const JavaParamRef<jstring>& j_title,
                                        const JavaParamRef<jobject>& j_url,
                                        jint j_position) {
  auto group_id =
      TabGroupSyncConversionsBridge::FromJavaTabGroupId(env, j_group_id);
  auto tab_id = FromJavaTabId(j_tab_id);
  auto title = ConvertJavaStringToUTF16(env, j_title);
  GURL url = url::GURLAndroid::ToNativeGURL(env, j_url);
  std::optional<size_t> position =
      j_position < 0 ? std::nullopt : std::make_optional<size_t>(j_position);
  tab_group_sync_service_->AddTab(group_id, tab_id, title, url, position);
}

void TabGroupSyncServiceAndroid::UpdateTab(
    JNIEnv* env,
    const JavaParamRef<jobject>& j_caller,
    const JavaParamRef<jobject>& j_group_id,
    jint j_tab_id,
    const JavaParamRef<jstring>& j_title,
    const JavaParamRef<jobject>& j_url,
    jint j_position) {
  auto group_id =
      TabGroupSyncConversionsBridge::FromJavaTabGroupId(env, j_group_id);
  auto tab_id = FromJavaTabId(j_tab_id);
  auto title = ConvertJavaStringToUTF16(env, j_title);
  GURL url = url::GURLAndroid::ToNativeGURL(env, j_url);
  std::optional<size_t> position =
      j_position < 0 ? std::nullopt : std::make_optional<size_t>(j_position);
  tab_group_sync_service_->UpdateTab(group_id, tab_id, title, url, position);
}

void TabGroupSyncServiceAndroid::RemoveTab(
    JNIEnv* env,
    const JavaParamRef<jobject>& j_caller,
    const JavaParamRef<jobject>& j_group_id,
    jint j_tab_id) {
  auto group_id =
      TabGroupSyncConversionsBridge::FromJavaTabGroupId(env, j_group_id);
  auto tab_id = FromJavaTabId(j_tab_id);
  tab_group_sync_service_->RemoveTab(group_id, tab_id);
}

void TabGroupSyncServiceAndroid::MoveTab(
    JNIEnv* env,
    const JavaParamRef<jobject>& j_caller,
    const JavaParamRef<jobject>& j_group_id,
    jint j_tab_id,
    int j_new_index_in_group) {
  auto group_id =
      TabGroupSyncConversionsBridge::FromJavaTabGroupId(env, j_group_id);
  auto tab_id = FromJavaTabId(j_tab_id);
  tab_group_sync_service_->MoveTab(group_id, tab_id, j_new_index_in_group);
}

ScopedJavaLocalRef<jobjectArray> TabGroupSyncServiceAndroid::GetAllGroupIds(
    JNIEnv* env,
    const JavaParamRef<jobject>& j_caller) {
  std::vector<std::string> sync_ids;
  const auto& groups = tab_group_sync_service_->GetAllGroups();
  for (const auto& group : groups) {
    sync_ids.emplace_back(group.saved_guid().AsLowercaseString());
  }

  return base::android::ToJavaArrayOfStrings(env, sync_ids);
}

ScopedJavaLocalRef<jobject> TabGroupSyncServiceAndroid::GetGroupBySyncGroupId(
    JNIEnv* env,
    const JavaParamRef<jobject>& j_caller,
    const JavaParamRef<jstring>& j_sync_group_id) {
  auto sync_group_id = JavaStringToUuid(env, j_sync_group_id);

  std::optional<SavedTabGroup> group =
      tab_group_sync_service_->GetGroup(sync_group_id);
  if (!group.has_value()) {
    return ScopedJavaLocalRef<jobject>();
  }

  return TabGroupSyncConversionsBridge::CreateGroup(env, group.value());
}

ScopedJavaLocalRef<jobject> TabGroupSyncServiceAndroid::GetGroupByLocalGroupId(
    JNIEnv* env,
    const JavaParamRef<jobject>& j_caller,
    const JavaParamRef<jobject>& j_local_group_id) {
  auto local_group_id =
      TabGroupSyncConversionsBridge::FromJavaTabGroupId(env, j_local_group_id);
  std::optional<SavedTabGroup> group =
      tab_group_sync_service_->GetGroup(local_group_id);
  if (!group.has_value()) {
    return ScopedJavaLocalRef<jobject>();
  }
  return TabGroupSyncConversionsBridge::CreateGroup(env, group.value());
}

ScopedJavaLocalRef<jobjectArray> TabGroupSyncServiceAndroid::GetDeletedGroupIds(
    JNIEnv* env,
    const JavaParamRef<jobject>& j_caller) {
  auto group_ids = tab_group_sync_service_->GetDeletedGroupIds();
  std::vector<ScopedJavaLocalRef<jobject>> j_group_ids;
  for (const auto& group_id : group_ids) {
    auto j_group_id =
        TabGroupSyncConversionsBridge::ToJavaTabGroupId(env, group_id);
    j_group_ids.emplace_back(j_group_id);
  }
  return base::android::ToJavaArrayOfObjects(env, j_group_ids);
}

void TabGroupSyncServiceAndroid::UpdateLocalTabGroupMapping(
    JNIEnv* env,
    const JavaParamRef<jobject>& j_caller,
    const JavaParamRef<jstring>& j_sync_id,
    const JavaParamRef<jobject>& j_local_id) {
  auto sync_id = JavaStringToUuid(env, j_sync_id);
  auto local_id =
      TabGroupSyncConversionsBridge::FromJavaTabGroupId(env, j_local_id);
  tab_group_sync_service_->UpdateLocalTabGroupMapping(sync_id, local_id);
}

void TabGroupSyncServiceAndroid::RemoveLocalTabGroupMapping(
    JNIEnv* env,
    const JavaParamRef<jobject>& j_caller,
    const JavaParamRef<jobject>& j_local_id) {
  auto local_id =
      TabGroupSyncConversionsBridge::FromJavaTabGroupId(env, j_local_id);
  tab_group_sync_service_->RemoveLocalTabGroupMapping(local_id);
}

void TabGroupSyncServiceAndroid::UpdateLocalTabId(
    JNIEnv* env,
    const JavaParamRef<jobject>& j_caller,
    const JavaParamRef<jobject>& j_group_id,
    const JavaParamRef<jstring>& j_sync_tab_id,
    jint j_local_tab_id) {
  auto local_group_id =
      TabGroupSyncConversionsBridge::FromJavaTabGroupId(env, j_group_id);
  auto sync_tab_id = JavaStringToUuid(env, j_sync_tab_id);
  auto local_tab_id = FromJavaTabId(j_local_tab_id);
  tab_group_sync_service_->UpdateLocalTabId(local_group_id, sync_tab_id,
                                            local_tab_id);
}

}  // namespace tab_groups
