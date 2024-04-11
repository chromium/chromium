// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAVED_TAB_GROUPS_ANDROID_TAB_GROUP_SYNC_CONVERSIONS_BRIDGE_H_
#define COMPONENTS_SAVED_TAB_GROUPS_ANDROID_TAB_GROUP_SYNC_CONVERSIONS_BRIDGE_H_

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "components/saved_tab_groups/saved_tab_group.h"

using base::android::ScopedJavaLocalRef;

namespace tab_groups {

// A helper class for creating Java SavedTabGroup instances from its native
// counterpart.
class TabGroupSyncConversionsBridge {
 public:
  // Creates a Java SavedTabGroup from the given |group|.
  static base::android::ScopedJavaLocalRef<jobject> CreateGroup(
      JNIEnv* env,
      const SavedTabGroup& group);

 private:
  TabGroupSyncConversionsBridge() = default;
};

}  // namespace tab_groups

#endif  // COMPONENTS_SAVED_TAB_GROUPS_ANDROID_TAB_GROUP_SYNC_CONVERSIONS_BRIDGE_H_
