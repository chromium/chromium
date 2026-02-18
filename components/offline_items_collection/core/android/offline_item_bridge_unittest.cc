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
      : j_test_(JOfflineItemBridgeUnitTestClass::Constructor(
            AttachCurrentThread())) {}

  const jni_zero::ScopedJavaGlobalRef<JOfflineItemBridgeUnitTest>& j_test() {
    return j_test_;
  }

 private:
  jni_zero::ScopedJavaGlobalRef<JOfflineItemBridgeUnitTest> j_test_;
};

// Verfies a default offline item can be created in Java.
TEST_F(OfflineItemBridgeTest, CreateOfflineItem) {
  OfflineItem item;
  auto* env = AttachCurrentThread();
  auto j_offline_item = OfflineItemBridge::CreateOfflineItem(env, item);
  j_test()->testCreateDefaultOfflineItem(env, j_offline_item);
}

}  // namespace
}  // namespace android
}  // namespace offline_items_collection

DEFINE_JNI(OfflineItemBridgeUnitTest)
