// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/accessibility/read_anything/read_anything_text_aligner.h"

#include <memory>
#include <string>
#include <vector>

#include "base/test/metrics/histogram_tester.h"
#include "base/types/expected.h"
#include "chrome/renderer/accessibility/read_anything/read_anything_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/accessibility/ax_tree.h"
#include "ui/accessibility/ax_tree_id.h"
#include "ui/accessibility/ax_tree_update.h"

class ReadAnythingTextAlignerTest : public testing::Test {
 protected:
  // Replaces the tree that the blocks are mapped onto.
  void SetTree(const ui::AXTreeUpdate& update) {
    tree_ = std::make_unique<ui::AXTree>(update);
  }

  // Maps `blocks` onto the tree from SetTree(). Returns false if the aligner
  // fails, in which case every block is left unmapped.
  bool MapRenderedTextToTree(const std::vector<std::u16string>& blocks,
                             bool is_loaded = false) {
    base::expected<ReadAnythingTextAligner::TextToAXMap,
                   ReadAnythingTextAligner::AlignmentError>
        text_to_ax_map = ReadAnythingTextAligner::MapRenderedTextToTree(
            blocks, *tree_, is_loaded);
    const bool has_value = text_to_ax_map.has_value();
    text_to_ax_map_ =
        std::move(text_to_ax_map)
            .value_or(ReadAnythingTextAligner::TextToAXMap(blocks.size()));
    return has_value;
  }

  const ui::AXTree& tree() const { return *tree_; }

  // Returns the segments mapped to the block at `index` by the last
  // MapRenderedTextToTree() call.
  std::vector<ReadAnythingTextAligner::MappingSegment> GetAXMapping(
      size_t index) const {
    if (index >= text_to_ax_map_.size()) {
      return {};
    }
    return text_to_ax_map_[index];
  }

  const ui::AXTreeID tree_id_ = ui::AXTreeID::CreateNewAXTreeID();

 private:
  std::unique_ptr<ui::AXTree> tree_;
  ReadAnythingTextAligner::TextToAXMap text_to_ax_map_;
};

TEST_F(ReadAnythingTextAlignerTest,
       MapRenderedTextToTree_DefersWhenTreeIncomplete) {
  base::HistogramTester histograms;
  std::vector<std::u16string> blocks = {
      u"Article Title - Header",
      u"This is the full body paragraph of the distilled article content."};

  // Initial tree only contains a short header (< 50% of distilled length).
  ui::AXTreeUpdate short_header_update;
  test::SetUpdateTreeID(&short_header_update, tree_id_);
  short_header_update.root_id = 1;
  short_header_update.nodes = {test::TextNode(1, u"Header")};
  SetTree(short_header_update);

  EXPECT_FALSE(MapRenderedTextToTree(blocks));

  // Tree has enough total characters (>= 50% of distilled length) due to
  // non-article chrome, but mapped characters (6 / 87 = 6%) are still < 50%.
  ui::AXTreeUpdate nav_update;
  test::SetUpdateTreeID(&nav_update, tree_id_);
  nav_update.root_id = 1;
  nav_update.nodes = {test::TextNode(
      1, u"Header and unrelated navigation links before body loads")};
  SetTree(nav_update);

  EXPECT_FALSE(MapRenderedTextToTree(blocks));
  EXPECT_TRUE(GetAXMapping(0).empty());
  EXPECT_TRUE(GetAXMapping(1).empty());
  histograms.ExpectTotalCount(
      "Accessibility.ReadAnything.ReadabilityMapping.SuccessRate", 0);

  // Once the full tree arrives, mapping succeeds.
  ui::AXTreeUpdate full_update;
  test::SetUpdateTreeID(&full_update, tree_id_);
  full_update.root_id = 1;
  full_update.nodes = {test::TextNode(1, blocks[1])};
  SetTree(full_update);

  EXPECT_TRUE(MapRenderedTextToTree(blocks));
  EXPECT_FALSE(GetAXMapping(1).empty());
  histograms.ExpectTotalCount(
      "Accessibility.ReadAnything.ReadabilityMapping.SuccessRate", 1);
}

TEST_F(ReadAnythingTextAlignerTest,
       MapRenderedTextToTree_AcceptsLowMatchRateWhenLoaded) {
  base::HistogramTester histograms;
  ui::AXTreeUpdate update;
  test::SetUpdateTreeID(&update, tree_id_);
  update.root_id = 1;
  update.nodes = {test::TextNode(1, u"Header")};
  SetTree(update);
  std::vector<std::u16string> blocks = {
      u"Article Title - Header",
      u"This is the full body paragraph of the distilled article content."};

  // While loading, too little of the text matches.
  EXPECT_FALSE(MapRenderedTextToTree(blocks));

  // Once loaded, whatever matched is accepted.
  EXPECT_TRUE(MapRenderedTextToTree(blocks, /*is_loaded=*/true));
  histograms.ExpectUniqueSample(
      "Accessibility.ReadAnything.ReadabilityMapping.SuccessRate", 6, 1);
}

TEST_F(ReadAnythingTextAlignerTest,
       MapRenderedTextToTree_TreeWithNoText_ReturnsError) {
  ui::AXTreeUpdate update;
  test::SetUpdateTreeID(&update, tree_id_);
  update.root_id = 1;
  update.nodes = {test::GenericContainerNode(1)};
  SetTree(update);

  EXPECT_EQ(
      ReadAnythingTextAligner::MapRenderedTextToTree({u"block1"}, tree(),
                                                     /*is_loaded=*/false),
      base::unexpected(ReadAnythingTextAligner::AlignmentError::kLowMatchRate));
  EXPECT_EQ(ReadAnythingTextAligner::MapRenderedTextToTree({u"block1"}, tree(),
                                                           /*is_loaded=*/true),
            base::unexpected(ReadAnythingTextAligner::AlignmentError::kNoText));
}

TEST_F(ReadAnythingTextAlignerTest,
       MapRenderedTextToTree_MapsNestedTextInReadingOrder) {
  // Create a tree structure:
  //      1 (root)
  //     / \
  //    2   3 (container)
  //         \
  //          4 (text)
  ui::AXTreeUpdate update;
  test::SetUpdateTreeID(&update, tree_id_);
  update.root_id = 1;
  ui::AXNodeData node1 = test::GenericContainerNode(1);
  node1.child_ids = {2, 3};
  ui::AXNodeData node2 = test::TextNode(2, u"Hello");
  ui::AXNodeData node3 = test::GenericContainerNode(3);
  node3.child_ids = {4};
  ui::AXNodeData node4 = test::TextNode(4, u" world!");
  update.nodes = {node1, node2, node3, node4};
  SetTree(update);

  ASSERT_TRUE(MapRenderedTextToTree({u"Title", u"Hello world!"}));

  // The block spans both text nodes, in reading order.
  auto mapping = GetAXMapping(1);
  ASSERT_EQ(mapping.size(), 2u);

  EXPECT_EQ(mapping[0].id, 2);
  EXPECT_EQ(mapping[0].start, 0);
  EXPECT_EQ(mapping[0].end, 5);
  EXPECT_EQ(mapping[0].ax_node_offset, 0);

  EXPECT_EQ(mapping[1].id, 4);
  EXPECT_EQ(mapping[1].start, 5);
  EXPECT_EQ(mapping[1].end, 12);
  EXPECT_EQ(mapping[1].ax_node_offset, 0);
}

TEST_F(ReadAnythingTextAlignerTest, MapRenderedTextToTree_RecordsMetrics) {
  base::HistogramTester histograms;
  // Setup a tree
  ui::AXTreeUpdate update;
  test::SetUpdateTreeID(&update, tree_id_);
  update.root_id = 1;
  update.nodes = {test::TextNode(1, u"Hello World")};
  SetTree(update);
  MapRenderedTextToTree({u"Hello World"});

  histograms.ExpectUniqueSample(
      "Accessibility.ReadAnything.ReadabilityMapping.SuccessRate", 100, 1);
  histograms.ExpectTotalCount(
      "Accessibility.ReadAnything.ReadabilityMapping.0_Total.ExecutionTime", 1);
  histograms.ExpectTotalCount(
      "Accessibility.ReadAnything.ReadabilityMapping.1_Flattening."
      "ExecutionTime",
      1);
  histograms.ExpectTotalCount(
      "Accessibility.ReadAnything.ReadabilityMapping.2_SuffixArray."
      "ExecutionTime",
      1);
  histograms.ExpectTotalCount(
      "Accessibility.ReadAnything.ReadabilityMapping.3_InitialAnchors."
      "ExecutionTime",
      1);
  histograms.ExpectTotalCount(
      "Accessibility.ReadAnything.ReadabilityMapping.4_GapAlignment."
      "ExecutionTime",
      1);
}

TEST_F(ReadAnythingTextAlignerTest,
       MapRenderedTextToTree_RecordsPartialSuccessRate) {
  base::HistogramTester histograms;
  // Setup a tree
  ui::AXTreeUpdate update;
  test::SetUpdateTreeID(&update, tree_id_);
  update.root_id = 1;
  update.nodes = {test::TextNode(1, u"Mapped Text")};
  SetTree(update);

  // Provide two blocks: one that matches perfectly, and one that doesn't match
  // at all. Both blocks are the same length (11 characters).
  MapRenderedTextToTree({u"Mapped Text", u"Unmap Text!"});

  histograms.ExpectUniqueSample(
      "Accessibility.ReadAnything.ReadabilityMapping.SuccessRate", 50, 1);
}

TEST_F(ReadAnythingTextAlignerTest, MapRenderedTextToTree_OneBlockToManyNodes) {
  // Setup AXTree: A single paragraph spanning 3 nodes (Text -> Link -> Text).
  ui::AXTreeUpdate update;
  test::SetUpdateTreeID(&update, tree_id_);
  ui::AXNodeData root = test::GenericContainerNode(1);
  ui::AXNodeData node2 = test::TextNode(2, u"This is ");
  ui::AXNodeData node3 = test::LinkNode(3, "http://google.com");
  ui::AXNodeData node5 = test::TextNode(5, u"a link");
  node3.child_ids = {node5.id};
  ui::AXNodeData node4 = test::TextNode(4, u" in a paragraph.");
  root.child_ids = {node2.id, node3.id, node4.id};
  update.root_id = root.id;
  update.nodes = {root, node2, node3, node5, node4};
  SetTree(update);

  // Mock the distilled Block.
  std::vector<std::u16string> blocks = {u"This is a link in a paragraph."};

  // Run mapping algorithm.
  MapRenderedTextToTree(blocks);

  // Validate mapping was done correctly.

  // Mapping of block is of correct size (3 nodes map to the block).
  auto mapping = GetAXMapping(0);
  ASSERT_EQ(mapping.size(), 3u);

  // Segment 1: "This is " (Node 2)
  EXPECT_EQ(mapping[0].id, 2);
  EXPECT_EQ(mapping[0].start, 0);
  EXPECT_EQ(mapping[0].end, 8);

  // Segment 2: "a link" (Node 5 - the StaticText child of the Link)
  EXPECT_EQ(mapping[1].id, 5);
  EXPECT_EQ(mapping[1].start, 8);
  EXPECT_EQ(mapping[1].end, 14);

  // Segment 3: " in a paragraph." (Node 4)
  EXPECT_EQ(mapping[2].id, 4);
  EXPECT_EQ(mapping[2].start, 14);
  EXPECT_EQ(mapping[2].end, 30);
}

TEST_F(ReadAnythingTextAlignerTest,
       MapRenderedTextToTree_UniquenessConstraints) {
  // Setup AXTree: A single paragraph spanning 3 nodes (Duplicate  -> Unique  ->
  // Duplicate).
  ui::AXTreeUpdate update;
  test::SetUpdateTreeID(&update, tree_id_);
  ui::AXNodeData root = test::GenericContainerNode(1);
  ui::AXNodeData node2 = test::TextNode(2, u"Twinword");
  ui::AXNodeData node3 = test::TextNode(3, u"Unique");
  ui::AXNodeData node4 = test::TextNode(4, u"Twinword");
  root.child_ids = {node2.id, node3.id, node4.id};
  update.root_id = root.id;
  update.nodes = {root, node2, node3, node4};
  SetTree(update);

  // "Duplicate" is unique in blocks, but not AXTree (appears twice).
  // "Unique" is unique in both.
  // "Repeated" is unique in AXTree (it's not there), but not in blocks.
  std::vector<std::u16string> blocks = {u"Twinword", u"Unique", u"Repeat",
                                        u"Repeat"};

  // Run mapping algorithm.
  MapRenderedTextToTree(blocks);

  EXPECT_FALSE(GetAXMapping(0).empty());  // Duplicate in AXTree
  EXPECT_FALSE(GetAXMapping(1).empty());  // Unique in both
  EXPECT_TRUE(GetAXMapping(2).empty());   // Duplicate in blocks
  EXPECT_TRUE(GetAXMapping(3).empty());   // Duplicate in blocks
}

TEST_F(ReadAnythingTextAlignerTest,
       MapRenderedTextToTree_ReadabilityTitle_AvoidsFalseMatch) {
  // Setup AXTree: Body appears before title string to simulate same text as
  // title being in the body.
  std::u16string title = u"Synthesized Title";
  std::u16string body = u"This is the actual body content.";

  ui::AXTreeUpdate update;
  test::SetUpdateTreeID(&update, tree_id_);
  update.root_id = 1;
  update.nodes = {test::GenericContainerNode(1), test::TextNode(2, body),
                  test::TextNode(3, title)};
  update.nodes[0].child_ids = {2, 3};
  SetTree(update);

  // Add title first to simulate title text coming from metadata and not AXtree.
  std::vector<std::u16string> blocks = {title, body};
  MapRenderedTextToTree(blocks);

  // Body (block 1) should map correctly to node 2.
  EXPECT_FALSE(GetAXMapping(1).empty());
  EXPECT_EQ(GetAXMapping(1)[0].id, 2);

  // Title (block 0) should NOT map to node 3.
  EXPECT_TRUE(GetAXMapping(0).empty());
}

TEST_F(ReadAnythingTextAlignerTest, MapRenderedTextToTree_PartialNodeMapping) {
  // Setup AXTree: "Hello", " ", "World!" (Nodes 2, 3, 4)
  ui::AXTreeUpdate update;
  test::SetUpdateTreeID(&update, tree_id_);
  ui::AXNodeData root = test::GenericContainerNode(1);
  ui::AXNodeData n2 = test::TextNode(2, u"Hello");
  ui::AXNodeData n3 = test::TextNode(3, u" ");
  ui::AXNodeData n4 = test::TextNode(4, u"World!");
  root.child_ids = {2, 3, 4};
  update.root_id = 1;
  update.nodes = {root, n2, n3, n4};
  SetTree(update);

  // Mock a "llo Wor" block (maps to multiple nodes partially).
  // Global Text: "Hello World!" (Indices 012345678901)
  // Match range: [2, 9) -> "llo Wor"
  std::vector<std::u16string> blocks = {u"llo Wor"};

  // Run mapping algorithm.
  MapRenderedTextToTree(blocks);

  // Validate mapping was done correctly.

  // Mapping of block is of correct size (3 nodes map to the block).
  auto mapping = GetAXMapping(0);
  ASSERT_EQ(mapping.size(), 3u);

  // Node 2 ("Hello") contains "llo"
  EXPECT_EQ(mapping[0].id, 2);
  EXPECT_EQ(mapping[0].ax_node_offset, 2);
  EXPECT_EQ(mapping[0].start, 0);  // "llo" is at the start of our 7-char block
  EXPECT_EQ(mapping[0].end, 3);

  // Node 3 (" ") contains " "
  EXPECT_EQ(mapping[1].id, 3);
  EXPECT_EQ(mapping[1].start, 3);
  EXPECT_EQ(mapping[1].end, 4);
  EXPECT_EQ(mapping[1].ax_node_offset, 0);

  // Node 4 ("World!") contains "Wor"
  EXPECT_EQ(mapping[2].id, 4);
  EXPECT_EQ(mapping[2].start, 4);
  EXPECT_EQ(mapping[2].end, 7);  // End of our 7-char block
  EXPECT_EQ(mapping[2].ax_node_offset, 0);
}

TEST_F(ReadAnythingTextAlignerTest,
       MapRenderedTextToTree_NonBreakingSpaceWhitespaceNodes) {
  // Setup AXTree where words are separated by whitespace-only "\u00A0" nodes:
  // "Blood", "\u00A0", "is", "\u00A0", "vital." (Nodes 2, 3, 4, 5, 6)
  ui::AXTreeUpdate update;
  test::SetUpdateTreeID(&update, tree_id_);
  ui::AXNodeData root = test::GenericContainerNode(1);
  ui::AXNodeData n2 = test::TextNode(2, u"Blood");
  ui::AXNodeData n3 = test::TextNode(3, u"\u00A0");
  ui::AXNodeData n4 = test::TextNode(4, u"is");
  ui::AXNodeData n5 = test::TextNode(5, u"\u00A0");
  ui::AXNodeData n6 = test::TextNode(6, u"vital.");
  root.child_ids = {2, 3, 4, 5, 6};
  update.root_id = 1;
  update.nodes = {root, n2, n3, n4, n5, n6};
  SetTree(update);

  // In Readability, inline whitespace separators are replaced with regular
  // spaces (' '), producing "Blood is vital.".
  std::vector<std::u16string> blocks = {u"Blood is vital."};
  EXPECT_TRUE(MapRenderedTextToTree(blocks));

  // Verify the entire block (including short words and whitespace nodes) maps
  // cleanly across all 5 AXNodes.
  auto mapping = GetAXMapping(0);
  ASSERT_EQ(mapping.size(), 5u);

  EXPECT_EQ(mapping[0].id, 2);
  EXPECT_EQ(mapping[0].start, 0);
  EXPECT_EQ(mapping[0].end, 5);

  EXPECT_EQ(mapping[1].id, 3);
  EXPECT_EQ(mapping[1].start, 5);
  EXPECT_EQ(mapping[1].end, 6);

  EXPECT_EQ(mapping[2].id, 4);
  EXPECT_EQ(mapping[2].start, 6);
  EXPECT_EQ(mapping[2].end, 8);

  EXPECT_EQ(mapping[3].id, 5);
  EXPECT_EQ(mapping[3].start, 8);
  EXPECT_EQ(mapping[3].end, 9);

  EXPECT_EQ(mapping[4].id, 6);
  EXPECT_EQ(mapping[4].start, 9);
  EXPECT_EQ(mapping[4].end, 15);
}

TEST_F(ReadAnythingTextAlignerTest,
       MapRenderedTextToTree_GapSubstringAlignment_LocalUniqueness) {
  // Setup AXTree: [AnchorA] DuplicateString [AnchorB] DuplicateString
  std::u16string dup = u"DuplicateString12345";
  ui::AXTreeUpdate update;
  test::SetUpdateTreeID(&update, tree_id_);
  update.root_id = 1;
  update.nodes = {
      test::GenericContainerNode(1), test::TextNode(2, u"AnchorA_________"),
      test::TextNode(3, dup), test::TextNode(4, u"AnchorB_________"),
      test::TextNode(5, dup)};
  update.nodes[0].child_ids = {2, 3, 4, 5};
  SetTree(update);

  // Distilled blocks match the first occurrence.
  // Phase 1 will map AnchorA and AnchorB (globally unique).
  // Phase 2 should map the first "DuplicateString" because it's locally unique
  // between AnchorA and AnchorB.
  std::vector<std::u16string> blocks = {u"AnchorA_________", dup,
                                        u"AnchorB_________"};
  MapRenderedTextToTree(blocks);

  // AnchorA (Phase 1)
  auto mapping0 = GetAXMapping(0);
  ASSERT_EQ(mapping0.size(), 1u);
  EXPECT_EQ(mapping0[0].id, 2);

  // DuplicateString (Phase 2 - Local Uniqueness)
  auto mapping1 = GetAXMapping(1);
  ASSERT_EQ(mapping1.size(), 1u);
  EXPECT_EQ(mapping1[0].id, 3);  // Should map to the first occurrence (Node 3)

  // AnchorB (Phase 1)
  auto mapping2 = GetAXMapping(2);
  ASSERT_EQ(mapping2.size(), 1u);
  EXPECT_EQ(mapping2[0].id, 4);
}

TEST_F(ReadAnythingTextAlignerTest,
       MapRenderedTextToTree_GapSubstringAlignment_ShuffledBlockIgnored) {
  // Setup AXTree: [A] [B] Gap [C]
  std::u16string textA = u"AnchorA_________";
  std::u16string textB = u"ShuffledB_______";
  std::u16string textGap = u"GapContent______";
  std::u16string textC = u"AnchorC_________";

  ui::AXTreeUpdate update;
  test::SetUpdateTreeID(&update, tree_id_);
  update.root_id = 1;
  update.nodes = {test::GenericContainerNode(1), test::TextNode(2, textA),
                  test::TextNode(3, textB), test::TextNode(4, textGap),
                  test::TextNode(5, textC)};
  update.nodes[0].child_ids = {2, 3, 4, 5};
  SetTree(update);

  // Setup Distilled blocks : A -> Gap -> C -> B (B is shuffled)
  std::vector<std::u16string> blocks = {textA, textGap, textC, textB};
  MapRenderedTextToTree(blocks);

  // A, C, and B are globally unique -> Mapped in Phase 1.
  // B is shuffled -> Rejected by LIS, so it's NOT a major anchor.
  // Gap between A and C in AXTree is [textB, textGap].
  // GapSubstringAlignment searches this gap for textGap.

  // Verify B remains mapped to its original position (Node 3) even though it
  // was shuffled.
  auto mappingB = GetAXMapping(3);
  ASSERT_EQ(mappingB.size(), 1u);
  EXPECT_EQ(mappingB[0].id, 3);

  // Verify GapContent is mapped to Node 4, NOT Node 3.
  auto mappingGap = GetAXMapping(1);
  ASSERT_EQ(mappingGap.size(), 1u);
  EXPECT_EQ(mappingGap[0].id, 4);
}

TEST_F(ReadAnythingTextAlignerTest,
       MapRenderedTextToTree_GapSubstringAlignment_ShuffledAXTextIgnored) {
  // Scenario: short_block is a substring of long_block.
  std::u16string short_block = u"short_block______";
  std::u16string long_block = u"long_block___________";
  long_block.replace(2, short_block.length(), short_block);

  // Setup AXTree: [A] [short_block][__] [B] [long_block (containing
  // short_block)]
  ui::AXTreeUpdate update;
  test::SetUpdateTreeID(&update, tree_id_);
  update.root_id = 1;
  update.nodes = {test::GenericContainerNode(1),
                  test::TextNode(2, u"BLOCK_A_________"),
                  test::TextNode(3, short_block),
                  test::TextNode(4, u"___"),
                  test::TextNode(5, u"BLOCK_B_________"),
                  test::TextNode(6, long_block)};
  update.nodes[0].child_ids = {2, 3, 4, 5, 6};
  SetTree(update);

  // Distilled order: A -> Long(shuffled) -> Short -> B -> Short(repeated)
  std::vector<std::u16string> blocks = {
      u"BLOCK_A_________",
      long_block,   // Block 1: Shuffled (Node 6)
      short_block,  // Block 2: Gap 1 (maps to Node 3)
      u"BLOCK_B_________",
      short_block  // Block 4: Repeated, won't map in P2
  };
  MapRenderedTextToTree(blocks);

  // Phase 1 Mappings:
  EXPECT_EQ(GetAXMapping(0)[0].id, 2);  // Block A
  EXPECT_EQ(GetAXMapping(1)[0].id, 6);  // Long Shuffle (shuffled)
  EXPECT_EQ(GetAXMapping(3)[0].id, 5);  // Block B

  // GapSubstringAlignment phase Mapping:
  // So Block 2 should map to Node 3(short_block) instead of long_block.
  auto mapping2 = GetAXMapping(2);
  ASSERT_FALSE(mapping2.empty());
  EXPECT_EQ(mapping2[0].id, 3);
}

TEST_F(ReadAnythingTextAlignerTest,
       MapRenderedTextToTree_GapSubstringAlignment_MultiBlockRecursion) {
  // Setup AXTree: [Anchor1] [UniqueC] [UniqueD] [Anchor2]
  std::u16string a1 = u"Anchor1_________";
  std::u16string uc = u"UniqueC_________";
  std::u16string ud = u"UniqueD_________";
  std::u16string a2 = u"Anchor2_________";

  ui::AXTreeUpdate update;
  test::SetUpdateTreeID(&update, tree_id_);
  update.root_id = 1;
  update.nodes = {test::GenericContainerNode(1), test::TextNode(2, a1),
                  test::TextNode(3, uc),         test::TextNode(4, ud),
                  test::TextNode(5, a2),         test::TextNode(6, uc),
                  test::TextNode(7, ud)};
  update.nodes[0].child_ids = {2, 3, 4, 5, 6, 7};
  SetTree(update);

  std::vector<std::u16string> blocks = {a1, uc, ud, a2, uc, ud};
  MapRenderedTextToTree(blocks);

  // All blocks should be mapped.
  EXPECT_EQ(GetAXMapping(0)[0].id, 2);
  EXPECT_EQ(GetAXMapping(1)[0].id, 3);
  EXPECT_EQ(GetAXMapping(2)[0].id, 4);
  EXPECT_EQ(GetAXMapping(3)[0].id, 5);
  EXPECT_EQ(GetAXMapping(4)[0].id, 6);
  EXPECT_EQ(GetAXMapping(5)[0].id, 7);
}

TEST_F(ReadAnythingTextAlignerTest,
       MapRenderedTextToTree_GapSubstringAlignment_SubBlockAnchoring) {
  // Setup AXTree: [StartAnchor] "This is a very long and unique string that
  // should be anchored." [EndAnchor]
  std::u16string unique =
      u"This is a very long and unique string that should be anchored.";
  ui::AXTreeUpdate update;
  test::SetUpdateTreeID(&update, tree_id_);
  update.root_id = 1;
  update.nodes = {
      test::GenericContainerNode(1), test::TextNode(2, u"StartAnchor_____"),
      test::TextNode(3, unique), test::TextNode(4, u"EndAnchor_______")};
  update.nodes[0].child_ids = {2, 3, 4};
  SetTree(update);

  // Distilled block has noise around the unique string.
  std::vector<std::u16string> blocks = {
      u"StartAnchor_____", u"NoisePrefix " + unique + u" NoisePostfix",
      u"EndAnchor_______"};
  MapRenderedTextToTree(blocks);

  auto mapping = GetAXMapping(1);
  // Should have found the unique substring within the noisy block.
  ASSERT_FALSE(mapping.empty());
  EXPECT_EQ(mapping[0].id, 3);
  // Start offset in block should be length of "NoisePrefix " (12)
  EXPECT_EQ(mapping[0].start, 12);
  EXPECT_EQ(mapping[0].end, 12 + (int)unique.length());
}

TEST_F(ReadAnythingTextAlignerTest,
       MapRenderedTextToTree_GapSubstringAlignment_MultiNodeToOneBlock) {
  // Setup AXTree: [Target] [B] [Target]
  std::u16string target = u"Fragmented_Text_Example_Target_";  // 30 chars

  ui::AXTreeUpdate update;
  test::SetUpdateTreeID(&update, tree_id_);
  update.root_id = 1;
  update.nodes = {
      test::GenericContainerNode(1), test::TextNode(2, u"AnchorA_________"),
      // Target split across 3 nodes
      test::TextNode(3, target.substr(0, 10)),
      test::TextNode(4, target.substr(10, 10)),
      test::TextNode(5, target.substr(20)),
      test::TextNode(6, u"AnchorB_________"),
      test::TextNode(7, target)  // Duplicate for Phase 1 skip.
  };
  update.nodes[0].child_ids = {2, 3, 4, 5, 6, 7};
  SetTree(update);

  // Scenario: A single distilled block is split across 3 AX nodes.
  MapRenderedTextToTree(
      {u"AnchorA_________ theme", target, u"AnchorB_________"});

  // Verify 'target' (Block 1) maps to all 3 fragmented nodes (3, 4, 5).
  auto mapping = GetAXMapping(1);
  ASSERT_EQ(mapping.size(), 3u);
  EXPECT_EQ(mapping[0].id, 3);
  EXPECT_EQ(mapping[1].id, 4);
  EXPECT_EQ(mapping[2].id, 5);
}

TEST_F(ReadAnythingTextAlignerTest,
       MapRenderedTextToTree_GapSubstringAlignment_MultiBlockToOneNode) {
  // Setup AXTree: [A] full [B] full
  std::u16string part1 = u"Part_One_Of_Single_Node_";
  std::u16string part2 = u"Part_Two_Of_Single_Node_";
  std::u16string full = part1 + part2;

  ui::AXTreeUpdate update;
  test::SetUpdateTreeID(&update, tree_id_);
  update.root_id = 1;
  update.nodes = {
      test::GenericContainerNode(1), test::TextNode(2, u"AnchorA_________"),
      test::TextNode(3, full),  // Multiple blocks should map here
      test::TextNode(4, u"AnchorB_________"),
      test::TextNode(5, full)  // Duplicate for Phase 1 skip
  };
  update.nodes[0].child_ids = {2, 3, 4, 5};
  SetTree(update);

  // Distilled: [AnchorA] [part1] [part2] [AnchorB]
  std::vector<std::u16string> blocks = {u"AnchorA_________", part1, part2,
                                        u"AnchorB_________"};

  // Scenario: Two distilled blocks map to a single AX node.
  MapRenderedTextToTree(blocks);

  // Block 1 (part1) should map to first half of Node 3.
  auto mapping1 = GetAXMapping(1);
  ASSERT_EQ(mapping1.size(), 1u);
  EXPECT_EQ(mapping1[0].id, 3);
  EXPECT_EQ(mapping1[0].start, 0);
  EXPECT_EQ(mapping1[0].end, 24);

  // Block 2 (part2) should map to second half of Node 3.
  auto mapping2 = GetAXMapping(2);
  ASSERT_EQ(mapping2.size(), 1u);
  EXPECT_EQ(mapping2[0].id, 3);
  EXPECT_EQ(mapping2[0].start, 0);  // Offset within the BLOCK is 0
  EXPECT_EQ(mapping2[0].end, 24);
}

TEST_F(ReadAnythingTextAlignerTest,
       MapRenderedTextToTree_AlignRelativeOrder_SequentialIntegrity) {
  // Setup AXTree: [AnchorA] Duplicate Repeated [AnchorB]
  std::u16string a1 = u"Anchor1_________";
  std::u16string dup = u"RepeatedText";  // Length 12 (< 15)
  std::u16string a2 = u"Anchor2_________";

  ui::AXTreeUpdate update;
  test::SetUpdateTreeID(&update, tree_id_);
  update.root_id = 1;
  update.nodes = {test::GenericContainerNode(1), test::TextNode(2, a1),
                  test::TextNode(3, dup), test::TextNode(4, dup),
                  test::TextNode(5, a2)};
  update.nodes[0].child_ids = {2, 3, 4, 5};
  SetTree(update);

  // Distilled order matches AXTree order.
  std::vector<std::u16string> blocks = {a1, dup, dup, a2};
  MapRenderedTextToTree(blocks);

  // Block 1 should map to node 3
  auto mapping1 = GetAXMapping(1);
  ASSERT_EQ(mapping1.size(), 1u);
  EXPECT_EQ(mapping1[0].id, 3);

  // Block 2 should map to node 4
  auto mapping2 = GetAXMapping(2);
  ASSERT_EQ(mapping2.size(), 1u);
  EXPECT_EQ(mapping2[0].id, 4);
}

TEST_F(ReadAnythingTextAlignerTest,
       MapRenderedTextToTree_AlignRelativeOrder_ShrinkToFit) {
  // Setup AXTree: [AnchorA] "Match." [AnchorB]
  std::u16string a1 = u"Anchor1_________";
  std::u16string dup = u"This is a match.";
  std::u16string a2 = u"Anchor2_________";

  ui::AXTreeUpdate update;
  test::SetUpdateTreeID(&update, tree_id_);
  update.root_id = 1;
  update.nodes = {test::GenericContainerNode(1), test::TextNode(2, a1),
                  test::TextNode(3, dup), test::TextNode(4, dup),
                  test::TextNode(5, a2)};
  update.nodes[0].child_ids = {2, 3, 4, 5};
  SetTree(update);

  // Distilled text has different punctuation ("!" vs ".").
  std::u16string distilled_text = u"This is a match!";
  std::vector<std::u16string> blocks = {a1, distilled_text, distilled_text, a2};
  MapRenderedTextToTree(blocks);

  // Block 1 should map to the portion of node 3 that matches.
  auto mapping = GetAXMapping(1);
  ASSERT_EQ(mapping.size(), 1u);
  EXPECT_EQ(mapping[0].id, 3);
  EXPECT_EQ(mapping[0].start, 0);
  EXPECT_EQ(mapping[0].end, 15);
}

TEST_F(ReadAnythingTextAlignerTest,
       MapRenderedTextToTree_GapSubstringAlignment_MinAnchorLengthBoundary) {
  std::u16string a1 = u"Anchor1_________";
  std::u16string a2 = u"Anchor2_________";

  // In the gap between Anchor1 and Anchor2:
  // - "RepeatedPrefix_" appears twice (nodes 3 and 4), so it is not locally
  //   unique.
  // - "XY" (length 2 < kMinAnchorLength) is locally unique in node 5, but too
  //   short to anchor.
  // - "ABC" (length 3 == kMinAnchorLength) is locally unique in node 6 and
  //   should anchor.
  ui::AXTreeUpdate update;
  test::SetUpdateTreeID(&update, tree_id_);
  update.root_id = 1;
  update.nodes = {test::GenericContainerNode(1),
                  test::TextNode(2, a1),
                  test::TextNode(3, u"RepeatedPrefix_1"),
                  test::TextNode(4, u"RepeatedPrefix_2"),
                  test::TextNode(5, u"XY"),
                  test::TextNode(6, u"ABC"),
                  test::TextNode(7, a2),
                  test::TextNode(8, u"ABC")};
  update.nodes[0].child_ids = {2, 3, 4, 5, 6, 7, 8};
  SetTree(update);

  std::vector<std::u16string> blocks = {
      a1,
      u"RepeatedPrefix_XY",
      u"Noise_ABC_Noise",
      a2,
  };
  MapRenderedTextToTree(blocks);

  // Block 1 ("RepeatedPrefix_XY") should ignore the 2-char locally unique "XY"
  // and map the 15-char "RepeatedPrefix_" via AlignRelativeOrder to node 3.
  auto mapping1 = GetAXMapping(1);
  ASSERT_EQ(mapping1.size(), 1u);
  EXPECT_EQ(mapping1[0].id, 3);
  EXPECT_EQ(mapping1[0].start, 0);
  EXPECT_EQ(mapping1[0].end, 15);

  // Block 2 ("Noise_ABC_Noise") should anchor the 3-char "ABC" to node 6.
  auto mapping2 = GetAXMapping(2);
  ASSERT_EQ(mapping2.size(), 1u);
  EXPECT_EQ(mapping2[0].id, 6);
  EXPECT_EQ(mapping2[0].start, 6);
  EXPECT_EQ(mapping2[0].end, 9);
}

TEST_F(
    ReadAnythingTextAlignerTest,
    MapRenderedTextToTree_GapSubstringAlignment_MergedShuffledOccupiedRanges) {
  std::u16string title = u"Title___________";
  std::u16string a1 = u"Anchor1_________";
  std::u16string shuffled1 = u"Shuffled1_Shared";
  std::u16string shuffled2 = u"Part_Shuffled2";
  std::u16string gap_target = u"SharedPart";
  std::u16string a2 = u"Anchor2_________";

  // In the AXTree, shuffled1 (node 4) and shuffled2 (node 5) are adjacent, so
  // their boundary forms "SharedPart", while node 6 immediately follows node 5
  // with an unoccupied "SharedPart".
  ui::AXTreeUpdate update;
  test::SetUpdateTreeID(&update, tree_id_);
  update.root_id = 1;
  update.nodes = {test::GenericContainerNode(1), test::TextNode(2, title),
                  test::TextNode(3, a1),         test::TextNode(4, shuffled1),
                  test::TextNode(5, shuffled2),  test::TextNode(6, gap_target),
                  test::TextNode(7, a2),         test::TextNode(8, gap_target)};
  update.nodes[0].child_ids = {2, 3, 4, 5, 6, 7, 8};
  SetTree(update);

  // Place shuffled2 and shuffled1 (in reverse AX order) before a1 so their
  // occupied ranges are recorded out of order before sorting and merging,
  // while [a1, a2] is the unique longest increasing anchor subsequence.
  std::vector<std::u16string> blocks = {
      title, shuffled2, shuffled1, a1, gap_target, a2,
  };
  MapRenderedTextToTree(blocks);

  // "SharedPart" (Block 4) must skip the occupied cross-boundary match in
  // nodes 4+5 and map to the immediately adjacent unoccupied node 6.
  auto mapping = GetAXMapping(4);
  ASSERT_EQ(mapping.size(), 1u);
  EXPECT_EQ(mapping[0].id, 6);
  EXPECT_EQ(mapping[0].start, 0);
  EXPECT_EQ(mapping[0].end, 10);
}

TEST_F(
    ReadAnythingTextAlignerTest,
    MapRenderedTextToTree_AlignRelativeOrder_BinarySearchAndMinLengthBoundary) {
  std::u16string a1 = u"Anchor1_________________________________________";
  std::u16string a2 = u"Anchor2_________________________________________";

  // Duplicate nodes in the gap ensure no substring in the gap is locally
  // unique, forcing AlignSubstring to fall back to AlignRelativeOrder.
  std::u16string dup_mid = u"RepeatedMatch_";
  std::u16string dup_five = u"Five5";
  std::u16string dup_four = u"Four";

  ui::AXTreeUpdate update;
  test::SetUpdateTreeID(&update, tree_id_);
  update.root_id = 1;
  update.nodes = {test::GenericContainerNode(1), test::TextNode(2, a1),
                  test::TextNode(3, dup_mid),    test::TextNode(4, dup_mid),
                  test::TextNode(5, dup_five),   test::TextNode(6, dup_five),
                  test::TextNode(7, dup_four),   test::TextNode(8, dup_four),
                  test::TextNode(9, a2)};
  update.nodes[0].child_ids = {2, 3, 4, 5, 6, 7, 8, 9};
  SetTree(update);

  std::vector<std::u16string> blocks = {
      a1,
      dup_mid + u"UnmatchedLongSuffix",
      dup_five + u"UnmatchedTail",
      dup_four + u"UnmatchedTail",
      a2,
  };
  MapRenderedTextToTree(blocks);

  // Block 1 binary-searches down to the 14-char prefix in node 3.
  auto mapping1 = GetAXMapping(1);
  ASSERT_EQ(mapping1.size(), 1u);
  EXPECT_EQ(mapping1[0].id, 3);
  EXPECT_EQ(mapping1[0].start, 0);
  EXPECT_EQ(mapping1[0].end, 14);

  // Block 2 matches at the exact kMinSequentialMatchLength (5) boundary in
  // node 5.
  auto mapping2 = GetAXMapping(2);
  ASSERT_EQ(mapping2.size(), 1u);
  EXPECT_EQ(mapping2[0].id, 5);
  EXPECT_EQ(mapping2[0].start, 0);
  EXPECT_EQ(mapping2[0].end, 5);

  // Block 3 only matches 4 chars (< kMinSequentialMatchLength) and must remain
  // unmapped.
  EXPECT_TRUE(GetAXMapping(3).empty());
}
