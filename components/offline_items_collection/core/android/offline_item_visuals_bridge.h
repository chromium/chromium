// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_ITEMS_COLLECTION_CORE_ANDROID_OFFLINE_ITEM_VISUALS_BRIDGE_H_
#define COMPONENTS_OFFLINE_ITEMS_COLLECTION_CORE_ANDROID_OFFLINE_ITEM_VISUALS_BRIDGE_H_


#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"

namespace offline_items_collection {

struct OfflineItemVisuals;

namespace android {

// A helper class for creating Java OfflineItemVisuals instances from the C++
// OfflineItemVisuals counterpart.
class OfflineItemVisualsBridge {
 public:
  // Creates a Java OfflineItemVisuals from |visuals|.
  static base::android::ScopedJavaLocalRef<jobject> CreateOfflineItemVisuals(
      JNIEnv* env,
      std::unique_ptr<OfflineItemVisuals> visuals);

 private:
  OfflineItemVisualsBridge();
};

}  // namespace android
}  // namespace offline_items_collection

#endif  // COMPONENTS_OFFLINE_ITEMS_COLLECTION_CORE_ANDROID_OFFLINE_ITEM_VISUALS_BRIDGE_H_
