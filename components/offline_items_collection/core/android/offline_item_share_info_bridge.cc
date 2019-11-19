// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_items_collection/core/android/offline_item_share_info_bridge.h"

#include "base/android/jni_string.h"
#include "components/offline_items_collection/core/jni_headers/OfflineItemShareInfoBridge_jni.h"
#include "components/offline_items_collection/core/offline_item.h"

using base::android::ConvertUTF8ToJavaString;
using base::android::ScopedJavaLocalRef;

namespace offline_items_collection {
namespace android {

// static
ScopedJavaLocalRef<jobject>
OfflineItemShareInfoBridge::CreateOfflineItemShareInfo(
    JNIEnv* env,
    std::unique_ptr<OfflineItemShareInfo> const share_info) {
  if (!share_info)
    return nullptr;

  return Java_OfflineItemShareInfoBridge_createOfflineItemShareInfo(
      env, ConvertUTF8ToJavaString(env, share_info->uri.value()));
}

}  // namespace android
}  // namespace offline_items_collection
