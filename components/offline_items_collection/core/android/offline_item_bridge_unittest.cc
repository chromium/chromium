// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_items_collection/core/android/offline_item_bridge.h"

#include "base/android/jni_android.h"
#include "components/offline_items_collection/core/native_j_unittests_jni_headers/OfflineItemBridgeUnitTest_jni.h"
#include "testing/gtest/include/gtest/gtest.h"

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

// Verifies OfflineItemSchedule can be plumbed to Java correctly.
TEST_F(OfflineItemBridgeTest, OfflineItemSchedule) {
  // OfflineItemSchedule only on wifi.
  auto* env = AttachCurrentThread();
  OfflineItem item;
  item.schedule = absl::make_optional<OfflineItemSchedule>(
      true /*only_on_wifi*/, absl::nullopt);
  auto j_offline_item = OfflineItemBridge::CreateOfflineItem(env, item);
  Java_OfflineItemBridgeUnitTest_testOfflineItemSchedule(
      env, j_test(), j_offline_item, true /*only_on_wifi*/, 0);

  // OfflineItemSchedule with specific start time.
  auto start_time = base::Time::FromDeltaSinceWindowsEpoch(base::Days(1));
  item.schedule = absl::make_optional<OfflineItemSchedule>(
      false /*only_on_wifi*/, start_time);
  EXPECT_EQ(start_time, item.schedule->start_time);
  EXPECT_FALSE(start_time.is_null());
  j_offline_item = OfflineItemBridge::CreateOfflineItem(env, item);
  Java_OfflineItemBridgeUnitTest_testOfflineItemSchedule(
      env, j_test(), j_offline_item, false /*only_on_wifi*/,
      start_time.ToJavaTime());
}

}  // namespace
}  // namespace android
}  // namespace offline_items_collection
