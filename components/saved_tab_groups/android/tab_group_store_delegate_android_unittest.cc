// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/saved_tab_groups/android/tab_group_store_delegate_android.h"

#include "base/android/jni_android.h"
#include "base/test/bind.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "base/uuid.h"
#include "components/saved_tab_groups/tab_group_store_id.h"
#include "testing/gtest/include/gtest/gtest.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "components/saved_tab_groups/native_j_unittests_jni_headers/TabGroupStoreDelegateTestSupport_jni.h"

namespace tab_groups {

namespace {
void ClearAllPersistedData() {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_TabGroupStoreDelegateTestSupport_clearTabGroupMetadataPeristentStore(
      env);
}

}  // namespace

class TabGroupStoreDelegateAndroidTest : public testing::Test {
 public:
  TabGroupStoreDelegateAndroidTest() = default;
  ~TabGroupStoreDelegateAndroidTest() override = default;

  void TearDown() override { ClearAllPersistedData(); }

  std::map<base::Uuid, TabGroupIDMetadata> GetAll() {
    std::map<base::Uuid, TabGroupIDMetadata> outcome;
    base::RunLoop run_loop;
    delegate_.GetAllTabGroupIDMetadatas(base::BindLambdaForTesting(
        [&run_loop, &outcome](std::map<base::Uuid, TabGroupIDMetadata> result) {
          outcome = result;
          run_loop.Quit();
        }));
    run_loop.Run();
    return outcome;
  }

  base::test::TaskEnvironment task_environment_;
  TabGroupStoreDelegateAndroid delegate_;
};

TEST_F(TabGroupStoreDelegateAndroidTest, InitialStateIsEmpty) {
  std::map<base::Uuid, TabGroupIDMetadata> entries = GetAll();
  EXPECT_EQ(0UL, entries.size());
}

TEST_F(TabGroupStoreDelegateAndroidTest, SingleRandomEntryCanBeReadAndDeleted) {
  base::Uuid uuid = base::Uuid::GenerateRandomV4();
  TabGroupIDMetadata metadata(base::Token::CreateRandom());

  delegate_.StoreTabGroupIDMetadata(uuid, metadata);

  std::map<base::Uuid, TabGroupIDMetadata> entries = GetAll();
  EXPECT_EQ(1UL, entries.size());
  auto it = entries.find(uuid);
  EXPECT_FALSE(it == entries.end());
  EXPECT_EQ(metadata.local_tab_group_id, it->second.local_tab_group_id);

  delegate_.DeleteTabGroupIDMetdata(uuid);
  entries = GetAll();
  EXPECT_EQ(0UL, entries.size());
}

TEST_F(TabGroupStoreDelegateAndroidTest, MultipleEntriesCanBeWrittenAndRead) {
  base::Uuid uuid1 = base::Uuid::GenerateRandomV4();
  TabGroupIDMetadata metadata1(base::Token::CreateRandom());
  base::Uuid uuid2 = base::Uuid::GenerateRandomV4();
  TabGroupIDMetadata metadata2(base::Token::CreateRandom());

  delegate_.StoreTabGroupIDMetadata(uuid1, metadata1);
  delegate_.StoreTabGroupIDMetadata(uuid2, metadata2);

  std::map<base::Uuid, TabGroupIDMetadata> entries = GetAll();
  EXPECT_EQ(2UL, entries.size());
  auto it1 = entries.find(uuid1);
  EXPECT_FALSE(it1 == entries.end());
  EXPECT_EQ(metadata1.local_tab_group_id, it1->second.local_tab_group_id);
  auto it2 = entries.find(uuid2);
  EXPECT_FALSE(it2 == entries.end());
  EXPECT_EQ(metadata2.local_tab_group_id, it2->second.local_tab_group_id);

  delegate_.DeleteTabGroupIDMetdata(uuid1);
  entries = GetAll();
  EXPECT_EQ(1UL, entries.size());
  EXPECT_EQ(1UL, entries.count(uuid2));

  delegate_.DeleteTabGroupIDMetdata(uuid2);
  entries = GetAll();
  EXPECT_EQ(0UL, entries.size());
}

TEST_F(TabGroupStoreDelegateAndroidTest, HugeUint64CanBeUsed) {
  base::Uuid uuid = base::Uuid::GenerateRandomV4();
  // Since the data is crossing the JNI boundary and JNI does not support
  // uint64_t, this test ensures that nothing bad happens even in cases where
  // values that would overflow a long has issues.
  uint64_t overflow_high = (static_cast<uint64_t>(1) << 63);
  uint64_t overflow_low = (static_cast<uint64_t>(1) << 63) + 1;
  base::Token overflowing_token = base::Token(overflow_high, overflow_low);
  TabGroupIDMetadata metadata(overflowing_token);

  delegate_.StoreTabGroupIDMetadata(uuid, metadata);

  std::map<base::Uuid, TabGroupIDMetadata> entries = GetAll();
  EXPECT_EQ(1UL, entries.size());
  auto it = entries.find(uuid);
  EXPECT_FALSE(it == entries.end());
  EXPECT_EQ(metadata.local_tab_group_id, it->second.local_tab_group_id);

  delegate_.DeleteTabGroupIDMetdata(uuid);
  entries = GetAll();
  EXPECT_EQ(0UL, entries.size());
}

}  // namespace tab_groups
