// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/accessibility/phrase_segmentation/phrase_segmenter.h"

#include <string>
#include <vector>

#include "chrome/renderer/accessibility/phrase_segmentation/dependency_tree.h"
#include "chrome/renderer/accessibility/phrase_segmentation/token_boundaries.h"
#include "chrome/renderer/accessibility/phrase_segmentation/tokenized_sentence.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class PhraseSegmentationPhraseSegmenterTest : public testing::Test {
 protected:
  PhraseSegmentationPhraseSegmenterTest() = default;
};

TEST_F(PhraseSegmentationPhraseSegmenterTest, SegmentTestWords) {
  const std::u16string sentence =
      u"The result is \"ice cream\"; a smooth, semisolid foam.";
  //    01234567890123 4567890123 4567890123456789012345678901
  const TokenizedSentence tokenized_sentence(sentence);
  const std::vector<int> dependency_heads = {1, 5,  5,  5,  5,  5, 5,
                                             5, 12, 12, 12, 12, 5, 5};
  ASSERT_EQ(dependency_heads.size(), tokenized_sentence.tokens().size());
  const DependencyTree dependency_tree(tokenized_sentence, dependency_heads);

  const TokenBoundaries token_boundaries(dependency_tree);
  PhraseSegmenter smart_highlight;
  std::vector<int> split_char_offsets =
      CalculatePhraseBoundaries(smart_highlight, tokenized_sentence,
                                token_boundaries, Strategy::kWords, 4);
  // The result is /"ice cream"; /a smooth, /semisolid foam.
  std::vector<int> expected_split_char_offsets = {0, 14, 27, 37};
  EXPECT_EQ(split_char_offsets, expected_split_char_offsets);
}

TEST_F(PhraseSegmentationPhraseSegmenterTest, SegmentTestCharacters) {
  const std::u16string sentence =
      u"The result is \"ice cream\"; a smooth, semisolid foam.";
  //    01234567890123 4567890123 4567890123456789012345678901
  const TokenizedSentence tokenized_sentence(sentence);
  const std::vector<int> dependency_heads = {1, 5,  5,  5,  5,  5, 5,
                                             5, 12, 12, 12, 12, 5, 5};
  ASSERT_EQ(dependency_heads.size(), tokenized_sentence.tokens().size());
  const DependencyTree dependency_tree(tokenized_sentence, dependency_heads);

  const TokenBoundaries token_boundaries(dependency_tree);
  PhraseSegmenter smart_highlight;
  std::vector<int> split_char_offsets =
      CalculatePhraseBoundaries(smart_highlight, tokenized_sentence,
                                token_boundaries, Strategy::kCharacters, 20);
  // The result is /"ice cream"; /a smooth, semisolid /foam.
  std::vector<int> expected_split_char_offsets = {0, 14, 27, 37};
  EXPECT_EQ(split_char_offsets, expected_split_char_offsets);
}

TEST_F(PhraseSegmentationPhraseSegmenterTest,
       SplitsPhrasesInCenterIfWeightsEqual) {
  const std::u16string sentence = u"a smooth, semisolid foam.";
  //                                0123456789012345678901234
  const TokenizedSentence tokenized_sentence(sentence);
  const std::vector<int> dependency_heads = {4, 4, 4, 4, 4, 4};
  ASSERT_EQ(dependency_heads.size(), tokenized_sentence.tokens().size());
  const DependencyTree dependency_tree(tokenized_sentence, dependency_heads);

  const TokenBoundaries token_boundaries(dependency_tree);
  PhraseSegmenter smart_highlight;
  std::vector<int> split_char_offsets =
      CalculatePhraseBoundaries(smart_highlight, tokenized_sentence,
                                token_boundaries, Strategy::kWords, 3);
  // a smooth, /semisolid foam.
  std::vector<int> expected_split_char_offsets = {0, 10};
  EXPECT_EQ(split_char_offsets, expected_split_char_offsets);
}

TEST_F(PhraseSegmentationPhraseSegmenterTest,
       SegmentTestCharactersWithLongWord) {
  const std::u16string sentence =
      u"A smooth, semisolidifyingableificationifical foam.";
  //    0123456789012345678901234567890123456789012345678901
  const TokenizedSentence tokenized_sentence(sentence);
  const std::vector<int> dependency_heads = {4, 4, 4, 4, 4, 4};
  ASSERT_EQ(dependency_heads.size(), tokenized_sentence.tokens().size());
  const DependencyTree dependency_tree(tokenized_sentence, dependency_heads);

  const TokenBoundaries token_boundaries(dependency_tree);
  PhraseSegmenter smart_highlight;
  std::vector<int> split_char_offsets =
      CalculatePhraseBoundaries(smart_highlight, tokenized_sentence,
                                token_boundaries, Strategy::kCharacters, 20);
  // The result is /"ice cream"; /a smooth, semisolid /foam.
  std::vector<int> expected_split_char_offsets = {0, 10, 45};
  EXPECT_EQ(split_char_offsets, expected_split_char_offsets);
}

}  // namespace
