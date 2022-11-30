// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/bloom_filter.h"

#include <stdint.h>
#include <string>

#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace optimization_guide {

namespace {

int CountBits(const ByteVector& vector) {
  int bit_count = 0;
  for (size_t i = 0; i < vector.size(); ++i) {
    uint8_t byte = vector[i];
    for (int j = 0; j < 8; ++j) {
      if (byte & (1 << j))
        bit_count++;
    }
  }
  return bit_count;
}

}  // namespace

TEST(BloomFilterTest, SingleHash) {
  BloomFilter filter(1 /* num_hash_functions */, 16 /* num_bits */);
  EXPECT_EQ(2u, filter.bytes().size());
  EXPECT_EQ(0, CountBits(filter.bytes()));

  EXPECT_FALSE(filter.Contains("Alfa"));
  EXPECT_FALSE(filter.Contains("Bravo"));
  EXPECT_FALSE(filter.Contains("Charlie"));

  filter.Add("Alfa");
  EXPECT_EQ(1, CountBits(filter.bytes()));
  EXPECT_TRUE(filter.Contains("Alfa"));
  EXPECT_FALSE(filter.Contains("Bravo"));
  EXPECT_FALSE(filter.Contains("Charlie"));

  filter.Add("Bravo");
  filter.Add("Chuck");
  EXPECT_EQ(3, CountBits(filter.bytes()));
  EXPECT_TRUE(filter.Contains("Alfa"));
  EXPECT_TRUE(filter.Contains("Bravo"));
  EXPECT_FALSE(filter.Contains("Charlie"));
}

TEST(BloomFilterTest, FalsePositivesWithSingleBitFilterCollisions) {
  BloomFilter filter(1 /* num_hash_functions */, 1 /* num_bits */);
  EXPECT_EQ(1u, filter.bytes().size());

  EXPECT_FALSE(filter.Contains("Alfa"));
  EXPECT_FALSE(filter.Contains("Bravo"));
  EXPECT_FALSE(filter.Contains("Charlie"));

  filter.Add("Alfa");
  EXPECT_TRUE(filter.Contains("Alfa"));
  EXPECT_TRUE(filter.Contains("Bravo"));
  EXPECT_TRUE(filter.Contains("Charlie"));
}

TEST(BloomFilterTest, MultiHash) {
  // Provide zero-ed filter data.
  std::string data(10, 0);
  BloomFilter filter(3 /* num_hash_functions */, 75 /* num_bits */, data);
  EXPECT_EQ(10u, filter.bytes().size());
  EXPECT_EQ(0, CountBits(filter.bytes()));

  EXPECT_FALSE(filter.Contains("Alfa"));
  EXPECT_FALSE(filter.Contains("Bravo"));
  EXPECT_FALSE(filter.Contains("Charlie"));

  filter.Add("Alfa");
  EXPECT_EQ(3, CountBits(filter.bytes()));
  EXPECT_TRUE(filter.Contains("Alfa"));
  EXPECT_FALSE(filter.Contains("Bravo"));
  EXPECT_FALSE(filter.Contains("Charlie"));

  filter.Add("Bravo");
  filter.Add("Chuck");
  EXPECT_EQ(9, CountBits(filter.bytes()));
  EXPECT_TRUE(filter.Contains("Alfa"));
  EXPECT_TRUE(filter.Contains("Bravo"));
  EXPECT_FALSE(filter.Contains("Charlie"));
}

TEST(BloomFilterTest, EverythingMatches) {
  // Provide filter data with all bits set ON.
  std::string data(1024, 0xff);
  BloomFilter filter(7 /* num_hash_functions */, 8191 /* num_bits */, data);

  EXPECT_TRUE(filter.Contains("Alfa"));
  EXPECT_TRUE(filter.Contains("Bravo"));
  EXPECT_TRUE(filter.Contains("Charlie"));
  EXPECT_TRUE(filter.Contains("Delta"));
  EXPECT_TRUE(filter.Contains("Echo"));
}

// Disable this test in configurations that don't print CHECK failures.
#if !BUILDFLAG(IS_IOS) && !(defined(OFFICIAL_BUILD) && defined(NDEBUG))
TEST(BloomFilterTest, ByteVectorTooSmall) {
  std::string data(1023, 0xff);
  EXPECT_DEATH(
      {
        BloomFilter filter(7 /* num_hash_functions */, 8191 /* num_bits */,
                           data);
      },
      "Check failed");
}
#endif

}  // namespace optimization_guide
