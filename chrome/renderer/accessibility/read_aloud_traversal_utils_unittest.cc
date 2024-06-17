// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/accessibility/read_aloud_traversal_utils.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/accessibility/ax_node_position.h"

class ReadAnythingReadAloudTraversalUtilsTest : public testing::Test {
 protected:
  ReadAnythingReadAloudTraversalUtilsTest() = default;
};

TEST_F(ReadAnythingReadAloudTraversalUtilsTest,
       GetNextSentence_ReturnsCorrectIndex) {
  const std::u16string first_sentence = u"This is a normal sentence. ";
  const std::u16string second_sentence = u"This is a second sentence.";

  const std::u16string sentence = first_sentence + second_sentence;
  size_t index = GetNextSentence(sentence, false);
  EXPECT_EQ(index, first_sentence.length());
  EXPECT_EQ(sentence.substr(0, index), first_sentence);
}

TEST_F(ReadAnythingReadAloudTraversalUtilsTest,
       GetNextSentence_OnlyOneSentence_ReturnsCorrectIndex) {
  const std::u16string sentence = u"Hello, this is a normal sentence.";

  size_t index = GetNextSentence(sentence, false);
  EXPECT_EQ(index, sentence.length());
  EXPECT_EQ(sentence.substr(0, index), sentence);
}

TEST_F(ReadAnythingReadAloudTraversalUtilsTest,
       GetNextSentence_NotPDF_DoesNotFilterReturnCharacters) {
  const std::u16string sentence =
      u"Hello, this is\n a sentence \r with line breaks.";

  size_t index = GetNextSentence(sentence, false);
  EXPECT_EQ(index, sentence.find('\n') + 2);
  EXPECT_EQ(sentence.substr(0, index), u"Hello, this is\n ");

  std::u16string next_sentence = sentence.substr(index);
  index = GetNextSentence(next_sentence, false);
  EXPECT_EQ(index, next_sentence.find('\r') + 2);
  EXPECT_EQ(next_sentence.substr(0, index), u"a sentence \r ");

  next_sentence = next_sentence.substr(index);
  index = GetNextSentence(next_sentence, false);
  EXPECT_EQ(index, next_sentence.length());
  EXPECT_EQ(next_sentence.substr(0, index), u"with line breaks.");
}

TEST_F(ReadAnythingReadAloudTraversalUtilsTest,
       GetNextSentence_PDF_FiltersReturnCharacters) {
  const std::u16string sentence =
      u"Hello, this is\n a sentence \r with line breaks.";

  size_t index = GetNextSentence(sentence, true);
  EXPECT_EQ(index, sentence.length());
  EXPECT_EQ(sentence.substr(0, index), sentence);
}

TEST_F(ReadAnythingReadAloudTraversalUtilsTest,
       GetNextSentence_PDF_DoesNotFilterReturnCharactersAtEndOfSentence) {
  const std::u16string sentence =
      u"Hello, this is a sentence with line breaks.\r\n";

  size_t index = GetNextSentence(sentence, true);
  EXPECT_EQ(index, sentence.length());
  EXPECT_EQ(sentence.substr(0, index), sentence);
}

TEST_F(ReadAnythingReadAloudTraversalUtilsTest,
       GetNextWord_ReturnsCorrectIndex) {
  const std::u16string first_word = u"onomatopoeia ";
  const std::u16string second_word = u"party";

  const std::u16string segment = first_word + second_word;
  size_t index = GetNextWord(segment);
  EXPECT_EQ(index, first_word.length());
  EXPECT_EQ(segment.substr(0, index), first_word);
}

TEST_F(ReadAnythingReadAloudTraversalUtilsTest,
       GetNextWord_OnlyOneWord_ReturnsCorrectIndex) {
  const std::u16string word = u"Happiness";

  size_t index = GetNextWord(word);
  EXPECT_EQ(index, word.length());
  EXPECT_EQ(word.substr(0, index), word);
}

TEST_F(ReadAnythingReadAloudTraversalUtilsTest,
       IsOpeningPunctuation_ReturnsExpected) {
  char open_parentheses = '(';
  char open_bracket = '[';

  EXPECT_TRUE(IsOpeningPunctuation(open_parentheses));
  EXPECT_TRUE(IsOpeningPunctuation(open_bracket));

  // Closing puncutation shouldn't count.
  char close_parentheses = ')';
  char close_bracket = ']';

  EXPECT_FALSE(IsOpeningPunctuation(close_parentheses));
  EXPECT_FALSE(IsOpeningPunctuation(close_bracket));
}
