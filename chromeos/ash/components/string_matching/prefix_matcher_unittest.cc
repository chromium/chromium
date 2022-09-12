// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/string_matching/prefix_matcher.h"

#include "chromeos/ash/components/string_matching/tokenized_string.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::string_matching {

namespace {

using constants::kIsFrontOfTokenCharScore;
using constants::kIsPrefixCharScore;
using constants::kIsWeakHitCharScore;
using constants::kNoMatchScore;

constexpr double kAbsError = 1e-5;

}  // namespace

class PrefixMatcherTest : public testing::Test {};

// Note on expected score calculations:
//
// When a query successfully matches to a text, each letter of the query
// contributes some amount towards a final total. The expected score in
// each test is then the sum over all of the contributions of the individual
// query letters. This is described in more detail in prefix_matcher.cc.
//
// When a query does not successfully match to a text, the overall expected
// score is `kNoMatchScore`.

TEST_F(PrefixMatcherTest, ExactMatch) {
  TokenizedString query(u"abc def");
  TokenizedString text(u"abc def");

  PrefixMatcher pm(query, text);
  pm.Match();
  double expected_score = kIsPrefixCharScore * 6;
  EXPECT_NEAR(pm.relevance(), expected_score, kAbsError);
}

TEST_F(PrefixMatcherTest, ExactPrefixMatch) {
  TokenizedString query(u"abc def");
  TokenizedString text(u"abc defgh ijklm");

  PrefixMatcher pm(query, text);
  pm.Match();
  double expected_score = kIsPrefixCharScore * 6;
  EXPECT_NEAR(pm.relevance(), expected_score, kAbsError);
}

TEST_F(PrefixMatcherTest, ExactPrefixMatchFirstToken) {
  TokenizedString query(u"ab");
  TokenizedString text(u"abc def");

  PrefixMatcher pm(query, text);
  pm.Match();
  double expected_score = kIsPrefixCharScore * 2;
  EXPECT_NEAR(pm.relevance(), expected_score, kAbsError);
}

TEST_F(PrefixMatcherTest, ExactPrefixMatchNonFirstToken) {
  TokenizedString query(u"de");
  TokenizedString text(u"abc def");

  PrefixMatcher pm(query, text);
  pm.Match();
  double expected_score = kIsFrontOfTokenCharScore + kIsWeakHitCharScore;
  EXPECT_NEAR(pm.relevance(), expected_score, kAbsError);
}

TEST_F(PrefixMatcherTest, AcronymMatchConsecutiveTokensWithFirstTokenMatch) {
  TokenizedString query(u"abc");
  TokenizedString text(u"axx bxx cxx dxx exx");

  PrefixMatcher pm(query, text);
  pm.Match();
  double expected_score = kIsPrefixCharScore + (kIsFrontOfTokenCharScore * 2);
  EXPECT_NEAR(pm.relevance(), expected_score, kAbsError);
}

TEST_F(PrefixMatcherTest, AcronymMatchConsecutiveTokensWithNonFirstTokenMatch) {
  TokenizedString query(u"bcd");
  TokenizedString text(u"axx bxx cxx dxx exx");

  PrefixMatcher pm(query, text);
  pm.Match();
  double expected_score = kIsFrontOfTokenCharScore * 3;
  EXPECT_NEAR(pm.relevance(), expected_score, kAbsError);
}

TEST_F(PrefixMatcherTest, AcronymMatchNonConsecutiveTokens) {
  TokenizedString query(u"acd");
  TokenizedString text(u"axx bxx cxx dxx exx");

  PrefixMatcher pm(query, text);
  pm.Match();
  double expected_score = kNoMatchScore;
  EXPECT_NEAR(pm.relevance(), expected_score, kAbsError);
}

// TODO(crbug.com/1336160): Fully separate acronym matching from prefix
// matching.
TEST_F(PrefixMatcherTest, MixedAcronymAndPrefixMatching) {
  TokenizedString query(u"adefg");
  TokenizedString text(u"abc def ghi");

  PrefixMatcher pm(query, text);
  pm.Match();
  // Individual character's score contributions in order of matched letters (a,
  // d, e, f, g).
  double expected_score = kIsPrefixCharScore + kIsFrontOfTokenCharScore +
                          (kIsWeakHitCharScore * 2) + kIsFrontOfTokenCharScore;
  EXPECT_NEAR(pm.relevance(), expected_score, kAbsError);
}

TEST_F(PrefixMatcherTest, FinalPartialTokenConsideredMatch) {
  TokenizedString query(u"abc de");
  TokenizedString text(u"abc def");

  PrefixMatcher pm(query, text);
  pm.Match();
  double expected_score = kIsPrefixCharScore * 5;
  EXPECT_NEAR(pm.relevance(), expected_score, kAbsError);
}

TEST_F(PrefixMatcherTest, NonFinalPartialTokenConsideredNonMatch) {
  TokenizedString query(u"abce");
  TokenizedString text(u"a bcd e");

  PrefixMatcher pm(query, text);
  pm.Match();
  double expected_score = kNoMatchScore;
  EXPECT_NEAR(pm.relevance(), expected_score, kAbsError);
}

}  // namespace ash::string_matching
