// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/integrators/at_memory/at_memory_string_filtering_util.h"

#include "components/autofill/core/browser/data_model/addresses/autofill_normalization_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

namespace {

// Tests exact token matching without edits.
TEST(AtMemoryStringFilteringUtilTest, FuzzyMatchesSingleToken_ExactMatch) {
  EXPECT_TRUE(FuzzyMatchesSingleToken(u"john", u"john"));
  EXPECT_TRUE(FuzzyMatchesSingleToken(u"street", u"street"));
}

// Tests short query tokens (len <= 2) which require 0 edits.
TEST(AtMemoryStringFilteringUtilTest, FuzzyMatchesSingleToken_ShortTokens) {
  EXPECT_TRUE(FuzzyMatchesSingleToken(u"john", u"jo"));
  EXPECT_FALSE(FuzzyMatchesSingleToken(u"john", u"ji"));
}

// Tests medium-length query tokens (3 <= len <= 5) which allow up to 1 edit.
TEST(AtMemoryStringFilteringUtilTest, FuzzyMatchesSingleToken_MediumTokens) {
  EXPECT_TRUE(FuzzyMatchesSingleToken(u"john", u"jhn"));
  EXPECT_TRUE(FuzzyMatchesSingleToken(u"street", u"strt"));
  EXPECT_FALSE(FuzzyMatchesSingleToken(u"john", u"xyz"));
}

// Tests long query tokens (len > 5) which allow up to 2 edits.
TEST(AtMemoryStringFilteringUtilTest, FuzzyMatchesSingleToken_LongTokens) {
  EXPECT_TRUE(FuzzyMatchesSingleToken(u"street", u"stteet"));
  EXPECT_TRUE(FuzzyMatchesSingleToken(u"passport", u"passprt"));
  EXPECT_FALSE(FuzzyMatchesSingleToken(u"passport", u"abcdefg"));
}

// Tests prefix fuzzy matching for incomplete tokens typed during user input.
TEST(AtMemoryStringFilteringUtilTest, FuzzyMatchesSingleToken_PrefixMatch) {
  EXPECT_TRUE(FuzzyMatchesSingleToken(u"smith", u"smi"));
  EXPECT_TRUE(FuzzyMatchesSingleToken(u"smith", u"smt"));
}

// Tests multi-token ordered fuzzy matching with typos and word skips.
TEST(AtMemoryStringFilteringUtilTest, FuzzyMatchesOrderedTokens_Basic) {
  EXPECT_TRUE(FuzzyMatchesOrderedTokens(
      normalization::NormalizeForComparison(u"John Smith"),
      normalization::NormalizeForComparison(u"john smith")));
  EXPECT_TRUE(FuzzyMatchesOrderedTokens(
      normalization::NormalizeForComparison(u"Dr. John Alex Smith"),
      normalization::NormalizeForComparison(u"jhn smi")));
}

// Tests that out-of-order query tokens fail to match.
TEST(AtMemoryStringFilteringUtilTest,
     FuzzyMatchesOrderedTokens_OutOfOrderFails) {
  EXPECT_FALSE(FuzzyMatchesOrderedTokens(
      normalization::NormalizeForComparison(u"John Smith"),
      normalization::NormalizeForComparison(u"smi jhn")));
}

// Tests that queries containing more tokens than the target entry fail to
// match.
TEST(AtMemoryStringFilteringUtilTest,
     FuzzyMatchesOrderedTokens_TooManyQueryTokens) {
  EXPECT_FALSE(FuzzyMatchesOrderedTokens(
      normalization::NormalizeForComparison(u"John Smith"),
      normalization::NormalizeForComparison(u"john alex smith jr")));
}

// Tests that an empty filter string matches any target string.
TEST(AtMemoryStringFilteringUtilTest, FuzzyMatchesOrderedTokens_EmptyFilter) {
  EXPECT_TRUE(FuzzyMatchesOrderedTokens(
      normalization::NormalizeForComparison(u"John Smith"), u""));
}

}  // namespace

}  // namespace autofill
