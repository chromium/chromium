// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/string_matching/sequence_matcher.h"

#include "base/strings/utf_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace string_matching {

namespace {
constexpr bool kDefaultUseEditDistance = false;

using Match = SequenceMatcher::Match;
bool MatchEqual(const Match& match1, const Match& match2) {
  return match1.pos_first_string == match2.pos_first_string &&
         match1.pos_second_string == match2.pos_second_string &&
         match1.length == match2.length;
}
}  // namespace

class SequenceMatcherTest : public testing::Test {};

TEST_F(SequenceMatcherTest, TestEditDistance) {
  // Transposition
  ASSERT_EQ(SequenceMatcher(u"abcd", u"abdc", kDefaultUseEditDistance, 0.0)
                .EditDistance(),
            1);

  // Deletion
  ASSERT_EQ(SequenceMatcher(u"abcde", u"abcd", kDefaultUseEditDistance, 0.0)
                .EditDistance(),
            1);
  ASSERT_EQ(
      SequenceMatcher(u"12", u"", kDefaultUseEditDistance, 0.0).EditDistance(),
      2);

  // Insertion
  ASSERT_EQ(SequenceMatcher(u"abc", u"abxbc", kDefaultUseEditDistance, 0.0)
                .EditDistance(),
            2);
  ASSERT_EQ(SequenceMatcher(u"", u"abxbc", kDefaultUseEditDistance, 0.0)
                .EditDistance(),
            5);

  // Substitution
  ASSERT_EQ(SequenceMatcher(u"book", u"back", kDefaultUseEditDistance, 0.0)
                .EditDistance(),
            2);

  // Combination
  ASSERT_EQ(SequenceMatcher(u"caclulation", u"calculator",
                            kDefaultUseEditDistance, 0.0)
                .EditDistance(),
            3);
  ASSERT_EQ(
      SequenceMatcher(u"sunday", u"saturday", kDefaultUseEditDistance, 0.0)
          .EditDistance(),
      3);
}

TEST_F(SequenceMatcherTest, TestFindLongestMatch) {
  SequenceMatcher sequence_match(u"miscellanious", u"miscellaneous",
                                 kDefaultUseEditDistance, 0.0);
  ASSERT_TRUE(MatchEqual(sequence_match.FindLongestMatch(0, 13, 0, 13),
                         Match(0, 0, 9)));
  ASSERT_TRUE(MatchEqual(sequence_match.FindLongestMatch(7, 13, 7, 13),
                         Match(10, 10, 3)));

  ASSERT_TRUE(
      MatchEqual(SequenceMatcher(u"", u"abcd", kDefaultUseEditDistance, 0.0)
                     .FindLongestMatch(0, 0, 0, 4),
                 Match(0, 0, 0)));
  ASSERT_TRUE(MatchEqual(SequenceMatcher(u"abababbababa", u"ababbaba",
                                         kDefaultUseEditDistance, 0.0)
                             .FindLongestMatch(0, 12, 0, 8),
                         Match(2, 0, 8)));
  ASSERT_TRUE(MatchEqual(
      SequenceMatcher(u"aaaaaa", u"aaaaa", kDefaultUseEditDistance, 0.0)
          .FindLongestMatch(0, 6, 0, 5),
      Match(0, 0, 5)));
}

TEST_F(SequenceMatcherTest, TestGetMatchingBlocks) {
  SequenceMatcher sequence_match(u"This is a demo sentence!!!",
                                 u"This demo sentence is good!!!",
                                 kDefaultUseEditDistance, 0.0);
  const std::vector<Match> true_matches = {Match(0, 0, 4), Match(9, 4, 14),
                                           Match(23, 26, 3), Match(26, 29, 0)};
  const std::vector<Match> matches = sequence_match.GetMatchingBlocks();
  ASSERT_EQ(matches.size(), 4ul);
  for (int i = 0; i < 4; i++) {
    ASSERT_TRUE(MatchEqual(matches[i], true_matches[i]));
  }
}

TEST_F(SequenceMatcherTest, TestSequenceMatcherRatio) {
  ASSERT_EQ(
      SequenceMatcher(u"abcd", u"adbc", kDefaultUseEditDistance, 0.0).Ratio(),
      0.75);
  ASSERT_EQ(SequenceMatcher(u"white cats", u"cats white",
                            kDefaultUseEditDistance, 0.0)
                .Ratio(),
            0.5);
}

TEST_F(SequenceMatcherTest, TestSequenceMatcherRatioWithoutPenalty) {
  // Two matching blocks, total matching blocks length is 4.
  EXPECT_NEAR(
      SequenceMatcher(u"word", u"hello world", kDefaultUseEditDistance, 0.0)
          .Ratio(),
      0.533, 0.001);

  // One matching block, length is 4.
  EXPECT_NEAR(
      SequenceMatcher(u"worl", u"hello world", kDefaultUseEditDistance, 0.0)
          .Ratio(),
      0.533, 0.001);

  // No matching block at all.
  EXPECT_NEAR(
      SequenceMatcher(u"abcd", u"xyz", kDefaultUseEditDistance, 0.0).Ratio(),
      0.0, 0.001);
}

TEST_F(SequenceMatcherTest, TestSequenceMatcherRatioWithPenalty) {
  // Two matching blocks, total matching blocks length is 4.
  EXPECT_NEAR(
      SequenceMatcher(u"word", u"hello world", kDefaultUseEditDistance, 0.1)
          .Ratio(),
      0.4825, 0.0001);
  // One matching block, length is 4.
  EXPECT_NEAR(
      SequenceMatcher(u"worl", u"hello world", kDefaultUseEditDistance, 0.1)
          .Ratio(),
      0.533, 0.001);

  // No matching block at all.
  EXPECT_NEAR(
      SequenceMatcher(u"abcd", u"xyz", kDefaultUseEditDistance, 0.1).Ratio(),
      0.0, 0.001);
}

TEST_F(SequenceMatcherTest, TestEditDistanceRatio) {
  ASSERT_EQ(SequenceMatcher(u"abcd", u"adbc", true, 0.0).Ratio(), 0.5);
  EXPECT_NEAR(SequenceMatcher(u"white cats", u"cats white", true, 0.0).Ratio(),
              0.2, 0.01);

  // Totally different
  EXPECT_NEAR(SequenceMatcher(u"dog", u"elphant", true, 0.0).Ratio(), 0.0,
              0.01);
}

TEST_F(SequenceMatcherTest, TestEmptyStrings) {
  ASSERT_EQ(SequenceMatcher(u"", u"",
                            /*use_edit_distance=*/true, 0.0)
                .Ratio(),
            0.0);

  ASSERT_EQ(SequenceMatcher(u"", u"abcd",
                            /*use_edit_distance=*/true, 0.0)
                .Ratio(),
            0.0);

  ASSERT_EQ(SequenceMatcher(u"", u"",
                            /*use_edit_distance=*/false, 0.0)
                .Ratio(),
            0.0);

  ASSERT_EQ(SequenceMatcher(u"", u"abcd",
                            /*use_edit_distance=*/false, 0.0)
                .Ratio(),
            0.0);
}

}  // namespace string_matching
}  // namespace chromeos
