// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/saved_tab_groups/android/tab_group_sync_service_android.h"

#include <memory>

#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "components/saved_tab_groups/jni_headers/TabGroupSyncServiceImpl_jni.h"
#include "components/saved_tab_groups/tab_group_sync_service.h"

using base::android::AttachCurrentThread;
using base::android::ConvertJavaStringToUTF16;
using base::android::ConvertJavaStringToUTF8;
using base::android::ConvertUTF8ToJavaString;
using base::android::JavaParamRef;
using base::android::ScopedJavaGlobalRef;
using base::android::ScopedJavaLocalRef;

namespace tab_groups {
namespace {

const char kTabGroupSyncServiceBridgeKey[] = "tab_group_sync_service_bridge";

}  // namespace

// This function is declared in tab_group_sync_service.h and
// should be linked in to any binary using
// TabGroupSyncService::GetJavaObject.
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

void TabGroupSyncServiceAndroid::RemoveGroup(
    JNIEnv* env,
    const JavaParamRef<jobject>& j_caller,
    jint j_group_id) {
  // TODO(b/329124957): Implement.
}

void TabGroupSyncServiceAndroid::OnInitialized() {
  // TODO(b/329124957): Implement.
}

void TabGroupSyncServiceAndroid::OnTabGroupAdded(const SavedTabGroup& group,
                                                 TriggerSource source) {
  // TODO(b/329124957): Implement.
}

void TabGroupSyncServiceAndroid::OnTabGroupUpdated(const SavedTabGroup& group,
                                                   TriggerSource source) {
  // TODO(b/329124957): Implement.
}

void TabGroupSyncServiceAndroid::OnTabGroupRemoved(
    const LocalTabGroupID& local_id) {
  // TODO(b/329124957): Implement.
}

}  // namespace tab_groups
