// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAVED_TAB_GROUPS_ANDROID_TAB_GROUP_SYNC_SERVICE_ANDROID_H_
#define COMPONENTS_SAVED_TAB_GROUPS_ANDROID_TAB_GROUP_SYNC_SERVICE_ANDROID_H_

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "base/memory/raw_ptr.h"
#include "base/supports_user_data.h"
#include "components/saved_tab_groups/tab_group_sync_service.h"

using base::android::JavaParamRef;
using base::android::ScopedJavaGlobalRef;
using base::android::ScopedJavaLocalRef;

namespace tab_groups {

// Helper class responsible for bridging the TabGroupSyncService between
// C++ and Java.
class TabGroupSyncServiceAndroid : public base::SupportsUserData::Data,
                                   public TabGroupSyncService::Observer {
 public:
  explicit TabGroupSyncServiceAndroid(TabGroupSyncService* service);
  ~TabGroupSyncServiceAndroid() override;

  ScopedJavaLocalRef<jobject> GetJavaObject();

  // TabGroupSyncService::Observer overrides.
  void OnInitialized() override;
  void OnTabGroupAdded(const SavedTabGroup& group,
                       TriggerSource source) override;
  void OnTabGroupUpdated(const SavedTabGroup& group,
                         TriggerSource source) override;
  void OnTabGroupRemoved(const LocalTabGroupID& local_id,
                         TriggerSource source) override;
  void OnTabGroupRemoved(const base::Uuid& sync_id,
                         TriggerSource source) override;

  // Mutation methods (Java -> native).
  // Mutator methods that result in group metadata mutation.
  ScopedJavaLocalRef<jstring> CreateGroup(
      JNIEnv* env,
      const JavaParamRef<jobject>& j_caller,
      const JavaParamRef<jobject>& j_group_id);

  void RemoveGroupByLocalId(JNIEnv* env,
                            const JavaParamRef<jobject>& j_caller,
                            const JavaParamRef<jobject>& j_local_group_id);

  void RemoveGroupBySyncId(JNIEnv* env,
                           const JavaParamRef<jobject>& j_caller,
                           const JavaParamRef<jstring>& j_sync_group_id);

  void UpdateVisualData(JNIEnv* env,
                        const JavaParamRef<jobject>& j_caller,
                        const JavaParamRef<jobject>& j_group_id,
                        const JavaParamRef<jstring>& j_title,
                        jint j_color);

  // Mutator methods that result in tab metadata mutation.
  void AddTab(JNIEnv* env,
              const JavaParamRef<jobject>& j_caller,
              const JavaParamRef<jobject>& j_group_id,
              jint j_tab_id,
              const JavaParamRef<jstring>& j_title,
              const JavaParamRef<jobject>& j_url,
              jint j_position);

  void UpdateTab(JNIEnv* env,
                 const JavaParamRef<jobject>& j_caller,
                 const JavaParamRef<jobject>& j_group_id,
                 jint j_tab_id,
                 const JavaParamRef<jstring>& j_title,
                 const JavaParamRef<jobject>& j_url,
                 jint j_position);

  void RemoveTab(JNIEnv* env,
                 const JavaParamRef<jobject>& j_caller,
                 const JavaParamRef<jobject>& j_group_id,
                 jint j_tab_id);

  void MoveTab(JNIEnv* env,
               const JavaParamRef<jobject>& j_caller,
               const JavaParamRef<jobject>& j_group_id,
               jint j_tab_id,
               int j_new_index_in_group);

  // Accessor methods.
  ScopedJavaLocalRef<jobjectArray> GetAllGroupIds(
      JNIEnv* env,
      const JavaParamRef<jobject>& j_caller);

  ScopedJavaLocalRef<jobject> GetGroupBySyncGroupId(
      JNIEnv* env,
      const JavaParamRef<jobject>& j_caller,
      const JavaParamRef<jstring>& j_sync_group_id);

  ScopedJavaLocalRef<jobject> GetGroupByLocalGroupId(
      JNIEnv* env,
      const JavaParamRef<jobject>& j_caller,
      const JavaParamRef<jobject>& j_group_id);

  ScopedJavaLocalRef<jobjectArray> GetDeletedGroupIds(
      JNIEnv* env,
      const JavaParamRef<jobject>& j_caller);

  // Book-keeping methods to maintain in-memory mapping of sync and local IDs.
  void UpdateLocalTabGroupMapping(JNIEnv* env,
                                  const JavaParamRef<jobject>& j_caller,
                                  const JavaParamRef<jstring>& j_sync_id,
                                  const JavaParamRef<jobject>& j_local_id);
  void RemoveLocalTabGroupMapping(JNIEnv* env,
                                  const JavaParamRef<jobject>& j_caller,
                                  const JavaParamRef<jobject>& j_local_id);
  void UpdateLocalTabId(JNIEnv* env,
                        const JavaParamRef<jobject>& j_caller,
                        const JavaParamRef<jobject>& j_group_id,
                        const JavaParamRef<jstring>& j_sync_tab_id,
                        jint j_local_tab_id);

 private:
  // A reference to the Java counterpart of this class.  See
  // TabGroupSyncServiceImpl.java.
  ScopedJavaGlobalRef<jobject> java_obj_;

  // Not owned. This is safe because the JNI bridge is destroyed and the
  // native pointer in Java is cleared whenever TabGroupSyncService is
  // destroyed. Hence there will be no subsequent unsafe calls to native.
  raw_ptr<TabGroupSyncService> tab_group_sync_service_;
};

}  // namespace tab_groups

#endif  // COMPONENTS_SAVED_TAB_GROUPS_ANDROID_TAB_GROUP_SYNC_SERVICE_ANDROID_H_
