// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/search_utils/fuzzy_tokenized_string_match.h"

#include "ash/public/cpp/app_list/app_list_features.h"
#include "ash/public/cpp/app_list/tokenized_string.h"
#include "base/macros.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/app_list/search/search_utils/sequence_matcher.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace app_list {

class FuzzyTokenizedStringMatchTest : public testing::Test {};

TEST_F(FuzzyTokenizedStringMatchTest, PartialRatioTest) {
  FuzzyTokenizedStringMatch match;
  EXPECT_EQ(match.PartialRatio(base::UTF8ToUTF16("abcde"),
                               base::UTF8ToUTF16("ababcXXXbcdeY")),
            0.8);
  EXPECT_NEAR(match.PartialRatio(base::UTF8ToUTF16("big string"),
                                 base::UTF8ToUTF16("strength")),
              0.71, 0.01);
  EXPECT_EQ(match.PartialRatio(base::UTF8ToUTF16("abc"), base::UTF8ToUTF16("")),
            0);
  EXPECT_NEAR(match.PartialRatio(base::UTF8ToUTF16("different in order"),
                                 base::UTF8ToUTF16("order text")),
              0.67, 0.01);
}

TEST_F(FuzzyTokenizedStringMatchTest, TokenSetRatioTest) {
  FuzzyTokenizedStringMatch match;
  {
    base::string16 query(base::UTF8ToUTF16("order different in"));
    base::string16 text(base::UTF8ToUTF16("text order"));
    EXPECT_EQ(match.TokenSetRatio(ash::TokenizedString(query),
                                  ash::TokenizedString(text), true),
              1);
    EXPECT_NEAR(match.TokenSetRatio(ash::TokenizedString(query),
                                    ash::TokenizedString(text), false),
                0.67, 0.01);
  }
  {
    base::string16 query(base::UTF8ToUTF16("short text"));
    base::string16 text(
        base::UTF8ToUTF16("this text is really really really long"));
    EXPECT_EQ(match.TokenSetRatio(ash::TokenizedString(query),
                                  ash::TokenizedString(text), true),
              1);
    EXPECT_NEAR(match.TokenSetRatio(ash::TokenizedString(query),
                                    ash::TokenizedString(text), false),
                0.57, 0.01);
  }
  {
    base::string16 query(base::UTF8ToUTF16("common string"));
    base::string16 text(base::UTF8ToUTF16("nothing is shared"));
    EXPECT_NEAR(match.TokenSetRatio(ash::TokenizedString(query),
                                    ash::TokenizedString(text), true),
                0.38, 0.01);
    EXPECT_NEAR(match.TokenSetRatio(ash::TokenizedString(query),
                                    ash::TokenizedString(text), false),
                0.33, 0.01);
  }
  {
    base::string16 query(
        base::UTF8ToUTF16("token shared token same shared same"));
    base::string16 text(base::UTF8ToUTF16("token shared token text text long"));
    EXPECT_EQ(match.TokenSetRatio(ash::TokenizedString(query),
                                  ash::TokenizedString(text), true),
              1);
    EXPECT_NEAR(match.TokenSetRatio(ash::TokenizedString(query),
                                    ash::TokenizedString(text), false),
                0.83, 0.01);
  }
}

TEST_F(FuzzyTokenizedStringMatchTest, TokenSortRatioTest) {
  FuzzyTokenizedStringMatch match;
  {
    base::string16 query(base::UTF8ToUTF16("order different in"));
    base::string16 text(base::UTF8ToUTF16("text order"));
    EXPECT_NEAR(match.TokenSortRatio(ash::TokenizedString(query),
                                     ash::TokenizedString(text), true),
                0.67, 0.01);
    EXPECT_NEAR(match.TokenSortRatio(ash::TokenizedString(query),
                                     ash::TokenizedString(text), false),
                0.36, 0.01);
  }
  {
    base::string16 query(base::UTF8ToUTF16("short text"));
    base::string16 text(
        base::UTF8ToUTF16("this text is really really really long"));
    EXPECT_EQ(match.TokenSortRatio(ash::TokenizedString(query),
                                   ash::TokenizedString(text), true),
              0.5);
    EXPECT_NEAR(match.TokenSortRatio(ash::TokenizedString(query),
                                     ash::TokenizedString(text), false),
                0.33, 0.01);
  }
  {
    base::string16 query(base::UTF8ToUTF16("common string"));
    base::string16 text(base::UTF8ToUTF16("nothing is shared"));
    EXPECT_NEAR(match.TokenSortRatio(ash::TokenizedString(query),
                                     ash::TokenizedString(text), true),
                0.38, 0.01);
    EXPECT_NEAR(match.TokenSortRatio(ash::TokenizedString(query),
                                     ash::TokenizedString(text), false),
                0.33, 0.01);
  }
}

TEST_F(FuzzyTokenizedStringMatchTest, WeightedRatio) {
  FuzzyTokenizedStringMatch match;
  {
    base::string16 query(base::UTF8ToUTF16("anonymous"));
    base::string16 text(base::UTF8ToUTF16("famous"));
    EXPECT_NEAR(match.WeightedRatio(ash::TokenizedString(query),
                                    ash::TokenizedString(text)),
                0.67, 0.01);
  }
  {
    base::string16 query(base::UTF8ToUTF16("Clash.of.clan"));
    base::string16 text(base::UTF8ToUTF16("ClashOfTitan"));
    EXPECT_NEAR(match.WeightedRatio(ash::TokenizedString(query),
                                    ash::TokenizedString(text)),
                0.81, 0.01);
  }
  {
    base::string16 query(base::UTF8ToUTF16("final fantasy"));
    base::string16 text(base::UTF8ToUTF16("finalfantasy"));
    EXPECT_NEAR(match.WeightedRatio(ash::TokenizedString(query),
                                    ash::TokenizedString(text)),
                0.96, 0.01);
  }
  {
    base::string16 query(base::UTF8ToUTF16("short text!!!"));
    base::string16 text(
        base::UTF8ToUTF16("this sentence is much much much much much longer "
                          "than the text before"));
    EXPECT_NEAR(match.WeightedRatio(ash::TokenizedString(query),
                                    ash::TokenizedString(text)),
                0.85, 0.01);
  }
}

TEST_F(FuzzyTokenizedStringMatchTest, FirstCharacterMatchTest) {
  {
    base::string16 query(base::UTF8ToUTF16("COC"));
    base::string16 text(base::UTF8ToUTF16("Clash of Clan"));
    EXPECT_EQ(internal::FirstCharacterMatch(ash::TokenizedString(query),
                                            ash::TokenizedString(text)),
              1.0);
  }
  {
    base::string16 query(base::UTF8ToUTF16("CC"));
    base::string16 text(base::UTF8ToUTF16("Clash of Clan"));
    EXPECT_EQ(internal::FirstCharacterMatch(ash::TokenizedString(query),
                                            ash::TokenizedString(text)),
              0.8);
  }
  {
    base::string16 query(base::UTF8ToUTF16("C o C"));
    base::string16 text(base::UTF8ToUTF16("Clash of Clan"));
    EXPECT_EQ(internal::FirstCharacterMatch(ash::TokenizedString(query),
                                            ash::TokenizedString(text)),
              0.0);
  }
}

TEST_F(FuzzyTokenizedStringMatchTest, PrefixMatchTest) {
  {
    base::string16 query(base::UTF8ToUTF16("clas"));
    base::string16 text(base::UTF8ToUTF16("Clash of Clan"));
    EXPECT_EQ(internal::PrefixMatch(ash::TokenizedString(query),
                                    ash::TokenizedString(text)),
              1.0);
  }
  {
    base::string16 query(base::UTF8ToUTF16("clash clan"));
    base::string16 text(base::UTF8ToUTF16("Clash of Clan"));
    EXPECT_EQ(internal::PrefixMatch(ash::TokenizedString(query),
                                    ash::TokenizedString(text)),
              0.9);
  }
  {
    base::string16 query(base::UTF8ToUTF16("c o c"));
    base::string16 text(base::UTF8ToUTF16("Clash of Clan"));
    EXPECT_EQ(internal::PrefixMatch(ash::TokenizedString(query),
                                    ash::TokenizedString(text)),
              1.0);
  }
  {
    base::string16 query(base::UTF8ToUTF16("clam"));
    base::string16 text(base::UTF8ToUTF16("Clash of Clan"));
    EXPECT_EQ(internal::PrefixMatch(ash::TokenizedString(query),
                                    ash::TokenizedString(text)),
              0.0);
  }
}

TEST_F(FuzzyTokenizedStringMatchTest, ParamThresholdTest1) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeaturesAndParameters(
      {{app_list_features::kEnableFuzzyAppSearch,
        {{"relevance_threshold", "0.4"}}}},
      {});
  FuzzyTokenizedStringMatch match;
  {
    base::string16 query(base::UTF8ToUTF16("anonymous"));
    base::string16 text(base::UTF8ToUTF16("famous"));
    EXPECT_FALSE(match.IsRelevant(ash::TokenizedString(query),
                                  ash::TokenizedString(text)));
  }
  {
    base::string16 query(base::UTF8ToUTF16("CC"));
    base::string16 text(base::UTF8ToUTF16("Clash Of Clan"));
    EXPECT_TRUE(match.IsRelevant(ash::TokenizedString(query),
                                 ash::TokenizedString(text)));
  }
  {
    base::string16 query(base::UTF8ToUTF16("Clash.of.clan"));
    base::string16 text(base::UTF8ToUTF16("ClashOfTitan"));
    EXPECT_TRUE(match.IsRelevant(ash::TokenizedString(query),
                                 ash::TokenizedString(text)));
  }
}

TEST_F(FuzzyTokenizedStringMatchTest, ParamThresholdTest2) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeaturesAndParameters(
      {{app_list_features::kEnableFuzzyAppSearch,
        {{"relevance_threshold", "0.5"}}}},
      {});
  FuzzyTokenizedStringMatch match;
  {
    base::string16 query(base::UTF8ToUTF16("anonymous"));
    base::string16 text(base::UTF8ToUTF16("famous"));
    EXPECT_FALSE(match.IsRelevant(ash::TokenizedString(query),
                                  ash::TokenizedString(text)));
  }
  {
    base::string16 query(base::UTF8ToUTF16("CC"));
    base::string16 text(base::UTF8ToUTF16("Clash Of Clan"));
    EXPECT_TRUE(match.IsRelevant(ash::TokenizedString(query),
                                 ash::TokenizedString(text)));
  }
  {
    base::string16 query(base::UTF8ToUTF16("Clash.of.clan"));
    base::string16 text(base::UTF8ToUTF16("ClashOfTitan"));
    EXPECT_FALSE(match.IsRelevant(ash::TokenizedString(query),
                                  ash::TokenizedString(text)));
  }
}

TEST_F(FuzzyTokenizedStringMatchTest, OtherParamTest) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeaturesAndParameters(
      {{app_list_features::kEnableFuzzyAppSearch,
        {{"relevance_threshold", "0.35"},
         {"use_weighted_ratio", "false"},
         {"use_edit_distance", "true"}}}},
      {});
  FuzzyTokenizedStringMatch match;
  base::string16 query(base::UTF8ToUTF16("anonymous"));
  base::string16 text(base::UTF8ToUTF16("famous"));
  EXPECT_FALSE(match.IsRelevant(ash::TokenizedString(query),
                                ash::TokenizedString(text)));
  EXPECT_NEAR(match.relevance(), 0.33, 0.01);
}

}  // namespace app_list
