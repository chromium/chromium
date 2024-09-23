// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/accessibility/phrase_segmentation/tokenized_sentence.h"

#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "testing/gtest/include/gtest/gtest.h"

namespace {

class PhraseSegmentationTokenizedSentenceTest : public testing::Test {
 protected:
  PhraseSegmentationTokenizedSentenceTest() = default;
};

TEST_F(PhraseSegmentationTokenizedSentenceTest, TokenizeTextCorrectly) {
  const std::u16string text =
      u"The result is \"ice cream\": a smooth, semi-solid foam. It is solid "
      "when cold (below 2 °C or 35 °F).";
  //    01234567890123 4567890123 456789012345678901234567890123456789012345
  //   6789012345678901234567890123456789

  TokenizedSentence tokenized_sentence(text);

  std::vector<std::u16string_view> expected_tokens = {
      u"The", u"result", u"is",    u"\"",   u"ice",  u"cream", u"\"",    u":",
      u"a",   u"smooth", u",",     u"semi", u"-",    u"solid", u"foam",  u".",
      u"It",  u"is",     u"solid", u"when", u"cold", u"(",     u"below", u"2",
      u"°",   u"C",      u"or",    u"35",   u"°",    u"F",     u")",     u"."};
  std::vector<std::pair<int, int>> expected_token_boundaries = {
      std::make_pair(0, 3),   std::make_pair(4, 10),  std::make_pair(11, 13),
      std::make_pair(14, 15), std::make_pair(15, 18), std::make_pair(19, 24),
      std::make_pair(24, 25), std::make_pair(25, 26), std::make_pair(27, 28),
      std::make_pair(29, 35), std::make_pair(35, 36), std::make_pair(37, 41),
      std::make_pair(41, 42), std::make_pair(42, 47), std::make_pair(48, 52),
      std::make_pair(52, 53), std::make_pair(54, 56), std::make_pair(57, 59),
      std::make_pair(60, 65), std::make_pair(66, 70), std::make_pair(71, 75),
      std::make_pair(76, 77), std::make_pair(77, 82), std::make_pair(83, 84),
      std::make_pair(85, 86), std::make_pair(86, 87), std::make_pair(88, 90),
      std::make_pair(91, 93), std::make_pair(94, 95), std::make_pair(95, 96),
      std::make_pair(96, 97), std::make_pair(97, 98)};

  EXPECT_EQ(tokenized_sentence.token_boundaries(), expected_token_boundaries);
  EXPECT_EQ(tokenized_sentence.tokens(), expected_tokens);
}

TEST_F(PhraseSegmentationTokenizedSentenceTest, WordsBetweenIndicesIsCorrect) {
  const std::u16string text =
      u"The result is \"ice cream\": a smooth, semi-solid foam. It is solid "
      "when cold (below 2 °C or 35 °F).";
  //    01234567890123 4567890123 456789012345678901234567890123456789012345
  //   6789012345678901234567890123456789

  TokenizedSentence tokenized_sentence(text);

  EXPECT_EQ(tokenized_sentence.WordsBetween(0, 0), 1);
  EXPECT_EQ(tokenized_sentence.WordsBetween(0, 1), 2);
  EXPECT_EQ(tokenized_sentence.WordsBetween(0, 2), 3);
  EXPECT_EQ(tokenized_sentence.WordsBetween(0, 3), 4);
  EXPECT_EQ(tokenized_sentence.WordsBetween(0, 4), 4);
  EXPECT_EQ(tokenized_sentence.WordsBetween(0, 5), 5);
  EXPECT_EQ(tokenized_sentence.WordsBetween(0, 6), 5);
  EXPECT_EQ(tokenized_sentence.WordsBetween(0, 7), 5);
  EXPECT_EQ(tokenized_sentence.WordsBetween(0, 8), 6);
  EXPECT_EQ(tokenized_sentence.WordsBetween(0, 9), 7);
  EXPECT_EQ(tokenized_sentence.WordsBetween(0, 10), 7);
  EXPECT_EQ(tokenized_sentence.WordsBetween(0, 11), 8);
  EXPECT_EQ(tokenized_sentence.WordsBetween(0, 12), 8);
  EXPECT_EQ(tokenized_sentence.WordsBetween(0, 13), 8);
  EXPECT_EQ(tokenized_sentence.WordsBetween(0, 14), 9);
  EXPECT_EQ(tokenized_sentence.WordsBetween(0, 15), 9);

  EXPECT_EQ(tokenized_sentence.WordsBetween(16, 16), 1);
  EXPECT_EQ(tokenized_sentence.WordsBetween(16, 17), 2);
  EXPECT_EQ(tokenized_sentence.WordsBetween(16, 18), 3);
  EXPECT_EQ(tokenized_sentence.WordsBetween(16, 19), 4);
  EXPECT_EQ(tokenized_sentence.WordsBetween(16, 20), 5);
  EXPECT_EQ(tokenized_sentence.WordsBetween(16, 21), 6);
  EXPECT_EQ(tokenized_sentence.WordsBetween(16, 22), 6);
  EXPECT_EQ(tokenized_sentence.WordsBetween(16, 23), 7);
  EXPECT_EQ(tokenized_sentence.WordsBetween(16, 24), 8);
  EXPECT_EQ(tokenized_sentence.WordsBetween(16, 25), 8);
  EXPECT_EQ(tokenized_sentence.WordsBetween(16, 26), 9);
  EXPECT_EQ(tokenized_sentence.WordsBetween(16, 27), 10);
  EXPECT_EQ(tokenized_sentence.WordsBetween(16, 28), 11);
  EXPECT_EQ(tokenized_sentence.WordsBetween(16, 29), 11);
  EXPECT_EQ(tokenized_sentence.WordsBetween(16, 30), 11);
  EXPECT_EQ(tokenized_sentence.WordsBetween(16, 31), 11);
}

}  // namespace
