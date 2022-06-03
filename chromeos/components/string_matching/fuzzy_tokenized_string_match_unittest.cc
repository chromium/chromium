// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/string_matching/fuzzy_tokenized_string_match.h"

#include "base/strings/utf_string_conversions.h"
#include "chromeos/components/string_matching/sequence_matcher.h"
#include "chromeos/components/string_matching/tokenized_string.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace string_matching {

namespace {
constexpr double kPartialMatchPenaltyRate = 0.9;

}  // namespace

class FuzzyTokenizedStringMatchTest : public testing::Test {};

// TODO(crbug.com/1018613): update the tests once params are consolidated.
TEST_F(FuzzyTokenizedStringMatchTest, PartialRatioTest) {
  FuzzyTokenizedStringMatch match;
  EXPECT_NEAR(match.PartialRatio(u"abcde", u"ababcXXXbcdeY",
                                 kPartialMatchPenaltyRate, false, 0.0),
              0.6, 0.01);
  EXPECT_NEAR(match.PartialRatio(u"big string", u"strength",
                                 kPartialMatchPenaltyRate, false, 0.0),
              0.71, 0.01);
  EXPECT_EQ(
      match.PartialRatio(u"abc", u"", kPartialMatchPenaltyRate, false, 0.0), 0);
  EXPECT_NEAR(match.PartialRatio(u"different in order", u"order text",
                                 kPartialMatchPenaltyRate, false, 0.0),
              0.67, 0.01);
}

TEST_F(FuzzyTokenizedStringMatchTest, TokenSetRatioTest) {
  FuzzyTokenizedStringMatch match;
  {
    std::u16string query(u"order different in");
    std::u16string text(u"text order");
    EXPECT_EQ(match.TokenSetRatio(TokenizedString(query), TokenizedString(text),
                                  true, kPartialMatchPenaltyRate, false, 0.0),
              1);
    EXPECT_NEAR(
        match.TokenSetRatio(TokenizedString(query), TokenizedString(text),
                            false, kPartialMatchPenaltyRate, false, 0.0),
        0.67, 0.01);
  }
  {
    std::u16string query(u"short text");
    std::u16string text(u"this text is really really really long");
    EXPECT_EQ(match.TokenSetRatio(TokenizedString(query), TokenizedString(text),
                                  true, kPartialMatchPenaltyRate, false, 0.0),
              1);
    EXPECT_NEAR(
        match.TokenSetRatio(TokenizedString(query), TokenizedString(text),
                            false, kPartialMatchPenaltyRate, false, 0.0),
        0.57, 0.01);
  }
  {
    std::u16string query(u"common string");
    std::u16string text(u"nothing is shared");
    EXPECT_NEAR(
        match.TokenSetRatio(TokenizedString(query), TokenizedString(text), true,
                            kPartialMatchPenaltyRate, false, 0.0),
        0.38, 0.01);
    EXPECT_NEAR(
        match.TokenSetRatio(TokenizedString(query), TokenizedString(text),
                            false, kPartialMatchPenaltyRate, false, 0.0),
        0.33, 0.01);
  }
  {
    std::u16string query(u"token shared token same shared same");
    std::u16string text(u"token shared token text text long");
    EXPECT_EQ(match.TokenSetRatio(TokenizedString(query), TokenizedString(text),
                                  true, kPartialMatchPenaltyRate, false, 0.0),
              1);
    EXPECT_NEAR(
        match.TokenSetRatio(TokenizedString(query), TokenizedString(text),
                            false, kPartialMatchPenaltyRate, false, 0.0),
        0.83, 0.01);
  }
}

TEST_F(FuzzyTokenizedStringMatchTest, TokenSortRatioTest) {
  FuzzyTokenizedStringMatch match;
  {
    std::u16string query(u"order different in");
    std::u16string text(u"text order");
    EXPECT_NEAR(
        match.TokenSortRatio(TokenizedString(query), TokenizedString(text),
                             true, kPartialMatchPenaltyRate, false, 0.0),
        0.67, 0.01);
    EXPECT_NEAR(
        match.TokenSortRatio(TokenizedString(query), TokenizedString(text),
                             false, kPartialMatchPenaltyRate, false, 0.0),
        0.36, 0.01);
  }
  {
    std::u16string query(u"short text");
    std::u16string text(u"this text is really really really long");
    EXPECT_EQ(
        match.TokenSortRatio(TokenizedString(query), TokenizedString(text),
                             true, kPartialMatchPenaltyRate, false, 0.0),
        0.5 * std::pow(0.9, 1));
    EXPECT_NEAR(
        match.TokenSortRatio(TokenizedString(query), TokenizedString(text),
                             false, kPartialMatchPenaltyRate, false, 0.0),
        0.33, 0.01);
  }
  {
    std::u16string query(u"common string");
    std::u16string text(u"nothing is shared");
    EXPECT_NEAR(
        match.TokenSortRatio(TokenizedString(query), TokenizedString(text),
                             true, kPartialMatchPenaltyRate, false, 0.0),
        0.38, 0.01);
    EXPECT_NEAR(
        match.TokenSortRatio(TokenizedString(query), TokenizedString(text),
                             false, kPartialMatchPenaltyRate, false, 0.0),
        0.33, 0.01);
  }
}

TEST_F(FuzzyTokenizedStringMatchTest, WeightedRatio) {
  FuzzyTokenizedStringMatch match;
  {
    std::u16string query(u"anonymous");
    std::u16string text(u"famous");
    EXPECT_NEAR(
        match.WeightedRatio(TokenizedString(query), TokenizedString(text),
                            kPartialMatchPenaltyRate, false, 0.0),
        0.67, 0.01);
  }
  {
    std::u16string query(u"Clash.of.clan");
    std::u16string text(u"ClashOfTitan");
    EXPECT_NEAR(
        match.WeightedRatio(TokenizedString(query), TokenizedString(text),
                            kPartialMatchPenaltyRate, false, 0.0),
        0.81, 0.01);
  }
  {
    std::u16string query(u"final fantasy");
    std::u16string text(u"finalfantasy");
    EXPECT_NEAR(
        match.WeightedRatio(TokenizedString(query), TokenizedString(text),
                            kPartialMatchPenaltyRate, false, 0.0),
        0.96, 0.01);
  }
  {
    std::u16string query(u"short text!!!");
    std::u16string text(
        u"this sentence is much much much much much longer "
        u"than the text before");
    EXPECT_NEAR(
        match.WeightedRatio(TokenizedString(query), TokenizedString(text),
                            kPartialMatchPenaltyRate, false, 0.0),
        0.49, 0.01);
  }
}

TEST_F(FuzzyTokenizedStringMatchTest, PrefixMatcherTest) {
  {
    std::u16string query(u"clas");
    std::u16string text(u"Clash of Clan");
    EXPECT_NEAR(FuzzyTokenizedStringMatch::PrefixMatcher(TokenizedString(query),
                                                         TokenizedString(text)),
                0.94, 0.01);
  }
  {
    std::u16string query(u"clash clan");
    std::u16string text(u"Clash of Clan");
    EXPECT_EQ(FuzzyTokenizedStringMatch::PrefixMatcher(TokenizedString(query),
                                                       TokenizedString(text)),
              0.0);
  }
  {
    std::u16string query(u"c o c");
    std::u16string text(u"Clash of Clan");
    EXPECT_NEAR(FuzzyTokenizedStringMatch::PrefixMatcher(TokenizedString(query),
                                                         TokenizedString(text)),
                0.84, 0.01);
  }
  {
    std::u16string query(u"wifi");
    std::u16string text(u"wi-fi");
    EXPECT_NEAR(FuzzyTokenizedStringMatch::PrefixMatcher(TokenizedString(query),
                                                         TokenizedString(text)),
                0.91, 0.01);
  }
  {
    std::u16string query(u"clam");
    std::u16string text(u"Clash of Clan");
    EXPECT_EQ(FuzzyTokenizedStringMatch::PrefixMatcher(TokenizedString(query),
                                                       TokenizedString(text)),
              0.0);
  }
  {
    std::u16string query(u"rp");
    std::u16string text(u"Remove Google Play Store");
    EXPECT_EQ(FuzzyTokenizedStringMatch::PrefixMatcher(TokenizedString(query),
                                                       TokenizedString(text)),
              0.0);
  }
  {
    std::u16string query(u"remove play");
    std::u16string text(u"Remove Google Play Store");
    EXPECT_EQ(FuzzyTokenizedStringMatch::PrefixMatcher(TokenizedString(query),
                                                       TokenizedString(text)),
              0.0);
  }
  {
    std::u16string query(u"google play");
    std::u16string text(u"Remove Google Play Store");
    EXPECT_NEAR(FuzzyTokenizedStringMatch::PrefixMatcher(TokenizedString(query),
                                                         TokenizedString(text)),
                0.99, 0.01);
  }
}

TEST_F(FuzzyTokenizedStringMatchTest, ParamThresholdTest1) {
  FuzzyTokenizedStringMatch match;
  {
    std::u16string query(u"anonymous");
    std::u16string text(u"famous");
    EXPECT_FALSE(match.IsRelevant(TokenizedString(query), TokenizedString(text),
                                  0.4, false, true, false,
                                  kPartialMatchPenaltyRate, 0.0));
  }
  {
    std::u16string query(u"CC");
    std::u16string text(u"Clash Of Clan");
    EXPECT_FALSE(match.IsRelevant(TokenizedString(query), TokenizedString(text),
                                  0.25, false, true, false,
                                  kPartialMatchPenaltyRate, 0.0));
  }
  {
    std::u16string query(u"Clash.of.clan");
    std::u16string text(u"ClashOfTitan");
    EXPECT_TRUE(match.IsRelevant(TokenizedString(query), TokenizedString(text),
                                 0.4, false, true, false,
                                 kPartialMatchPenaltyRate, 0.0));
  }
}

TEST_F(FuzzyTokenizedStringMatchTest, ParamThresholdTest2) {
  FuzzyTokenizedStringMatch match;
  {
    std::u16string query(u"anonymous");
    std::u16string text(u"famous");
    EXPECT_FALSE(match.IsRelevant(TokenizedString(query), TokenizedString(text),
                                  0.5, false, true, false,
                                  kPartialMatchPenaltyRate, 0.0));
  }
  {
    std::u16string query(u"CC");
    std::u16string text(u"Clash Of Clan");
    EXPECT_FALSE(match.IsRelevant(TokenizedString(query), TokenizedString(text),
                                  0.25, false, true, false,
                                  kPartialMatchPenaltyRate));
  }
  {
    std::u16string query(u"Clash.of.clan");
    std::u16string text(u"ClashOfTitan");
    EXPECT_FALSE(match.IsRelevant(TokenizedString(query), TokenizedString(text),
                                  0.5, false, true, false,
                                  kPartialMatchPenaltyRate, 0.0));
  }
}

TEST_F(FuzzyTokenizedStringMatchTest, OtherParamTest) {
  FuzzyTokenizedStringMatch match;
  std::u16string query(u"anonymous");
  std::u16string text(u"famous");
  EXPECT_FALSE(match.IsRelevant(TokenizedString(query), TokenizedString(text),
                                0.35, false, false, true,
                                kPartialMatchPenaltyRate, 0.0));
  EXPECT_NEAR(match.relevance(), 0.33 / 2, 0.01);
}

TEST_F(FuzzyTokenizedStringMatchTest, ExactTextMatchTest) {
  FuzzyTokenizedStringMatch match;
  std::u16string query(u"yat");
  std::u16string text(u"YaT");
  EXPECT_TRUE(match.IsRelevant(TokenizedString(query), TokenizedString(text),
                               0.35, false, false, true,
                               kPartialMatchPenaltyRate, 0.0));
  EXPECT_DOUBLE_EQ(match.relevance(), 1.0);
  EXPECT_EQ(match.hits().size(), 1u);
  EXPECT_EQ(match.hits()[0].start(), 0u);
  EXPECT_EQ(match.hits()[0].end(), 3u);
}

}  // namespace string_matching
}  // namespace chromeos
