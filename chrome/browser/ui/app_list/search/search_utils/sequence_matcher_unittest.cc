// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/search_utils/sequence_matcher.h"

#include "base/macros.h"
#include "base/strings/utf_string_conversions.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace app_list {

namespace {
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
  ASSERT_EQ(
      SequenceMatcher(base::UTF8ToUTF16("abcd"), base::UTF8ToUTF16("abdc"))
          .EditDistance(),
      1);

  // Deletion
  ASSERT_EQ(
      SequenceMatcher(base::UTF8ToUTF16("abcde"), base::UTF8ToUTF16("abcd"))
          .EditDistance(),
      1);
  ASSERT_EQ(SequenceMatcher(base::UTF8ToUTF16("12"), base::UTF8ToUTF16(""))
                .EditDistance(),
            2);

  // Insertion
  ASSERT_EQ(
      SequenceMatcher(base::UTF8ToUTF16("abc"), base::UTF8ToUTF16("abxbc"))
          .EditDistance(),
      2);
  ASSERT_EQ(SequenceMatcher(base::UTF8ToUTF16(""), base::UTF8ToUTF16("abxbc"))
                .EditDistance(),
            5);

  // Substitution
  ASSERT_EQ(
      SequenceMatcher(base::UTF8ToUTF16("book"), base::UTF8ToUTF16("back"))
          .EditDistance(),
      2);

  // Combination
  ASSERT_EQ(SequenceMatcher(base::UTF8ToUTF16("caclulation"),
                            base::UTF8ToUTF16("calculator"))
                .EditDistance(),
            3);
  ASSERT_EQ(SequenceMatcher(base::UTF8ToUTF16("sunday"),
                            base::UTF8ToUTF16("saturday"))
                .EditDistance(),
            3);
}

TEST_F(SequenceMatcherTest, TestFindLongestMatch) {
  SequenceMatcher sequence_match(base::UTF8ToUTF16("miscellanious"),
                                 base::UTF8ToUTF16("miscellaneous"));
  ASSERT_TRUE(MatchEqual(sequence_match.FindLongestMatch(0, 13, 0, 13),
                         Match(0, 0, 9)));
  ASSERT_TRUE(MatchEqual(sequence_match.FindLongestMatch(7, 13, 7, 13),
                         Match(10, 10, 3)));

  ASSERT_TRUE(MatchEqual(
      SequenceMatcher(base::UTF8ToUTF16(""), base::UTF8ToUTF16("abcd"))
          .FindLongestMatch(0, 0, 0, 4),
      Match(0, 0, 0)));
  ASSERT_TRUE(MatchEqual(SequenceMatcher(base::UTF8ToUTF16("abababbababa"),
                                         base::UTF8ToUTF16("ababbaba"))
                             .FindLongestMatch(0, 12, 0, 8),
                         Match(2, 0, 8)));
  ASSERT_TRUE(MatchEqual(
      SequenceMatcher(base::UTF8ToUTF16("aaaaaa"), base::UTF8ToUTF16("aaaaa"))
          .FindLongestMatch(0, 6, 0, 5),
      Match(0, 0, 5)));
}

TEST_F(SequenceMatcherTest, TestGetMatchingBlocks) {
  SequenceMatcher sequence_match(
      base::UTF8ToUTF16("This is a demo sentence!!!"),
      base::UTF8ToUTF16("This demo sentence is good!!!"));
  const std::vector<Match> true_matches = {Match(0, 0, 4), Match(9, 4, 14),
                                           Match(23, 26, 3), Match(26, 29, 0)};
  const std::vector<Match> matches = sequence_match.GetMatchingBlocks();
  ASSERT_EQ(matches.size(), 4ul);
  for (int i = 0; i < 4; i++) {
    ASSERT_TRUE(MatchEqual(matches[i], true_matches[i]));
  }
}

}  // namespace app_list
