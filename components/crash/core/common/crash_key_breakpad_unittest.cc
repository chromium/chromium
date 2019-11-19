// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/crash/core/common/crash_key.h"

#include "components/crash/core/common/crash_key_internal.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace crash_reporter {

class CrashKeyBreakpadTest : public testing::Test {
 public:
  void SetUp() override {
    internal::ResetCrashKeyStorageForTesting();
    InitializeCrashKeys();
    ASSERT_TRUE(internal::GetCrashKeyStorage());
  }

  void TearDown() override { internal::ResetCrashKeyStorageForTesting(); }

  internal::TransitionalCrashKeyStorage* storage() {
    return internal::GetCrashKeyStorage();
  }

  size_t* GetIndexArray(internal::CrashKeyStringImpl* key) {
    return key->index_array_;
  }
  size_t GetIndexArrayCount(internal::CrashKeyStringImpl* key) {
    return key->index_array_count_;
  }
};

TEST_F(CrashKeyBreakpadTest, ConstantAssertions) {
  // Tests in this file generate and validate data based on constants
  // having specific values. This test asserts those assumptions.
  EXPECT_EQ(128u, internal::kCrashKeyStorageValueSize);
}

TEST_F(CrashKeyBreakpadTest, Allocation) {
  const size_t kSentinel = internal::kCrashKeyStorageNumEntries;

  static CrashKeyStringBreakpad<32> key1("short");
  ASSERT_EQ(1u, GetIndexArrayCount(&key1));
  auto* indexes = GetIndexArray(&key1);
  EXPECT_EQ(kSentinel, indexes[0]);

  // An extra index slot is created for lengths equal to the value size.
  static CrashKeyStringBreakpad<128> key2("extra");
  ASSERT_EQ(2u, GetIndexArrayCount(&key2));
  indexes = GetIndexArray(&key2);
  EXPECT_EQ(kSentinel, indexes[0]);
  EXPECT_EQ(kSentinel, indexes[1]);

  static CrashKeyStringBreakpad<395> key3("large");
  ASSERT_EQ(4u, GetIndexArrayCount(&key3));
  indexes = GetIndexArray(&key3);
  EXPECT_EQ(kSentinel, indexes[0]);
  EXPECT_EQ(kSentinel, indexes[1]);
  EXPECT_EQ(kSentinel, indexes[2]);
  EXPECT_EQ(kSentinel, indexes[3]);
}

TEST_F(CrashKeyBreakpadTest, SetClearSingle) {
  static CrashKeyStringBreakpad<32> key("test-key");

  EXPECT_FALSE(storage()->GetValueForKey("test-key"));
  EXPECT_EQ(0u, storage()->GetCount());

  key.Set("value");

  ASSERT_EQ(1u, storage()->GetCount());
  EXPECT_STREQ("value", storage()->GetValueForKey("test-key"));

  key.Set("value 2");

  ASSERT_EQ(1u, storage()->GetCount());
  EXPECT_STREQ("value 2", storage()->GetValueForKey("test-key"));

  key.Clear();

  EXPECT_FALSE(storage()->GetValueForKey("test-key"));
  EXPECT_EQ(0u, storage()->GetCount());
}

TEST_F(CrashKeyBreakpadTest, SetChunked) {
  std::string chunk1(128, 'A');
  std::string chunk2(128, 'B');
  std::string chunk3(128, 'C');

  static CrashKeyStringBreakpad<400> key("chunky");

  EXPECT_EQ(0u, storage()->GetCount());

  key.Set((chunk1 + chunk2 + chunk3).c_str());

  ASSERT_EQ(4u, storage()->GetCount());

  // Since chunk1 through chunk3 are the same size as a storage slot,
  // and the storage NUL-terminates the value, ensure no bytes are
  // lost when chunking.
  EXPECT_EQ(std::string(127, 'A'), storage()->GetValueForKey("chunky__1"));
  EXPECT_EQ(std::string("A") + std::string(126, 'B'),
            storage()->GetValueForKey("chunky__2"));
  EXPECT_EQ(std::string(2, 'B') + std::string(125, 'C'),
            storage()->GetValueForKey("chunky__3"));
  EXPECT_EQ(std::string(3, 'C'), storage()->GetValueForKey("chunky__4"));

  std::string chunk4(240, 'D');

  key.Set(chunk4.c_str());

  ASSERT_EQ(2u, storage()->GetCount());

  EXPECT_EQ(std::string(127, 'D'), storage()->GetValueForKey("chunky__1"));
  EXPECT_EQ(std::string(240 - 127, 'D'),
            storage()->GetValueForKey("chunky__2"));
  EXPECT_FALSE(storage()->GetValueForKey("chunky__3"));

  key.Clear();

  EXPECT_EQ(0u, storage()->GetCount());
}

TEST_F(CrashKeyBreakpadTest, SetTwoChunked) {
  static CrashKeyStringBreakpad<600> key1("big");
  static CrashKeyStringBreakpad<256> key2("small");

  EXPECT_EQ(0u, storage()->GetCount());

  key1.Set(std::string(200, '1').c_str());

  ASSERT_EQ(2u, storage()->GetCount());

  EXPECT_EQ(std::string(127, '1'), storage()->GetValueForKey("big__1"));
  EXPECT_EQ(std::string(73, '1'), storage()->GetValueForKey("big__2"));

  key2.Set(std::string(256, '2').c_str());

  ASSERT_EQ(5u, storage()->GetCount());

  EXPECT_EQ(std::string(127, '1'), storage()->GetValueForKey("big__1"));
  EXPECT_EQ(std::string(73, '1'), storage()->GetValueForKey("big__2"));
  EXPECT_EQ(std::string(127, '2'), storage()->GetValueForKey("small__1"));
  EXPECT_EQ(std::string(127, '2'), storage()->GetValueForKey("small__2"));
  EXPECT_EQ(std::string(2, '2'), storage()->GetValueForKey("small__3"));

  key1.Set(std::string(510, '3').c_str());

  ASSERT_EQ(8u, storage()->GetCount());

  EXPECT_EQ(std::string(127, '3'), storage()->GetValueForKey("big__1"));
  EXPECT_EQ(std::string(127, '3'), storage()->GetValueForKey("big__2"));
  EXPECT_EQ(std::string(127, '3'), storage()->GetValueForKey("big__3"));
  EXPECT_EQ(std::string(127, '3'), storage()->GetValueForKey("big__4"));
  EXPECT_EQ(std::string(2, '3'), storage()->GetValueForKey("big__5"));
  EXPECT_EQ(std::string(127, '2'), storage()->GetValueForKey("small__1"));
  EXPECT_EQ(std::string(127, '2'), storage()->GetValueForKey("small__2"));
  EXPECT_EQ(std::string(2, '2'), storage()->GetValueForKey("small__3"));

  key2.Clear();

  ASSERT_EQ(5u, storage()->GetCount());

  EXPECT_EQ(std::string(127, '3'), storage()->GetValueForKey("big__1"));
  EXPECT_EQ(std::string(127, '3'), storage()->GetValueForKey("big__2"));
  EXPECT_EQ(std::string(127, '3'), storage()->GetValueForKey("big__3"));
  EXPECT_EQ(std::string(127, '3'), storage()->GetValueForKey("big__4"));
  EXPECT_EQ(std::string(2, '3'), storage()->GetValueForKey("big__5"));
}

TEST_F(CrashKeyBreakpadTest, ChunkSingleEntry) {
  static CrashKeyStringBreakpad<200> crash_key("split");

  EXPECT_EQ(0u, storage()->GetCount());

  crash_key.Set("test");

  ASSERT_EQ(1u, storage()->GetCount());
  EXPECT_STREQ("test", storage()->GetValueForKey("split"));

  crash_key.Set(std::string(127, 'z') + "bloop");

  ASSERT_EQ(2u, storage()->GetCount());
  EXPECT_EQ(std::string(127, 'z'), storage()->GetValueForKey("split__1"));
  EXPECT_STREQ("bloop", storage()->GetValueForKey("split__2"));

  crash_key.Set("abcdefg");

  ASSERT_EQ(1u, storage()->GetCount());
  EXPECT_STREQ("abcdefg", storage()->GetValueForKey("split"));

  crash_key.Set("hijklmnop");

  ASSERT_EQ(1u, storage()->GetCount());
  EXPECT_STREQ("hijklmnop", storage()->GetValueForKey("split"));
}

}  // namespace crash_reporter
