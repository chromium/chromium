// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/accessibility/phrase_segmentation/token_boundaries.h"

#include <string>
#include <vector>

#include "chrome/renderer/accessibility/phrase_segmentation/dependency_tree.h"
#include "chrome/renderer/accessibility/phrase_segmentation/tokenized_sentence.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using ::testing::ElementsAreArray;

class PhraseSegmentationTokenBoundariesTest : public testing::Test {
 protected:
  PhraseSegmentationTokenBoundariesTest() = default;
};

TEST_F(PhraseSegmentationTokenBoundariesTest,
       InitializesTokenBoundariesCorrectly) {
  const std::u16string sentence =
      u"The result is \"ice cream\"; a smooth, semisolid foam.";

  const TokenizedSentence tokenized_sentence(sentence);
  const std::vector<int> dependency_heads = {1, 5,  5,  5,  5,  5, 5,
                                             5, 12, 12, 12, 12, 5, 5};
  ASSERT_EQ(dependency_heads.size(), tokenized_sentence.tokens().size());
  const DependencyTree dependency_tree(tokenized_sentence, dependency_heads);

  const TokenBoundaries token_boundaries(dependency_tree);
  const std::vector<int> expected_boundary_weights = {
      1 /* the-result: distance */,
      1 /* result-is: same parent */,
      2 /* is-": same parent + start-indicator */,
      0 /* "-ice: no whitespace */,
      1 /* ice-cream: same parent */,
      0 /* cream-": no whitespace */,
      0 /* "-;: no whitespace */,
      11 /* ;-a: distance + end-indicator */,
      1 /* a-smooth: same parent */,
      0 /* smooth-,: no whitespace */,
      5 /* ,-semisolid: same parent + end-indicator */,
      1 /* semisolid-foam: same parent */,
      0 /* foam-.: no whitespace */};
  EXPECT_THAT(token_boundaries.boundary_weights(),
              ElementsAreArray(expected_boundary_weights));
}

}  // namespace
