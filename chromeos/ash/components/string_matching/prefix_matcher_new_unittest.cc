// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/string_matching/prefix_matcher_new.h"

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

class PrefixMatcherNewTest : public testing::Test {};

// Note on expected score calculations:
//
// When a query successfully matches to a text, each letter of the query
// contributes some amount towards a final total. The expected score in
// each test is then the sum over all of the contributions of the individual
// query letters. This is described in more detail in prefix_matcher.cc.
//
// When a query does not successfully match to a text, the overall expected
// score is `kNoMatchScore`.

TEST_F(PrefixMatcherNewTest, ExactMatch) {
  TokenizedString query(u"abc def");
  TokenizedString text(u"abc def");

  PrefixMatcherNew pm(query, text);
  pm.Match();
  double expected_score = kIsPrefixCharScore * 6;
  EXPECT_NEAR(pm.relevance(), expected_score, kAbsError);
}

TEST_F(PrefixMatcherNewTest, ExactPrefixMatch) {
  TokenizedString query(u"abc def");
  TokenizedString text(u"abc defgh ijklm");

  PrefixMatcherNew pm(query, text);
  pm.Match();
  double expected_score = kIsPrefixCharScore * 6;
  EXPECT_NEAR(pm.relevance(), expected_score, kAbsError);
}

TEST_F(PrefixMatcherNewTest, ExactPrefixMatchFirstToken) {
  TokenizedString query(u"ab");
  TokenizedString text(u"abc def");

  PrefixMatcherNew pm(query, text);
  pm.Match();
  double expected_score = kIsPrefixCharScore * 2;
  EXPECT_NEAR(pm.relevance(), expected_score, kAbsError);
}

TEST_F(PrefixMatcherNewTest, ExactPrefixMatchNonFirstToken) {
  TokenizedString query(u"de");
  TokenizedString text(u"abc def");

  PrefixMatcherNew pm(query, text);
  pm.Match();
  double expected_score = kIsFrontOfTokenCharScore + kIsWeakHitCharScore;
  EXPECT_NEAR(pm.relevance(), expected_score, kAbsError);
}

TEST_F(PrefixMatcherNewTest, AcronymMatchFirstTokenMatchConsideredNonMatch) {
  TokenizedString query(u"abc");
  TokenizedString text(u"axx bxx cxx dxx exx");

  PrefixMatcherNew pm(query, text);
  pm.Match();
  double expected_score = kNoMatchScore;
  EXPECT_NEAR(pm.relevance(), expected_score, kAbsError);
}

TEST_F(PrefixMatcherNewTest, AcronymMatchNonFirstTokenMatchConsideredNonMatch) {
  TokenizedString query(u"bcd");
  TokenizedString text(u"axx bxx cxx dxx exx");

  PrefixMatcherNew pm(query, text);
  pm.Match();
  double expected_score = kNoMatchScore;
  EXPECT_NEAR(pm.relevance(), expected_score, kAbsError);
}

TEST_F(PrefixMatcherNewTest, AcronymMatchNonConsecutiveConsideredNonMatch) {
  TokenizedString query(u"acd");
  TokenizedString text(u"axx bxx cxx dxx exx");

  PrefixMatcherNew pm(query, text);
  pm.Match();
  double expected_score = kNoMatchScore;
  EXPECT_NEAR(pm.relevance(), expected_score, kAbsError);
}

TEST_F(PrefixMatcherNewTest, MixedAcronymAndPrefixMatchingConsideredNonMatch) {
  TokenizedString query(u"adefg");
  TokenizedString text(u"abc def ghi");

  PrefixMatcherNew pm(query, text);
  pm.Match();

  double expected_score = kNoMatchScore;
  EXPECT_NEAR(pm.relevance(), expected_score, kAbsError);
}

TEST_F(PrefixMatcherNewTest, FinalPartialTokenConsideredMatch) {
  TokenizedString query(u"abc de");
  TokenizedString text(u"abc def");

  PrefixMatcherNew pm(query, text);
  pm.Match();
  double expected_score = kIsPrefixCharScore * 5;
  EXPECT_NEAR(pm.relevance(), expected_score, kAbsError);
}

TEST_F(PrefixMatcherNewTest, NonFinalPartialTokenConsideredNonMatch) {
  TokenizedString query(u"abce");
  TokenizedString text(u"a bcd e");

  PrefixMatcherNew pm(query, text);
  pm.Match();
  double expected_score = kNoMatchScore;
  EXPECT_NEAR(pm.relevance(), expected_score, kAbsError);
}

TEST_F(PrefixMatcherNewTest, ExactPrefixMatchDiscrete) {
  TokenizedString query(u"abc ghi");
  TokenizedString text(u"abc def ghi");

  PrefixMatcherNew pm(query, text);
  pm.Match();
  double expected_score = kIsPrefixCharScore * 3 + kIsFrontOfTokenCharScore +
                          kIsWeakHitCharScore * 2;
  EXPECT_NEAR(pm.relevance(), expected_score, kAbsError);
}

TEST_F(PrefixMatcherNewTest, ExactPrefixMatchDiscreteNonFirstToken) {
  TokenizedString query(u"abc ghi");
  TokenizedString text(u"jkl abc def ghi");

  PrefixMatcherNew pm(query, text);
  pm.Match();
  double expected_score =
      kIsFrontOfTokenCharScore * 2 + kIsWeakHitCharScore * 4;
  EXPECT_NEAR(pm.relevance(), expected_score, kAbsError);
}

TEST_F(PrefixMatcherNewTest, ExactPrefixMatchOrderVariation) {
  TokenizedString query(u"ghi abc");
  TokenizedString text(u"abc ghi");

  PrefixMatcherNew pm(query, text);
  pm.Match();
  double expected_score =
      kIsFrontOfTokenCharScore * 2 + kIsWeakHitCharScore * 4;
  EXPECT_NEAR(pm.relevance(), expected_score, kAbsError);
}

TEST_F(PrefixMatcherNewTest, ExactPrefixMatchOrderVariationNonFirstToken) {
  TokenizedString query(u"ghi abc");
  TokenizedString text(u"jkl abc ghi");

  PrefixMatcherNew pm(query, text);
  pm.Match();
  double expected_score =
      kIsFrontOfTokenCharScore * 2 + kIsWeakHitCharScore * 4;
  EXPECT_NEAR(pm.relevance(), expected_score, kAbsError);
}

TEST_F(PrefixMatcherNewTest, ExactPrefixMatchOrderVariationAndDiscrete) {
  TokenizedString query(u"ghi abc");
  TokenizedString text(u"jkl abc def ghi");

  PrefixMatcherNew pm(query, text);
  pm.Match();
  double expected_score =
      kIsFrontOfTokenCharScore * 2 + kIsWeakHitCharScore * 4;
  EXPECT_NEAR(pm.relevance(), expected_score, kAbsError);
}

}  // namespace ash::string_matching
