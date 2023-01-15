// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/string_matching/prefix_matcher.h"

#include "chromeos/ash/components/string_matching/tokenized_string.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::string_matching {

namespace {

using prefix_matcher_constants::kIsFrontOfTokenCharScore;
using prefix_matcher_constants::kIsPrefixCharScore;
using prefix_matcher_constants::kIsWeakHitCharScore;
using prefix_matcher_constants::kNoMatchScore;

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

TEST_F(PrefixMatcherTest, AcronymMatchFirstTokenMatchConsideredNonMatch) {
  TokenizedString query(u"abc");
  TokenizedString text(u"axx bxx cxx dxx exx");

  PrefixMatcher pm(query, text);
  pm.Match();
  double expected_score = kNoMatchScore;
  EXPECT_NEAR(pm.relevance(), expected_score, kAbsError);
}

TEST_F(PrefixMatcherTest, AcronymMatchNonFirstTokenMatchConsideredNonMatch) {
  TokenizedString query(u"bcd");
  TokenizedString text(u"axx bxx cxx dxx exx");

  PrefixMatcher pm(query, text);
  pm.Match();
  double expected_score = kNoMatchScore;
  EXPECT_NEAR(pm.relevance(), expected_score, kAbsError);
}

TEST_F(PrefixMatcherTest, AcronymMatchNonConsecutiveConsideredNonMatch) {
  TokenizedString query(u"acd");
  TokenizedString text(u"axx bxx cxx dxx exx");

  PrefixMatcher pm(query, text);
  pm.Match();
  double expected_score = kNoMatchScore;
  EXPECT_NEAR(pm.relevance(), expected_score, kAbsError);
}

TEST_F(PrefixMatcherTest, MixedAcronymAndPrefixMatchingConsideredNonMatch) {
  TokenizedString query(u"adefg");
  TokenizedString text(u"abc def ghi");

  PrefixMatcher pm(query, text);
  pm.Match();

  double expected_score = kNoMatchScore;
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

TEST_F(PrefixMatcherTest, ExactPrefixMatchDiscrete) {
  TokenizedString query(u"abc ghi");
  TokenizedString text(u"abc def ghi");

  PrefixMatcher pm(query, text);
  pm.Match();
  double expected_score = kIsPrefixCharScore * 3 + kIsFrontOfTokenCharScore +
                          kIsWeakHitCharScore * 2;
  EXPECT_NEAR(pm.relevance(), expected_score, kAbsError);
}

TEST_F(PrefixMatcherTest, ExactPrefixMatchDiscreteNonFirstToken) {
  TokenizedString query(u"abc ghi");
  TokenizedString text(u"jkl abc def ghi");

  PrefixMatcher pm(query, text);
  pm.Match();
  double expected_score =
      kIsFrontOfTokenCharScore * 2 + kIsWeakHitCharScore * 4;
  EXPECT_NEAR(pm.relevance(), expected_score, kAbsError);
}

TEST_F(PrefixMatcherTest, ExactPrefixMatchOrderVariation) {
  TokenizedString query(u"ghi abc");
  TokenizedString text(u"abc ghi");

  PrefixMatcher pm(query, text);
  pm.Match();
  double expected_score =
      kIsFrontOfTokenCharScore * 2 + kIsWeakHitCharScore * 4;
  EXPECT_NEAR(pm.relevance(), expected_score, kAbsError);
}

TEST_F(PrefixMatcherTest, ExactPrefixMatchOrderVariationNonFirstToken) {
  TokenizedString query(u"ghi abc");
  TokenizedString text(u"jkl abc ghi");

  PrefixMatcher pm(query, text);
  pm.Match();
  double expected_score =
      kIsFrontOfTokenCharScore * 2 + kIsWeakHitCharScore * 4;
  EXPECT_NEAR(pm.relevance(), expected_score, kAbsError);
}

TEST_F(PrefixMatcherTest, ExactPrefixMatchOrderVariationAndDiscrete) {
  TokenizedString query(u"ghi abc");
  TokenizedString text(u"jkl abc def ghi");

  PrefixMatcher pm(query, text);
  pm.Match();
  double expected_score =
      kIsFrontOfTokenCharScore * 2 + kIsWeakHitCharScore * 4;
  EXPECT_NEAR(pm.relevance(), expected_score, kAbsError);
}

TEST_F(PrefixMatcherTest, SentencePrefixMatch) {
  TokenizedString query(u"abcd");
  TokenizedString text(u"a bcd e");

  PrefixMatcher pm(query, text);
  pm.Match();
  double expected_score = kIsPrefixCharScore * 4;
  EXPECT_NEAR(pm.relevance(), expected_score, kAbsError);
}

TEST_F(PrefixMatcherTest, SentencePrefixMatchNotInFront) {
  TokenizedString query(u"abcd");
  TokenizedString text(u"fgh a bcd e");

  PrefixMatcher pm(query, text);
  pm.Match();
  double expected_score = (kIsFrontOfTokenCharScore + kIsWeakHitCharScore) * 2;
  EXPECT_NEAR(pm.relevance(), expected_score, kAbsError);
}

TEST_F(PrefixMatcherTest, CaseSentencePrefixMatchPreferred) {
  TokenizedString query(u"abc def");
  TokenizedString text(u"abcdef abc def");

  PrefixMatcher pm(query, text);
  pm.Match();
  double expected_score = kIsPrefixCharScore * 6;
  EXPECT_NEAR(pm.relevance(), expected_score, kAbsError);
}

TEST_F(PrefixMatcherTest, CaseTokenPrefixMatchPreferred) {
  TokenizedString query(u"abc def");
  TokenizedString text(u"def abc abcdef");

  PrefixMatcher pm(query, text);
  pm.Match();
  double expected_score =
      kIsFrontOfTokenCharScore * 2 + kIsWeakHitCharScore * 4;
  EXPECT_NEAR(pm.relevance(), expected_score, kAbsError);
}

TEST_F(PrefixMatcherTest, LastQueryTokenCanBeMatchedAtMostOnce) {
  TokenizedString query(u"about c");
  TokenizedString text(u"Chrome Canvas");

  PrefixMatcher pm(query, text);
  pm.Match();
  double expected_score = 0;
  EXPECT_NEAR(pm.relevance(), expected_score, kAbsError);
}

}  // namespace ash::string_matching
