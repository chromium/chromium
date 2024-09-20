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

using testing::ElementsAre;
using testing::IsEmpty;

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

testing::Matcher<ReadAloudTextSegment> TextSegmentMatcher(ui::AXNodeID id,
                                                          int text_start,
                                                          int text_end) {
  return testing::AllOf(
      ::testing::Field(&ReadAloudTextSegment::id, ::testing::Eq(id)),
      ::testing::Field(&ReadAloudTextSegment::text_start,
                       ::testing::Eq(text_start)),
      ::testing::Field(&ReadAloudTextSegment::text_end,
                       ::testing::Eq(text_end)));
}

TEST_F(ReadAnythingReadAloudTraversalUtilsTest,
       GetSegmentsForRange_OnlyOneNode_ReturnsCorrectSegments) {
  a11y::ReadAloudCurrentGranularity current_granularity;
  // Text indices:                           012345678901234567890123
  current_granularity.AddText(101, 12, 36, u"Ice-cream candy and cake");
  std::vector<ReadAloudTextSegment> out =
      current_granularity.GetSegmentsForRange(2, 5);
  EXPECT_THAT(out, ElementsAre(TextSegmentMatcher(101, 14, 17)));
}

TEST_F(ReadAnythingReadAloudTraversalUtilsTest,
       GetSegmentsForRange_TwoNodes_ReturnsCorrectSegments) {
  a11y::ReadAloudCurrentGranularity current_granularity;
  // Text indices:                           01234567890123456
  current_granularity.AddText(101, 12, 22, u"Ice-cream ");
  current_granularity.AddText(102, 40, 55, u"candy and cakes");

  std::vector<ReadAloudTextSegment> out;
  // Within node 1
  out = current_granularity.GetSegmentsForRange(0, 0);
  EXPECT_THAT(out, IsEmpty());  // Empty return for empty range.

  // Just "I" of "Ice-cream"
  out = current_granularity.GetSegmentsForRange(0, 1);
  // Should return the first segment, and offsets 12 to 13, since the first
  // segment has a text_start of 12.
  EXPECT_THAT(out, ElementsAre(TextSegmentMatcher(101, 12, 13)));

  // "Ice" of "Ice-cream"
  out = current_granularity.GetSegmentsForRange(0, 3);
  EXPECT_THAT(out, ElementsAre(TextSegmentMatcher(101, 12, 15)));

  // Edge (final " " in "Ice-cream "), should return just that last offset (21
  // to 22), since 22 is the end of the first segment.
  out = current_granularity.GetSegmentsForRange(9, 10);
  EXPECT_THAT(out, ElementsAre(TextSegmentMatcher(101, 21, 22)));

  out = current_granularity.GetSegmentsForRange(10, 10);
  EXPECT_THAT(out, IsEmpty());  // Empty return for empty range.

  // Spanning both nodes. 4-9 is in the first node, and 10-15 is in the second.
  // Since the first node starts with offset 12 and the second node with offset
  // 40, the return value should consist of two elements:
  // Node 1 with text range 12+4 to 12+8+1, and
  // Node 2 with the range 40+0 to 40+5.
  out = current_granularity.GetSegmentsForRange(4, 15);
  EXPECT_THAT(out, ElementsAre(TextSegmentMatcher(101, 16, 22),
                               TextSegmentMatcher(102, 40, 45)));

  // Within node 2.
  out = current_granularity.GetSegmentsForRange(10, 11);
  EXPECT_THAT(out, ElementsAre(TextSegmentMatcher(102, 40, 41)));

  out = current_granularity.GetSegmentsForRange(12, 14);
  EXPECT_THAT(out, ElementsAre(TextSegmentMatcher(102, 42, 44)));

  // Edge (final "s" in "cakes").
  out = current_granularity.GetSegmentsForRange(24, 25);
  EXPECT_THAT(out, ElementsAre(TextSegmentMatcher(102, 54, 55)));

  // Beyond the end, should return empty.
  out = current_granularity.GetSegmentsForRange(25, 26);
  EXPECT_THAT(out, IsEmpty());
}

TEST_F(ReadAnythingReadAloudTraversalUtilsTest,
       GetSegmentsForRange_ManyNodes_ReturnsCorrectSegments) {
  a11y::ReadAloudCurrentGranularity current_granularity;
  // Sentence and indices:
  // Ice-cream candy and cakes and yummy treats for all of us, except you!
  // 0123456789012345678901234567890123456789012345678901234567890123456789
  current_granularity.AddText(101, 12, 22, u"Ice-cream ");
  current_granularity.AddText(102, 40, 56, u"candy and cakes ");
  current_granularity.AddText(103, 7, 28, u"and yummy treats for ");
  current_granularity.AddText(104, 16, 26, u"all of us,");
  current_granularity.AddText(105, 20, 31, u"except you!");
  std::vector<ReadAloudTextSegment> out =
      current_granularity.GetSegmentsForRange(4, 50);
  EXPECT_THAT(out, ElementsAre(TextSegmentMatcher(101, 16, 22),
                               TextSegmentMatcher(102, 40, 56),
                               TextSegmentMatcher(103, 7, 28),
                               TextSegmentMatcher(104, 16, 19)));
}

TEST_F(ReadAnythingReadAloudTraversalUtilsTest,
       GetSegmentsForRange_OutsideRange_ReturnsCorrectSegments) {
  a11y::ReadAloudCurrentGranularity current_granularity;
  // Sentence and indices:
  // Ice-cream candy and cake
  // 012345678901234567890123
  current_granularity.AddText(101, 12, 36, u"Ice-cream candy and cake");
  std::vector<ReadAloudTextSegment> out =
      current_granularity.GetSegmentsForRange(38, 45);
  EXPECT_THAT(out, IsEmpty());
}

TEST_F(ReadAnythingReadAloudTraversalUtilsTest,
       CalculatePhrases_EmptyText_ReturnsEmptyResult) {
  a11y::ReadAloudCurrentGranularity empty_sentence;

  EXPECT_THAT(empty_sentence.phrase_boundaries, IsEmpty());

  // CalculatePhrases should do nothing with an empty sentence.
  empty_sentence.CalculatePhrases();

  EXPECT_THAT(empty_sentence.phrase_boundaries, IsEmpty());
}

TEST_F(
    ReadAnythingReadAloudTraversalUtilsTest,
    CalculatePhrases_DifferentKindsOfText_ReturnsPhraseBoundariesEvery3Words) {
  a11y::ReadAloudCurrentGranularity normal_sentence;
  // Sentence and indices:
  // Ice-cream candy and cake
  // 012345678901234567890123
  normal_sentence.AddText(101, 12, 36, u"Ice cream candy and cake");
  // Before calculating phrases, phrase_boundaries is empty.
  EXPECT_THAT(normal_sentence.phrase_boundaries, IsEmpty());

  normal_sentence.CalculatePhrases();
  // Boundaries at "I" of Ice and 'c' of cake.
  EXPECT_THAT(normal_sentence.phrase_boundaries, ElementsAre(0, 16, 24));

  a11y::ReadAloudCurrentGranularity sentence_with_hyphen;
  // Sentence and indices:
  // Ice-cream candy and cake
  // 012345678901234567890123
  sentence_with_hyphen.AddText(101, 12, 36, u"Ice-cream candy and cake");
  sentence_with_hyphen.CalculatePhrases();
  // Boundaries at "I" of Ice-cream and 'a' of and
  EXPECT_THAT(sentence_with_hyphen.phrase_boundaries, ElementsAre(0, 16, 24));

  // Length is a multiple of three.
  a11y::ReadAloudCurrentGranularity sixword_sentence;
  sixword_sentence.AddText(101, 41, 65, u"He is going to the mall.");
  sixword_sentence.CalculatePhrases();
  // Boundary every 3 words.
  EXPECT_THAT(sixword_sentence.phrase_boundaries, ElementsAre(0, 12, 24));

  // Short sentences.
  a11y::ReadAloudCurrentGranularity short_sentence;
  short_sentence.AddText(101, 12, 27, u"Ice-cream candy");
  short_sentence.CalculatePhrases();
  // Boundary only at "I" of Ice-cream
  EXPECT_THAT(short_sentence.phrase_boundaries, ElementsAre(0, 15));

  // Very short sentences.
  a11y::ReadAloudCurrentGranularity oneword_sentence;
  oneword_sentence.AddText(101, 12, 15, u"Yes");
  oneword_sentence.CalculatePhrases();
  EXPECT_THAT(oneword_sentence.phrase_boundaries, ElementsAre(0, 3));
}

TEST_F(ReadAnythingReadAloudTraversalUtilsTest,
       GetPhraseIndex_ReturnsCorrectIndexes) {
  a11y::ReadAloudCurrentGranularity current_granularity;
  // Sentence and indices:
  // Ice-cream candy and cakes and yummy treats for all of us, except you!
  // 0123456789012345678901234567890123456789012345678901234567890123456789
  current_granularity.AddText(101, 12, 22, u"Ice-cream ");
  current_granularity.AddText(102, 40, 56, u"candy and cakes ");
  current_granularity.AddText(103, 7, 28, u"and yummy treats for ");
  current_granularity.AddText(104, 16, 26, u"all of us,");
  current_granularity.AddText(105, 20, 31, u"except you!");

  current_granularity.phrase_boundaries = {0, 26, 43, 58, 69};
  // Before the start of the string returns -1.
  EXPECT_THAT(current_granularity.GetPhraseIndex(-1), -1);

  // Within the string, returns the closest phrase start.
  EXPECT_THAT(current_granularity.GetPhraseIndex(0), 0);
  EXPECT_THAT(current_granularity.GetPhraseIndex(1), 0);
  EXPECT_THAT(current_granularity.GetPhraseIndex(10), 0);
  EXPECT_THAT(current_granularity.GetPhraseIndex(25), 0);
  EXPECT_THAT(current_granularity.GetPhraseIndex(26), 1);
  EXPECT_THAT(current_granularity.GetPhraseIndex(30), 1);
  EXPECT_THAT(current_granularity.GetPhraseIndex(42), 1);
  EXPECT_THAT(current_granularity.GetPhraseIndex(43), 2);
  EXPECT_THAT(current_granularity.GetPhraseIndex(57), 2);
  EXPECT_THAT(current_granularity.GetPhraseIndex(58), 3);
  EXPECT_THAT(current_granularity.GetPhraseIndex(68), 3);

  // Past the end of the string returns the last index.
  EXPECT_THAT(current_granularity.GetPhraseIndex(69), 4);
  EXPECT_THAT(current_granularity.GetPhraseIndex(79), 4);
  EXPECT_THAT(current_granularity.GetPhraseIndex(109), 4);
}
