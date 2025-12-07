// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/accessibility/phrase_segmentation/tokenizer.h"

#include <string>
#include <utility>
#include <vector>

#include "testing/gtest/include/gtest/gtest.h"

namespace {

class PhraseSegmentationTokenizerTest : public testing::Test {
 protected:
  PhraseSegmentationTokenizerTest() = default;
};

TEST_F(PhraseSegmentationTokenizerTest, TokenizeSentence) {
  const std::u16string input_string =
      u"They were described by the neighbors as a quiet middle-aged couple.";
  //    0123456789012345678901234567890123456789012345678901234567890123456

  std::vector<std::pair<int, int>> tokens{
      std::make_pair(0, 4),   std::make_pair(5, 9),   std::make_pair(10, 19),
      std::make_pair(20, 22), std::make_pair(23, 26), std::make_pair(27, 36),
      std::make_pair(37, 39), std::make_pair(40, 41), std::make_pair(42, 47),
      std::make_pair(48, 54), std::make_pair(54, 55), std::make_pair(55, 59),
      std::make_pair(60, 66), std::make_pair(66, 67)};
  Tokenizer tokenizer;
  EXPECT_EQ(tokenizer.Tokenize(input_string), tokens);
}

TEST_F(PhraseSegmentationTokenizerTest, TokenizeEmpty) {
  const std::u16string input_string = u"";
  std::vector<std::pair<int, int>> tokens{};
  Tokenizer tokenizer;
  EXPECT_EQ(tokenizer.Tokenize(input_string), tokens);
}

TEST_F(PhraseSegmentationTokenizerTest, TokenizeIrregularSpace) {
  const std::u16string input_string =
      u"They  were   described by the neighbors as a quiet middle-aged couple.";
  //    0123456789012345678901234567890123456789012345678901234567890123456789

  std::vector<std::pair<int, int>> tokens{
      std::make_pair(0, 4),   std::make_pair(6, 10),  std::make_pair(13, 22),
      std::make_pair(23, 25), std::make_pair(26, 29), std::make_pair(30, 39),
      std::make_pair(40, 42), std::make_pair(43, 44), std::make_pair(45, 50),
      std::make_pair(51, 57), std::make_pair(57, 58), std::make_pair(58, 62),
      std::make_pair(63, 69), std::make_pair(69, 70)};

  Tokenizer tokenizer;
  EXPECT_EQ(tokenizer.Tokenize(input_string), tokens);
}

TEST_F(PhraseSegmentationTokenizerTest, TokenizePunctuations) {
  const std::u16string input_string =
      u"They were described (by the neighbors) as a quiet, middle-aged couple.";
  //    0123456789012345678901234567890123456789012345678901234567890123456789
  std::vector<std::pair<int, int>> tokens{
      std::make_pair(0, 4),   std::make_pair(5, 9),   std::make_pair(10, 19),
      std::make_pair(20, 21), std::make_pair(21, 23), std::make_pair(24, 27),
      std::make_pair(28, 37), std::make_pair(37, 38), std::make_pair(39, 41),
      std::make_pair(42, 43), std::make_pair(44, 49), std::make_pair(49, 50),
      std::make_pair(51, 57), std::make_pair(57, 58), std::make_pair(58, 62),
      std::make_pair(63, 69), std::make_pair(69, 70)};

  Tokenizer tokenizer;
  EXPECT_EQ(tokenizer.Tokenize(input_string), tokens);
}

TEST_F(PhraseSegmentationTokenizerTest, TokenizeApostrophes) {
  const std::u16string input_string =
      u"David's father can't, won't, and didn't care.";
  //    0123456789012345678901234567890123456789012345
  std::vector<std::pair<int, int>> tokens{
      std::make_pair(0, 7),   std::make_pair(8, 14),  std::make_pair(15, 20),
      std::make_pair(20, 21), std::make_pair(22, 27), std::make_pair(27, 28),
      std::make_pair(29, 32), std::make_pair(33, 39), std::make_pair(40, 44),
      std::make_pair(44, 45)};

  Tokenizer tokenizer;
  EXPECT_EQ(tokenizer.Tokenize(input_string), tokens);
}

TEST_F(PhraseSegmentationTokenizerTest, TokenizeNonAscii) {
  const std::u16string input_string =
      u"Ce film est très intéressant : c'est un classique.";
  //    0123456789012345678901234567890123456789012345678901234567890123456789
  std::vector<std::pair<int, int>> tokens{
      std::make_pair(0, 2),   std::make_pair(3, 7),   std::make_pair(8, 11),
      std::make_pair(12, 16), std::make_pair(17, 28), std::make_pair(29, 30),
      std::make_pair(31, 36), std::make_pair(37, 39), std::make_pair(40, 49),
      std::make_pair(49, 50)};

  Tokenizer tokenizer;
  EXPECT_EQ(tokenizer.Tokenize(input_string), tokens);
}

TEST_F(PhraseSegmentationTokenizerTest, TokenizeNumbers) {
  const std::u16string input_string = u"2 °C is 35 °F for H2O.";
  //                                    01234567890123456789012
  std::vector<std::pair<int, int>> tokens{
      std::make_pair(0, 1),   std::make_pair(2, 3),   std::make_pair(3, 4),
      std::make_pair(5, 7),   std::make_pair(8, 10),  std::make_pair(11, 12),
      std::make_pair(12, 13), std::make_pair(14, 17), std::make_pair(18, 21),
      std::make_pair(21, 22)};

  Tokenizer tokenizer;
  EXPECT_EQ(tokenizer.Tokenize(input_string), tokens);
}

}  // namespace
