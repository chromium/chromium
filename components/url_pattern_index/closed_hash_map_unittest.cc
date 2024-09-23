// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/url_pattern_index/closed_hash_map.h"

#include <string>
#include <vector>

#include "base/strings/string_number_conversions.h"
#include "components/url_pattern_index/uint64_hasher.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace url_pattern_index {

namespace {

template <typename MapType>
void ExpectHashMapIntegrity(const MapType& map, uint32_t min_capacity = 0) {
  EXPECT_EQ(map.entries().size(), map.size());
  EXPECT_EQ(map.hash_table().size(), map.table_size());
  EXPECT_LE(map.size() * 2, map.table_size());
  EXPECT_LE(min_capacity * 2, map.table_size());

  std::vector<bool> entry_is_referenced(map.size());
  for (uint32_t i = 0; i < map.table_size(); ++i) {
    SCOPED_TRACE(testing::Message() << "Hash-table slot: " << i);

    const uint32_t entry_index = map.hash_table()[i];
    if (static_cast<uint32_t>(entry_index) >= map.size())
      continue;
    EXPECT_FALSE(entry_is_referenced[entry_index]);
    entry_is_referenced[entry_index] = true;
  }

  for (uint32_t i = 0; i < map.size(); ++i) {
    SCOPED_TRACE(testing::Message() << "Hash-table entry index: " << i);
    EXPECT_TRUE(entry_is_referenced[i]);
  }
}

template <typename MapType>
void ExpectEmptyMap(const MapType& map, uint32_t min_capacity) {
  ExpectHashMapIntegrity(map, min_capacity);
  EXPECT_EQ(0u, map.size());
}

}  // namespace

TEST(ClosedHashMapTest, EmptyMapDefault) {
  HashMap<int, int> hm;
  ExpectEmptyMap(hm, 0);
  EXPECT_EQ(nullptr, hm.Get(0));
  EXPECT_EQ(nullptr, hm.Get(100500));
  EXPECT_GT(hm.table_size(), 0u);
}

TEST(ClosedHashMapTest, EmptyMapWithCapacity) {
  HashMap<int, int> hm(100);
  ExpectEmptyMap(hm, 100);
  EXPECT_EQ(nullptr, hm.Get(0));
  EXPECT_EQ(nullptr, hm.Get(100500));
}

TEST(ClosedHashMapTest, InsertDistinctAndGet) {
  HashMap<int, int> hm;
  static const int kKeys[] = {1, 5, 10, 3, -100500};
  for (int key : kKeys) {
    EXPECT_TRUE(hm.Insert(key, -key));
    ExpectHashMapIntegrity(hm);
  }
  for (int key : kKeys) {
    const int* value = hm.Get(key);
    ASSERT_NE(nullptr, value);
    EXPECT_EQ(-key, *value);
  }
  EXPECT_EQ(nullptr, hm.Get(1234567));
}

TEST(ClosedHashMapTest, InsertExistingAndGet) {
  HashMap<int, int> hm;

  EXPECT_TRUE(hm.Insert(123, -123));
  EXPECT_FALSE(hm.Insert(123, -124));
  ExpectHashMapIntegrity(hm);

  const int* value = hm.Get(123);
  ASSERT_NE(nullptr, value);
  EXPECT_EQ(-123, *value);
}

TEST(ClosedHashMapTest, InsertManyKeysWithCustomHasher) {
  using CustomProber = SimpleQuadraticProber<uint64_t, Uint64ToUint32Hasher>;

  ClosedHashMap<uint64_t, std::string, CustomProber> hm;
  ExpectEmptyMap(hm, 0);

  std::vector<std::pair<uint64_t, std::string>> entries;
  for (int key = 10, i = 0; key < 1000000; key += ++i) {
    entries.push_back(std::make_pair(key, base::NumberToString(key)));
  }

  uint32_t expected_size = 0;
  for (const auto& entry : entries) {
    EXPECT_TRUE(hm.Insert(entry.first, entry.second));
    EXPECT_FALSE(hm.Insert(entry.first, "-1"));
    ++expected_size;

    EXPECT_EQ(expected_size, hm.size());
    EXPECT_LE(expected_size * 2, hm.table_size());
  }
  ExpectHashMapIntegrity(hm);

  for (const auto& entry : entries) {
    const std::string* value = hm.Get(entry.first);
    ASSERT_NE(nullptr, value);
    EXPECT_EQ(entry.second, *value);
  }
}

TEST(ClosedHashMapTest, OperatorBrackets) {
  HashMap<int, int> hm;

  for (int i = 0; i < 5; ++i) {
    const uint32_t expected_size = i ? 1 : 0;
    EXPECT_EQ(expected_size, hm.size());

    int expected_value = (i + 1) * 10;
    hm[123] = expected_value;
    EXPECT_EQ(1u, hm.size());

    const int* value_ptr = hm.Get(123);
    ASSERT_NE(nullptr, value_ptr);
    EXPECT_EQ(expected_value, *value_ptr);

    EXPECT_EQ(expected_value, hm[123]);
    EXPECT_EQ(1u, hm.size());

    expected_value *= 100;
    hm[123] = expected_value;
    EXPECT_EQ(expected_value, hm[123]);
  }
}

TEST(ClosedHashMapTest, ManualRehash) {
  HashMap<int, int> hm(3);
  const uint32_t expected_table_size = hm.table_size();

  static const int kKeys[] = {1, 5, 10};
  for (int key : kKeys) {
    EXPECT_TRUE(hm.Insert(key, -key));
  }
  // No rehashing occurred.
  EXPECT_EQ(expected_table_size, hm.table_size());

  for (int i = 1; i <= 2; ++i) {
    for (int key : kKeys) {
      const int* value = hm.Get(key);
      ASSERT_NE(nullptr, value);
      EXPECT_EQ(-key, *value);
    }
    hm.Rehash(100 * i);
    ExpectHashMapIntegrity(hm, 100 * i);
  }
}

}  // namespace url_pattern_index
