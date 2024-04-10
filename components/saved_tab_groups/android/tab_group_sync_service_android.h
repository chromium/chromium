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

  void RemoveGroup(JNIEnv* env,
                   const JavaParamRef<jobject>& j_caller,
                   jint j_group_id);

  // TabGroupSyncService::Observer overrides.
  void OnInitialized() override;
  void OnTabGroupAdded(const SavedTabGroup& group,
                       TriggerSource source) override;
  void OnTabGroupUpdated(const SavedTabGroup& group,
                         TriggerSource source) override;
  void OnTabGroupRemoved(const LocalTabGroupID& local_id) override;

 private:
  // A reference to the Java counterpart of this class.  See
  // TabGroupSyncServiceImpl.java.
  ScopedJavaGlobalRef<jobject> java_obj_;

  // Not owned. This is safe because the JNI bridge is destroyed and the native
  // pointer in Java is cleared whenever TabGroupSyncService is destroyed. Hence
  // there will be no subsequent unsafe calls to native.
  raw_ptr<TabGroupSyncService> tab_group_sync_service_;
};

}  // namespace tab_groups

#endif  // COMPONENTS_SAVED_TAB_GROUPS_ANDROID_TAB_GROUP_SYNC_SERVICE_ANDROID_H_
