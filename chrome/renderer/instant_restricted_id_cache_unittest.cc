// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <limits>
#include <string>
#include <utility>
#include <vector>

#include "chrome/renderer/instant_restricted_id_cache.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

struct TestData {
  TestData() {}
  explicit TestData(const std::string& i_value) : value(i_value) {}

  bool operator==(const TestData& rhs) const {
    return rhs.value == value;
  }

  std::string value;
};

// For printing failures nicely.
void PrintTo(const TestData& data, std::ostream* os) {
  *os << data.value;
}

}  // namespace

typedef testing::Test InstantRestrictedIDCacheTest;
typedef InstantRestrictedIDCache<TestData>::ItemIDPair ItemIDPair;

TEST_F(InstantRestrictedIDCacheTest, AutoIDGeneration) {
  InstantRestrictedIDCache<TestData> cache(7);
  EXPECT_EQ(0u, cache.cache_.size());
  EXPECT_EQ(0, cache.last_restricted_id_);

  // Check first addition.
  std::vector<TestData> input1;
  input1.push_back(TestData("A"));
  input1.push_back(TestData("B"));
  input1.push_back(TestData("C"));
  cache.AddItems(input1);
  EXPECT_EQ(3u, cache.cache_.size());
  EXPECT_EQ(3, cache.last_restricted_id_);

  std::vector<ItemIDPair> output;
  cache.GetCurrentItems(&output);
  EXPECT_EQ(3u, output.size());
  for (int i = 0; i < 3; ++i) {
    EXPECT_EQ(i + 1, output[i].first);
    EXPECT_EQ(input1[i], output[i].second);
  }

  TestData t;
  EXPECT_FALSE(cache.GetItemWithRestrictedID(4, &t));
  EXPECT_TRUE(cache.GetItemWithRestrictedID(3, &t));
  EXPECT_EQ(input1[2], t);

  // Add more items, no overflow.
  std::vector<TestData> input2;
  input2.push_back(TestData("D"));
  input2.push_back(TestData("E"));
  cache.AddItems(input2);
  EXPECT_EQ(5u, cache.cache_.size());
  EXPECT_EQ(5, cache.last_restricted_id_);

  output.clear();
  cache.GetCurrentItems(&output);
  EXPECT_EQ(2u, output.size());
  for (int i = 0; i < 2; ++i) {
    EXPECT_EQ(i + 4, output[i].first);
    EXPECT_EQ(input2[i], output[i].second);
  }

  EXPECT_FALSE(cache.GetItemWithRestrictedID(6, &t));
  EXPECT_TRUE(cache.GetItemWithRestrictedID(3, &t));
  EXPECT_EQ(input1[2], t);
  EXPECT_TRUE(cache.GetItemWithRestrictedID(5, &t));
  EXPECT_EQ(input2[1], t);

  // Add another set, overflows.
  std::vector<TestData> input3;
  input3.push_back(TestData("F"));
  input3.push_back(TestData("G"));
  input3.push_back(TestData("H"));
  input3.push_back(TestData("I"));
  cache.AddItems(input3);
  EXPECT_EQ(7u, cache.cache_.size());
  EXPECT_EQ(9, cache.last_restricted_id_);

  output.clear();
  cache.GetCurrentItems(&output);
  EXPECT_EQ(4u, output.size());
  for (int i = 0; i < 3; ++i) {
    EXPECT_EQ(i + 6, output[i].first);
    EXPECT_EQ(input3[i], output[i].second);
  }

  EXPECT_FALSE(cache.GetItemWithRestrictedID(1, &t));
  EXPECT_FALSE(cache.GetItemWithRestrictedID(2, &t));
  EXPECT_TRUE(cache.GetItemWithRestrictedID(3, &t));
  EXPECT_EQ(input1[2], t);
  EXPECT_TRUE(cache.GetItemWithRestrictedID(5, &t));
  EXPECT_EQ(input2[1], t);
  EXPECT_TRUE(cache.GetItemWithRestrictedID(7, &t));
  EXPECT_EQ(input3[1], t);
}

TEST_F(InstantRestrictedIDCacheTest, ManualIDGeneration) {
  InstantRestrictedIDCache<TestData> cache(5);
  EXPECT_EQ(0u, cache.cache_.size());
  EXPECT_EQ(0, cache.last_restricted_id_);

  // Check first addition.
  std::vector<ItemIDPair> input1;
  input1.push_back(std::make_pair(1, TestData("A")));
  input1.push_back(std::make_pair(2, TestData("B")));
  input1.push_back(std::make_pair(4, TestData("C")));
  cache.AddItemsWithRestrictedID(input1);
  EXPECT_EQ(3u, cache.cache_.size());
  EXPECT_EQ(4, cache.last_restricted_id_);

  std::vector<ItemIDPair> output;
  cache.GetCurrentItems(&output);
  EXPECT_EQ(3u, output.size());
  for (int i = 0; i < 3; ++i) {
    EXPECT_EQ(input1[i].first, output[i].first);
    EXPECT_EQ(input1[i].second, output[i].second);
  }

  TestData t;
  EXPECT_FALSE(cache.GetItemWithRestrictedID(3, &t));
  EXPECT_TRUE(cache.GetItemWithRestrictedID(4, &t));
  EXPECT_EQ(input1[2].second, t);


  // Add more items, one with same rid, no overflow.
  std::vector<ItemIDPair> input2;
  input2.push_back(std::make_pair(4, TestData("D")));
  input2.push_back(std::make_pair(7, TestData("E")));
  cache.AddItemsWithRestrictedID(input2);
  EXPECT_EQ(4u, cache.cache_.size());
  EXPECT_EQ(7, cache.last_restricted_id_);

  output.clear();
  cache.GetCurrentItems(&output);
  EXPECT_EQ(2u, output.size());
  for (int i = 0; i < 2; ++i) {
    EXPECT_EQ(input2[i].first, output[i].first);
    EXPECT_EQ(input2[i].second, output[i].second);
  }

  EXPECT_FALSE(cache.GetItemWithRestrictedID(6, &t));
  EXPECT_TRUE(cache.GetItemWithRestrictedID(2, &t));
  EXPECT_EQ(input1[1].second, t);
  EXPECT_TRUE(cache.GetItemWithRestrictedID(4, &t));
  EXPECT_EQ(input2[0].second, t);
  EXPECT_TRUE(cache.GetItemWithRestrictedID(7, &t));
  EXPECT_EQ(input2[1].second, t);

  // Add another set, duplicate rids, overflows.
  std::vector<ItemIDPair> input3;
  input3.push_back(std::make_pair(1, TestData("F")));
  input3.push_back(std::make_pair(6, TestData("G")));
  input3.push_back(std::make_pair(9, TestData("H")));
  cache.AddItemsWithRestrictedID(input3);
  EXPECT_EQ(5u, cache.cache_.size());
  EXPECT_EQ(9, cache.last_restricted_id_);

  output.clear();
  cache.GetCurrentItems(&output);
  EXPECT_EQ(3u, output.size());
  for (int i = 0; i < 3; ++i) {
    EXPECT_EQ(input3[i].first, output[i].first);
    EXPECT_EQ(input3[i].second, output[i].second);
  }

  EXPECT_TRUE(cache.GetItemWithRestrictedID(1, &t));
  EXPECT_EQ(input3[0].second, t);
  EXPECT_FALSE(cache.GetItemWithRestrictedID(2, &t));
  EXPECT_FALSE(cache.GetItemWithRestrictedID(3, &t));
  EXPECT_TRUE(cache.GetItemWithRestrictedID(4, &t));
  EXPECT_EQ(input2[0].second, t);
  EXPECT_TRUE(cache.GetItemWithRestrictedID(7, &t));
  EXPECT_EQ(input2[1].second, t);
  EXPECT_FALSE(cache.GetItemWithRestrictedID(8, &t));
  EXPECT_TRUE(cache.GetItemWithRestrictedID(9, &t));
  EXPECT_EQ(input3[2].second, t);
}

TEST_F(InstantRestrictedIDCacheTest, CrazyIDGeneration) {
  InstantRestrictedIDCache<TestData> cache(4);
  EXPECT_EQ(0u, cache.cache_.size());
  EXPECT_EQ(0, cache.last_restricted_id_);

  // Check first addition.
  std::vector<ItemIDPair> input1;
  input1.push_back(std::make_pair(0, TestData("A")));
  input1.push_back(
      std::make_pair(std::numeric_limits<int32_t>::max(), TestData("B")));
  input1.push_back(std::make_pair(-100, TestData("C")));
  cache.AddItemsWithRestrictedID(input1);
  EXPECT_EQ(3u, cache.cache_.size());
  EXPECT_EQ(std::numeric_limits<int32_t>::max(), cache.last_restricted_id_);

  std::vector<ItemIDPair> output;
  cache.GetCurrentItems(&output);
  EXPECT_EQ(3u, output.size());
  for (int i = 0; i < 3; ++i) {
    EXPECT_EQ(input1[i].first, output[i].first);
    EXPECT_EQ(input1[i].second, output[i].second);
  }

  TestData t;
  EXPECT_FALSE(cache.GetItemWithRestrictedID(1, &t));
  EXPECT_TRUE(
      cache.GetItemWithRestrictedID(std::numeric_limits<int32_t>::max(), &t));
  EXPECT_EQ(input1[1].second, t);
  EXPECT_TRUE(cache.GetItemWithRestrictedID(-100, &t));
  EXPECT_EQ(input1[2].second, t);

  // Add more items, one with same rid, no overflow.
  std::vector<ItemIDPair> input2;
  input2.push_back(
      std::make_pair(std::numeric_limits<int32_t>::min(), TestData("D")));
  input2.push_back(std::make_pair(7, TestData("E")));
  cache.AddItemsWithRestrictedID(input2);
  EXPECT_EQ(4u, cache.cache_.size());
  EXPECT_EQ(std::numeric_limits<int32_t>::max(), cache.last_restricted_id_);

  output.clear();
  cache.GetCurrentItems(&output);
  EXPECT_EQ(2u, output.size());
  for (int i = 0; i < 2; ++i) {
    EXPECT_EQ(input2[i].first, output[i].first);
    EXPECT_EQ(input2[i].second, output[i].second);
  }

  EXPECT_FALSE(cache.GetItemWithRestrictedID(0, &t));
  EXPECT_TRUE(
      cache.GetItemWithRestrictedID(std::numeric_limits<int32_t>::max(), &t));
  EXPECT_EQ(input1[1].second, t);
  EXPECT_TRUE(
      cache.GetItemWithRestrictedID(std::numeric_limits<int32_t>::min(), &t));
  EXPECT_EQ(input2[0].second, t);
  EXPECT_TRUE(cache.GetItemWithRestrictedID(7, &t));
  EXPECT_EQ(input2[1].second, t);

  // Add an item without RID. last_restricted_id_ will overflow.
  std::vector<TestData> input3;
  input3.push_back(TestData("F"));
  input3.push_back(TestData("G"));
  cache.AddItems(input3);
  EXPECT_EQ(4u, cache.cache_.size());
  EXPECT_EQ(std::numeric_limits<int32_t>::min() + 1, cache.last_restricted_id_);

  output.clear();
  cache.GetCurrentItems(&output);
  EXPECT_EQ(2u, output.size());
  for (int i = 0; i < 2; ++i) {
    EXPECT_EQ(std::numeric_limits<int32_t>::min() + i, output[i].first);
    EXPECT_EQ(input3[i], output[i].second);
  }

  EXPECT_TRUE(
      cache.GetItemWithRestrictedID(std::numeric_limits<int32_t>::min(), &t));
  EXPECT_EQ(input3[0], t);
}

TEST_F(InstantRestrictedIDCacheTest, MixIDGeneration) {
  InstantRestrictedIDCache<TestData> cache(5);
  EXPECT_EQ(0u, cache.cache_.size());
  EXPECT_EQ(0, cache.last_restricted_id_);

  // Add some items with manually assigned ids.
  std::vector<ItemIDPair> input1;
  input1.push_back(std::make_pair(1, TestData("A")));
  input1.push_back(std::make_pair(2, TestData("B")));
  input1.push_back(std::make_pair(4, TestData("C")));
  cache.AddItemsWithRestrictedID(input1);
  EXPECT_EQ(3u, cache.cache_.size());
  EXPECT_EQ(4, cache.last_restricted_id_);

  std::vector<ItemIDPair> output;
  cache.GetCurrentItems(&output);
  EXPECT_EQ(3u, output.size());
  for (int i = 0; i < 3; ++i) {
    EXPECT_EQ(input1[i].first, output[i].first);
    EXPECT_EQ(input1[i].second, output[i].second);
  }

  TestData t;
  EXPECT_FALSE(cache.GetItemWithRestrictedID(3, &t));
  EXPECT_TRUE(cache.GetItemWithRestrictedID(4, &t));
  EXPECT_EQ(input1[2].second, t);

  // Add items with auto id generation.
  std::vector<TestData> input2;
  input2.push_back(TestData("D"));
  input2.push_back(TestData("E"));
  cache.AddItems(input2);
  EXPECT_EQ(5u, cache.cache_.size());
  EXPECT_EQ(6, cache.last_restricted_id_);

  output.clear();
  cache.GetCurrentItems(&output);
  EXPECT_EQ(2u, output.size());
  for (int i = 0; i < 2; ++i) {
    EXPECT_EQ(i + 5, output[i].first);
    EXPECT_EQ(input2[i], output[i].second);
  }

  EXPECT_FALSE(cache.GetItemWithRestrictedID(3, &t));
  EXPECT_TRUE(cache.GetItemWithRestrictedID(2, &t));
  EXPECT_EQ(input1[1].second, t);
  EXPECT_TRUE(cache.GetItemWithRestrictedID(4, &t));
  EXPECT_EQ(input1[2].second, t);
  EXPECT_TRUE(cache.GetItemWithRestrictedID(5, &t));
  EXPECT_EQ(input2[0], t);
  EXPECT_TRUE(cache.GetItemWithRestrictedID(6, &t));
  EXPECT_EQ(input2[1], t);
  EXPECT_FALSE(cache.GetItemWithRestrictedID(7, &t));

  // Add manually assigned ids again.
  std::vector<ItemIDPair> input3;
  input3.push_back(std::make_pair(1, TestData("F")));
  input3.push_back(std::make_pair(5, TestData("G")));
  input3.push_back(std::make_pair(7, TestData("H")));
  cache.AddItemsWithRestrictedID(input3);
  EXPECT_EQ(5u, cache.cache_.size());
  EXPECT_EQ(7, cache.last_restricted_id_);

  output.clear();
  cache.GetCurrentItems(&output);
  EXPECT_EQ(3u, output.size());
  for (int i = 0; i < 2; ++i) {
    EXPECT_EQ(input3[i].first, output[i].first);
    EXPECT_EQ(input3[i].second, output[i].second);
  }

  EXPECT_TRUE(cache.GetItemWithRestrictedID(1, &t));
  EXPECT_EQ(input3[0].second, t);
  EXPECT_FALSE(cache.GetItemWithRestrictedID(2, &t));
  EXPECT_TRUE(cache.GetItemWithRestrictedID(4, &t));
  EXPECT_EQ(input1[2].second, t);
  EXPECT_TRUE(cache.GetItemWithRestrictedID(5, &t));
  EXPECT_EQ(input3[1].second, t);
  EXPECT_TRUE(cache.GetItemWithRestrictedID(6, &t));
  EXPECT_EQ(input2[1], t);
  EXPECT_TRUE(cache.GetItemWithRestrictedID(7, &t));
  EXPECT_EQ(input3[2].second, t);
}

TEST_F(InstantRestrictedIDCacheTest, AddEmptySet) {
  InstantRestrictedIDCache<TestData> cache(9);
  EXPECT_EQ(0u, cache.cache_.size());
  EXPECT_EQ(0, cache.last_restricted_id_);

  // Add a non-empty set of items.
  std::vector<TestData> input1;
  input1.push_back(TestData("A"));
  input1.push_back(TestData("B"));
  input1.push_back(TestData("C"));
  cache.AddItems(input1);
  EXPECT_EQ(3u, cache.cache_.size());
  EXPECT_EQ(3, cache.last_restricted_id_);

  std::vector<ItemIDPair> output;
  cache.GetCurrentItems(&output);
  EXPECT_EQ(3u, output.size());

  // Add an empty set.
  cache.AddItems(std::vector<TestData>());
  EXPECT_EQ(3u, cache.cache_.size());
  EXPECT_EQ(3, cache.last_restricted_id_);

  cache.GetCurrentItems(&output);
  EXPECT_TRUE(output.empty());

  // Manual IDs.
  std::vector<ItemIDPair> input2;
  input2.push_back(std::make_pair(10, TestData("A")));
  input2.push_back(std::make_pair(11, TestData("B")));
  input2.push_back(std::make_pair(12, TestData("C")));
  cache.AddItemsWithRestrictedID(input2);
  EXPECT_EQ(6u, cache.cache_.size());
  EXPECT_EQ(12, cache.last_restricted_id_);

  cache.GetCurrentItems(&output);
  EXPECT_EQ(3u, output.size());

  cache.AddItemsWithRestrictedID(std::vector<ItemIDPair>());
  EXPECT_EQ(6u, cache.cache_.size());
  EXPECT_EQ(12, cache.last_restricted_id_);

  cache.GetCurrentItems(&output);
  EXPECT_TRUE(output.empty());
}

TEST_F(InstantRestrictedIDCacheTest, AddItemsWithRestrictedID) {
  InstantRestrictedIDCache<TestData> cache(29);
  EXPECT_EQ(0u, cache.cache_.size());
  EXPECT_EQ(0, cache.last_restricted_id_);

  std::vector<ItemIDPair> input1;
  input1.push_back(std::make_pair(10, TestData("A")));
  input1.push_back(std::make_pair(11, TestData("B")));
  input1.push_back(std::make_pair(12, TestData("C")));
  cache.AddItemsWithRestrictedID(input1);
  EXPECT_EQ(3u, cache.cache_.size());
  EXPECT_EQ(12, cache.last_restricted_id_);
  EXPECT_EQ(10, cache.last_add_start_->first);

  std::vector<ItemIDPair> output;
  cache.GetCurrentItems(&output);
  EXPECT_EQ(3u, output.size());

  // Add the same items again.
  cache.AddItemsWithRestrictedID(input1);

  // Make sure |cache.last_add_start_| is still valid.
  cache.GetCurrentItems(&output);
  EXPECT_EQ(3u, output.size());
  EXPECT_EQ(3u, cache.cache_.size());
  EXPECT_EQ(12, cache.last_restricted_id_);
  EXPECT_EQ(10, cache.last_add_start_->first);
}
