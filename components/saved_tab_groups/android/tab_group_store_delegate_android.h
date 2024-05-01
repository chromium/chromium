// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAVED_TAB_GROUPS_ANDROID_TAB_GROUP_STORE_DELEGATE_ANDROID_H_
#define COMPONENTS_SAVED_TAB_GROUPS_ANDROID_TAB_GROUP_STORE_DELEGATE_ANDROID_H_

#include <optional>

#include "base/android/jni_android.h"
#include "base/functional/callback_forward.h"
#include "base/uuid.h"
#include "components/saved_tab_groups/tab_group_store_delegate.h"
#include "components/saved_tab_groups/tab_group_store_id.h"

using base::android::ScopedJavaGlobalRef;

namespace tab_groups {

// A Android specific implementation for retrieving and storing sync GUID ->
// TabGroupIDMetadata mappings.
class TabGroupStoreDelegateAndroid : public TabGroupStoreDelegate {
 public:
  TabGroupStoreDelegateAndroid();
  ~TabGroupStoreDelegateAndroid() override;

  // Disallow copy/assign.
  TabGroupStoreDelegateAndroid(const TabGroupStoreDelegateAndroid&) = delete;
  TabGroupStoreDelegateAndroid& operator=(const TabGroupStoreDelegateAndroid&) =
      delete;

  // TabGroupStoreDelegate implementation.
  void GetAllTabGroupIDMetadatas(GetCallback callback) override;
  void StoreTabGroupIDMetadata(
      const base::Uuid& sync_guid,
      const TabGroupIDMetadata& tab_group_id_metadata) override;
  void DeleteTabGroupIDMetdata(const base::Uuid& sync_guid) override;

  // Callback for Java.
  void OnGetTabGroupIDMetadata(
      JNIEnv* env,
      jlong callback_ptr,
      const base::android::JavaParamRef<jobjectArray>& j_sync_guids,
      const base::android::JavaParamRef<jobjectArray>& j_serialized_tokens);

 private:
  // An in-memory map for outstanding callbacks. Used for verifying that a
  // native pointer passed from Java is valid, and to ensure that we delete the
  // callbacks when `this` is deleted.
  std::map<jlong, std::unique_ptr<GetCallback>> callbacks_;

  ScopedJavaGlobalRef<jobject> java_obj_;
};

}  // namespace tab_groups

#endif  // COMPONENTS_SAVED_TAB_GROUPS_ANDROID_TAB_GROUP_STORE_DELEGATE_ANDROID_H_
