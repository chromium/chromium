// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_items_collection/core/android/offline_item_bridge.h"

#include "base/android/jni_android.h"
#include "testing/gtest/include/gtest/gtest.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "components/offline_items_collection/core/native_j_unittests_jni_headers/OfflineItemBridgeUnitTest_jni.h"

using base::android::AttachCurrentThread;

namespace offline_items_collection {
namespace android {
namespace {

// Tests the JNI bridge that creates Java OfflineItem.
class OfflineItemBridgeTest : public ::testing::Test {
 public:
  OfflineItemBridgeTest()
      : j_test_(
            Java_OfflineItemBridgeUnitTest_Constructor(AttachCurrentThread())) {
  }

  const base::android::ScopedJavaGlobalRef<jobject>& j_test() {
    return j_test_;
  }

 private:
  base::android::ScopedJavaGlobalRef<jobject> j_test_;
};

// Verfies a default offline item can be created in Java.
TEST_F(OfflineItemBridgeTest, CreateOfflineItem) {
  OfflineItem item;
  auto* env = AttachCurrentThread();
  auto j_offline_item = OfflineItemBridge::CreateOfflineItem(env, item);
  Java_OfflineItemBridgeUnitTest_testCreateDefaultOfflineItem(env, j_test(),
                                                              j_offline_item);
}

}  // namespace
}  // namespace android
}  // namespace offline_items_collection
