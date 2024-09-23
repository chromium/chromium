// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_items_collection/core/android/offline_item_visuals_bridge.h"

#include "components/offline_items_collection/core/offline_item.h"
#include "ui/gfx/android/java_bitmap.h"
#include "ui/gfx/image/image.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "components/offline_items_collection/core/jni_headers/OfflineItemVisualsBridge_jni.h"

using base::android::ScopedJavaLocalRef;

namespace offline_items_collection {
namespace android {

// static
ScopedJavaLocalRef<jobject> OfflineItemVisualsBridge::CreateOfflineItemVisuals(
    JNIEnv* env,
    std::unique_ptr<OfflineItemVisuals> const visuals) {
  if (!visuals)
    return nullptr;

  base::android::ScopedJavaLocalRef<jobject> j_icon;

  if (!visuals->icon.IsEmpty())
    j_icon = gfx::ConvertToJavaBitmap(*visuals->icon.ToSkBitmap());

  return Java_OfflineItemVisualsBridge_createOfflineItemVisuals(env, j_icon);
}

}  // namespace android
}  // namespace offline_items_collection
