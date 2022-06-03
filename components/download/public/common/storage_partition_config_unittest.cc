// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/download/public/common/storage_partition_config.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace download {

TEST(StoragePartitionConfigTest, EqualityOperators) {
  StoragePartitionConfig c1(std::string(), std::string(), false);
  StoragePartitionConfig c2(std::string(), std::string(), true);
  StoragePartitionConfig c3("a", std::string(), true);
  StoragePartitionConfig c4("b", std::string(), true);
  StoragePartitionConfig c5(std::string(), "abc", false);
  StoragePartitionConfig c6(std::string(), "abc", true);
  StoragePartitionConfig c7("a", "abc", true);
  StoragePartitionConfig c8("a", "abc", true);

  EXPECT_FALSE(c1 == c2);
  EXPECT_FALSE(c1 == c8);
  EXPECT_FALSE(c2 == c8);
  EXPECT_FALSE(c3 == c4);
  EXPECT_FALSE(c3 == c8);
  EXPECT_FALSE(c4 == c8);
  EXPECT_FALSE(c5 == c6);
  EXPECT_FALSE(c5 == c8);
  EXPECT_FALSE(c6 == c8);
  EXPECT_TRUE(c7 == c8);
  EXPECT_TRUE(c8 == c8);
}

TEST(StoragePartitionConfigTest, SerializeToString) {
  // Check that all of the config's values are serialized properly.
  StoragePartitionConfig c1("a", "abc", true);
  auto c1_str = c1.SerializeToString();
  EXPECT_EQ(c1_str, std::string("a|abc|in_memory"));

  // Check that "in_memory" is not set if the value is false.
  StoragePartitionConfig c2("b", "xyz", false);
  auto c2_str = c2.SerializeToString();
  EXPECT_EQ(c2_str, std::string("b|xyz|"));

  // Check that the partition domain is not set if the value is empty.
  StoragePartitionConfig c3("", "def", true);
  auto c3_str = c3.SerializeToString();
  EXPECT_EQ(c3_str, std::string("|def|in_memory"));

  // Check that the partition name is not set if the value is empty.
  StoragePartitionConfig c4("uvw", "", false);
  auto c4_str = c4.SerializeToString();
  EXPECT_EQ(c4_str, std::string("uvw||"));

  // Check that no values are set if the values are empty or false.
  StoragePartitionConfig c5("", "", false);
  auto c5_str = c5.SerializeToString();
  EXPECT_EQ(c5_str, std::string("||"));
}

TEST(StoragePartitionConfigTest, DeserializeFromString) {
  // Check that all of the values are deserialized properly into the config.
  std::string c1_str = "a|abc|in_memory";
  StoragePartitionConfig expected_c1("a", "abc", true);
  EXPECT_EQ(StoragePartitionConfig::DeserializeFromString(c1_str), expected_c1);

  // Check that deserialization is correct if in memory is missing.
  std::string c2_str = "b|xyz|";
  StoragePartitionConfig expected_c2("b", "xyz", false);
  EXPECT_EQ(StoragePartitionConfig::DeserializeFromString(c2_str), expected_c2);

  // Check that deserialization is correct if partition domain is missing.
  std::string c3_str = "|def|in_memory";
  StoragePartitionConfig expected_c3("", "def", true);
  EXPECT_EQ(StoragePartitionConfig::DeserializeFromString(c3_str), expected_c3);

  // Check that deserialization is correct if partition name is missing.
  std::string c4_str = "uvw||";
  StoragePartitionConfig expected_c4("uvw", "", false);
  EXPECT_EQ(StoragePartitionConfig::DeserializeFromString(c4_str), expected_c4);

  std::string empty_str = "";
  StoragePartitionConfig expected_c5("", "", false);
  EXPECT_EQ(StoragePartitionConfig::DeserializeFromString(empty_str),
            expected_c5);

  std::string invalid_value_str = "a|abc|invalid";
  StoragePartitionConfig expected_c6("a", "abc", false);
  EXPECT_EQ(StoragePartitionConfig::DeserializeFromString(invalid_value_str),
            expected_c6);

  std::string too_many_delimiters_str = "a|abc|in_memory|extra|delimiters";
  StoragePartitionConfig expected_c7("", "", false);
  EXPECT_EQ(
      StoragePartitionConfig::DeserializeFromString(too_many_delimiters_str),
      expected_c7);
}

}  // namespace download
