// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/string_matching/fuzzy_tokenized_string_match.h"

#include "base/macros.h"
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
  EXPECT_NEAR(match.PartialRatio(base::UTF8ToUTF16("abcde"),
                                 base::UTF8ToUTF16("ababcXXXbcdeY"),
                                 kPartialMatchPenaltyRate, false, 0.0),
              0.6, 0.01);
  EXPECT_NEAR(match.PartialRatio(base::UTF8ToUTF16("big string"),
                                 base::UTF8ToUTF16("strength"),
                                 kPartialMatchPenaltyRate, false, 0.0),
              0.71, 0.01);
  EXPECT_EQ(match.PartialRatio(base::UTF8ToUTF16("abc"), base::UTF8ToUTF16(""),
                               kPartialMatchPenaltyRate, false, 0.0),
            0);
  EXPECT_NEAR(match.PartialRatio(base::UTF8ToUTF16("different in order"),
                                 base::UTF8ToUTF16("order text"),
                                 kPartialMatchPenaltyRate, false, 0.0),
              0.67, 0.01);
}

TEST_F(FuzzyTokenizedStringMatchTest, TokenSetRatioTest) {
  FuzzyTokenizedStringMatch match;
  {
    std::u16string query(base::UTF8ToUTF16("order different in"));
    std::u16string text(base::UTF8ToUTF16("text order"));
    EXPECT_EQ(match.TokenSetRatio(TokenizedString(query), TokenizedString(text),
                                  true, kPartialMatchPenaltyRate, false, 0.0),
              1);
    EXPECT_NEAR(
        match.TokenSetRatio(TokenizedString(query), TokenizedString(text),
                            false, kPartialMatchPenaltyRate, false, 0.0),
        0.67, 0.01);
  }
  {
    std::u16string query(base::UTF8ToUTF16("short text"));
    std::u16string text(
        base::UTF8ToUTF16("this text is really really really long"));
    EXPECT_EQ(match.TokenSetRatio(TokenizedString(query), TokenizedString(text),
                                  true, kPartialMatchPenaltyRate, false, 0.0),
              1);
    EXPECT_NEAR(
        match.TokenSetRatio(TokenizedString(query), TokenizedString(text),
                            false, kPartialMatchPenaltyRate, false, 0.0),
        0.57, 0.01);
  }
  {
    std::u16string query(base::UTF8ToUTF16("common string"));
    std::u16string text(base::UTF8ToUTF16("nothing is shared"));
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
    std::u16string query(
        base::UTF8ToUTF16("token shared token same shared same"));
    std::u16string text(base::UTF8ToUTF16("token shared token text text long"));
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
    std::u16string query(base::UTF8ToUTF16("order different in"));
    std::u16string text(base::UTF8ToUTF16("text order"));
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
    std::u16string query(base::UTF8ToUTF16("short text"));
    std::u16string text(
        base::UTF8ToUTF16("this text is really really really long"));
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
    std::u16string query(base::UTF8ToUTF16("common string"));
    std::u16string text(base::UTF8ToUTF16("nothing is shared"));
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
    std::u16string query(base::UTF8ToUTF16("anonymous"));
    std::u16string text(base::UTF8ToUTF16("famous"));
    EXPECT_NEAR(
        match.WeightedRatio(TokenizedString(query), TokenizedString(text),
                            kPartialMatchPenaltyRate, false, 0.0),
        0.67, 0.01);
  }
  {
    std::u16string query(base::UTF8ToUTF16("Clash.of.clan"));
    std::u16string text(base::UTF8ToUTF16("ClashOfTitan"));
    EXPECT_NEAR(
        match.WeightedRatio(TokenizedString(query), TokenizedString(text),
                            kPartialMatchPenaltyRate, false, 0.0),
        0.81, 0.01);
  }
  {
    std::u16string query(base::UTF8ToUTF16("final fantasy"));
    std::u16string text(base::UTF8ToUTF16("finalfantasy"));
    EXPECT_NEAR(
        match.WeightedRatio(TokenizedString(query), TokenizedString(text),
                            kPartialMatchPenaltyRate, false, 0.0),
        0.96, 0.01);
  }
  {
    std::u16string query(base::UTF8ToUTF16("short text!!!"));
    std::u16string text(
        base::UTF8ToUTF16("this sentence is much much much much much longer "
                          "than the text before"));
    EXPECT_NEAR(
        match.WeightedRatio(TokenizedString(query), TokenizedString(text),
                            kPartialMatchPenaltyRate, false, 0.0),
        0.49, 0.01);
  }
}

TEST_F(FuzzyTokenizedStringMatchTest, PrefixMatcherTest) {
  {
    std::u16string query(base::UTF8ToUTF16("clas"));
    std::u16string text(base::UTF8ToUTF16("Clash of Clan"));
    EXPECT_NEAR(FuzzyTokenizedStringMatch::PrefixMatcher(TokenizedString(query),
                                                         TokenizedString(text)),
                0.94, 0.01);
  }
  {
    std::u16string query(base::UTF8ToUTF16("clash clan"));
    std::u16string text(base::UTF8ToUTF16("Clash of Clan"));
    EXPECT_EQ(FuzzyTokenizedStringMatch::PrefixMatcher(TokenizedString(query),
                                                       TokenizedString(text)),
              0.0);
  }
  {
    std::u16string query(base::UTF8ToUTF16("c o c"));
    std::u16string text(base::UTF8ToUTF16("Clash of Clan"));
    EXPECT_NEAR(FuzzyTokenizedStringMatch::PrefixMatcher(TokenizedString(query),
                                                         TokenizedString(text)),
                0.84, 0.01);
  }
  {
    std::u16string query(base::UTF8ToUTF16("wifi"));
    std::u16string text(base::UTF8ToUTF16("wi-fi"));
    EXPECT_NEAR(FuzzyTokenizedStringMatch::PrefixMatcher(TokenizedString(query),
                                                         TokenizedString(text)),
                0.91, 0.01);
  }
  {
    std::u16string query(base::UTF8ToUTF16("clam"));
    std::u16string text(base::UTF8ToUTF16("Clash of Clan"));
    EXPECT_EQ(FuzzyTokenizedStringMatch::PrefixMatcher(TokenizedString(query),
                                                       TokenizedString(text)),
              0.0);
  }
  {
    std::u16string query(base::UTF8ToUTF16("rp"));
    std::u16string text(base::UTF8ToUTF16("Remove Google Play Store"));
    EXPECT_EQ(FuzzyTokenizedStringMatch::PrefixMatcher(TokenizedString(query),
                                                       TokenizedString(text)),
              0.0);
  }
  {
    std::u16string query(base::UTF8ToUTF16("remove play"));
    std::u16string text(base::UTF8ToUTF16("Remove Google Play Store"));
    EXPECT_EQ(FuzzyTokenizedStringMatch::PrefixMatcher(TokenizedString(query),
                                                       TokenizedString(text)),
              0.0);
  }
  {
    std::u16string query(base::UTF8ToUTF16("google play"));
    std::u16string text(base::UTF8ToUTF16("Remove Google Play Store"));
    EXPECT_NEAR(FuzzyTokenizedStringMatch::PrefixMatcher(TokenizedString(query),
                                                         TokenizedString(text)),
                0.99, 0.01);
  }
}

TEST_F(FuzzyTokenizedStringMatchTest, ParamThresholdTest1) {
  FuzzyTokenizedStringMatch match;
  {
    std::u16string query(base::UTF8ToUTF16("anonymous"));
    std::u16string text(base::UTF8ToUTF16("famous"));
    EXPECT_FALSE(match.IsRelevant(TokenizedString(query), TokenizedString(text),
                                  0.4, false, true, false,
                                  kPartialMatchPenaltyRate, 0.0));
  }
  {
    std::u16string query(base::UTF8ToUTF16("CC"));
    std::u16string text(base::UTF8ToUTF16("Clash Of Clan"));
    EXPECT_FALSE(match.IsRelevant(TokenizedString(query), TokenizedString(text),
                                  0.25, false, true, false,
                                  kPartialMatchPenaltyRate, 0.0));
  }
  {
    std::u16string query(base::UTF8ToUTF16("Clash.of.clan"));
    std::u16string text(base::UTF8ToUTF16("ClashOfTitan"));
    EXPECT_TRUE(match.IsRelevant(TokenizedString(query), TokenizedString(text),
                                 0.4, false, true, false,
                                 kPartialMatchPenaltyRate, 0.0));
  }
}

TEST_F(FuzzyTokenizedStringMatchTest, ParamThresholdTest2) {
  FuzzyTokenizedStringMatch match;
  {
    std::u16string query(base::UTF8ToUTF16("anonymous"));
    std::u16string text(base::UTF8ToUTF16("famous"));
    EXPECT_FALSE(match.IsRelevant(TokenizedString(query), TokenizedString(text),
                                  0.5, false, true, false,
                                  kPartialMatchPenaltyRate, 0.0));
  }
  {
    std::u16string query(base::UTF8ToUTF16("CC"));
    std::u16string text(base::UTF8ToUTF16("Clash Of Clan"));
    EXPECT_FALSE(match.IsRelevant(TokenizedString(query), TokenizedString(text),
                                  0.25, false, true, false,
                                  kPartialMatchPenaltyRate));
  }
  {
    std::u16string query(base::UTF8ToUTF16("Clash.of.clan"));
    std::u16string text(base::UTF8ToUTF16("ClashOfTitan"));
    EXPECT_FALSE(match.IsRelevant(TokenizedString(query), TokenizedString(text),
                                  0.5, false, true, false,
                                  kPartialMatchPenaltyRate, 0.0));
  }
}

TEST_F(FuzzyTokenizedStringMatchTest, OtherParamTest) {
  FuzzyTokenizedStringMatch match;
  std::u16string query(base::UTF8ToUTF16("anonymous"));
  std::u16string text(base::UTF8ToUTF16("famous"));
  EXPECT_FALSE(match.IsRelevant(TokenizedString(query), TokenizedString(text),
                                0.35, false, false, true,
                                kPartialMatchPenaltyRate, 0.0));
  EXPECT_NEAR(match.relevance(), 0.33 / 2, 0.01);
}

TEST_F(FuzzyTokenizedStringMatchTest, ExactTextMatchTest) {
  FuzzyTokenizedStringMatch match;
  std::u16string query(base::UTF8ToUTF16("yat"));
  std::u16string text(base::UTF8ToUTF16("YaT"));
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
