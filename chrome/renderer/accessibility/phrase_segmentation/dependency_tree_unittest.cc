// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/accessibility/phrase_segmentation/dependency_tree.h"

#include <string>
#include <vector>

#include "chrome/renderer/accessibility/phrase_segmentation/tokenized_sentence.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using ::testing::ElementsAre;
using ::testing::FieldsAre;

class PhraseSegmentationDependencyTreeTest : public testing::Test {
 protected:
  PhraseSegmentationDependencyTreeTest() = default;
};

//
// The dependency graph for the example sentence is:
//
// cream                5
//   |- ice             4
//   |- result          1
//   |    |- the        0
//   |- is              2
//   |- "               3
//   |- "               6
//   |- ;               7
//   |- foam            12
//   |   |- a           8
//   |   |- smooth      9
//   |   |- ,           10
//   |   |- semisoild   11
//   |- .               13
//
TEST_F(PhraseSegmentationDependencyTreeTest,
       InitializesDependencyGraphCorrectly) {
  const std::u16string sentence =
      u"The result is \"ice cream\"; a smooth, semisolid foam.";
  //    01234567890123 4567890123 4567890123456789012345678901
  const TokenizedSentence tokenized_sentence(sentence);
  const std::vector<int> dependency_heads = {1, 5,  5,  5,  5,  5, 5,
                                             5, 12, 12, 12, 12, 5, 5};
  EXPECT_EQ(dependency_heads.size(), tokenized_sentence.tokens().size());
  const DependencyTree dependency_tree(tokenized_sentence, dependency_heads);

  std::vector<Dependency> expected_dep_head_array = {};
  EXPECT_THAT(
      dependency_tree.dep_head_array(),
      ElementsAre(
          FieldsAre(u"The", 0, 1, 0, 0, 0), FieldsAre(u"result", 1, 5, 4, 0, 1),
          FieldsAre(u"is", 2, 5, 11, 2, 2), FieldsAre(u"\"", 3, 5, 14, 3, 3),
          FieldsAre(u"ice", 4, 5, 15, 4, 4),
          FieldsAre(u"cream", 5, 5, 19, 0, 13),
          FieldsAre(u"\"", 6, 5, 24, 6, 6), FieldsAre(u";", 7, 5, 25, 7, 7),
          FieldsAre(u"a", 8, 12, 27, 8, 8),
          FieldsAre(u"smooth", 9, 12, 29, 9, 9),
          FieldsAre(u",", 10, 12, 35, 10, 10),
          FieldsAre(u"semisolid", 11, 12, 37, 11, 11),
          FieldsAre(u"foam", 12, 5, 47, 8, 12),
          FieldsAre(u".", 13, 5, 51, 13, 13)));
}

TEST_F(PhraseSegmentationDependencyTreeTest, FindsBoundariesForEdgeCorrectly) {
  const std::u16string sentence =
      u"The result is \"ice cream\"; a smooth, semisolid foam.";

  const TokenizedSentence tokenized_sentence(sentence);
  const std::vector<int> dependency_heads = {1, 5,  5,  5,  5,  5, 5,
                                             5, 12, 12, 12, 12, 5, 5};
  EXPECT_EQ(dependency_heads.size(), tokenized_sentence.tokens().size());
  const DependencyTree dependency_tree(tokenized_sentence, dependency_heads);

  EXPECT_EQ(dependency_tree.FindBoundaryForParentEdge(0), 0);    // The-result
  EXPECT_EQ(dependency_tree.FindBoundaryForParentEdge(1), 1);    // result-is
  EXPECT_EQ(dependency_tree.FindBoundaryForParentEdge(2), 2);    // is-"
  EXPECT_EQ(dependency_tree.FindBoundaryForParentEdge(3), 3);    // "-ice
  EXPECT_EQ(dependency_tree.FindBoundaryForParentEdge(4), 4);    // ice-cream
  EXPECT_EQ(dependency_tree.FindBoundaryForParentEdge(5), -1);   // (None)
  EXPECT_EQ(dependency_tree.FindBoundaryForParentEdge(6), 5);    // cream-"
  EXPECT_EQ(dependency_tree.FindBoundaryForParentEdge(7), 6);    // "-;
  EXPECT_EQ(dependency_tree.FindBoundaryForParentEdge(8), 8);    // a-smooth
  EXPECT_EQ(dependency_tree.FindBoundaryForParentEdge(9), 9);    // smooth-,
  EXPECT_EQ(dependency_tree.FindBoundaryForParentEdge(10), 10);  // ,-semisolid
  EXPECT_EQ(dependency_tree.FindBoundaryForParentEdge(11),
            11);  // semisolid-foam
  EXPECT_EQ(dependency_tree.FindBoundaryForParentEdge(12), 7);   // ;-a
  EXPECT_EQ(dependency_tree.FindBoundaryForParentEdge(13), 12);  // foam-.
}

}  // namespace
