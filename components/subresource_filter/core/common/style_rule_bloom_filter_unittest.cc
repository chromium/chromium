// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/subresource_filter/core/common/style_rule_bloom_filter.h"

#include <algorithm>
#include <string>
#include <vector>

#include "base/containers/flat_set.h"
#include "base/containers/span.h"
#include "base/strings/string_number_conversions.h"
#include "components/subresource_filter/core/common/indexed_ruleset.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace subresource_filter {

TEST(StyleRuleBloomFilterTest, EmptyFilter) {
  StyleRuleBloomFilter filter{base::span<const uint8_t>()};

  // If the bloom filter is empty it will always return true as a safe fallback,
  // to ensure that we look at the real list.
  EXPECT_TRUE(filter.MaybeContains(123));
  EXPECT_TRUE(filter.MaybeContains(456));
}

TEST(StyleRuleBloomFilterTest, SetAndCheck) {
  StyleRuleBloomFilterBuilder builder(10);
  builder.SetBits(123);
  builder.SetBits(456);

  StyleRuleBloomFilter filter(builder.buffer());
  EXPECT_TRUE(filter.MaybeContains(123));
  EXPECT_TRUE(filter.MaybeContains(456));

  // If the hashing algorithm changes this false test may need to be updated.
  EXPECT_FALSE(filter.MaybeContains(789));
}

TEST(StyleRuleBloomFilterTest, BuilderSizing) {
  StyleRuleBloomFilterBuilder builder(100);
  EXPECT_EQ(1024u, builder.buffer().size());

  StyleRuleBloomFilterBuilder builder2(10000);
  EXPECT_EQ(12500u, builder2.buffer().size());
}

TEST(StyleRuleBloomFilterTest, ManyItemsNoFalseNegatives) {
  StyleRuleBloomFilterBuilder builder(1000);
  std::vector<uint32_t> hashes;
  for (uint32_t i = 0; i < 500; ++i) {
    hashes.emplace_back(i * 100);
    builder.SetBits(i * 100);
  }

  StyleRuleBloomFilter filter(builder.buffer());
  for (uint32_t hash : hashes) {
    EXPECT_TRUE(filter.MaybeContains(hash));
  }
}

TEST(StyleRuleBloomFilterTest, FalsePositiveRate) {
  StyleRuleBloomFilterBuilder builder(
      100);  // Results in 1024 bytes = 8192 bits

  base::flat_set<uint32_t> inserted_hashes;
  for (uint32_t i = 0; i < 100; ++i) {
    std::string name = "class-" + base::NumberToString(i);
    uint32_t hash = GetStyleRuleHash(name);
    builder.SetBits(hash);
    inserted_hashes.insert(hash);
  }

  StyleRuleBloomFilter filter(builder.buffer());
  uint32_t false_positives = 0;
  uint32_t total_checks = 10000;
  uint32_t checked = 0;
  uint32_t i = 0;
  while (checked < total_checks) {
    std::string name = "check-" + base::NumberToString(i);
    uint32_t hash = GetStyleRuleHash(name);
    if (!inserted_hashes.contains(hash)) {
      if (filter.MaybeContains(hash)) {
        false_positives++;
      }
      checked++;
    }
    i++;
  }

  // With 100 items in a 1KB filter, the FP rate should be very low.
  // (1 - e^(-2*100/8192))^2 is approximately 0.0006.
  // Out of 10,000 checks, we expect around 6 false positives.
  EXPECT_LT(false_positives, 10u);
}

}  // namespace subresource_filter
