// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/accessibility/read_anything/read_anything_app_model.h"

#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/threading/platform_thread.h"
#include "chrome/renderer/accessibility/read_anything/read_anything_node_utils.h"
#include "chrome/renderer/accessibility/read_anything/read_anything_test_utils.h"
#include "chrome/test/base/chrome_render_view_test.h"
#include "read_anything_test_utils.h"
#include "services/strings/grit/services_strings.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/accessibility/ax_enums.mojom-shared.h"
#include "ui/accessibility/ax_event.h"
#include "ui/accessibility/ax_node_id_forward.h"
#include "ui/accessibility/ax_serializable_tree.h"
#include "ui/accessibility/ax_updates_and_events.h"
#include "ui/base/l10n/l10n_util.h"

using ::testing::ElementsAre;

class ReadAnythingAppModelTest : public ChromeRenderViewTest {
 public:
  ReadAnythingAppModelTest() = default;
  ~ReadAnythingAppModelTest() override = default;
  ReadAnythingAppModelTest(const ReadAnythingAppModelTest&) = delete;
  ReadAnythingAppModelTest& operator=(const ReadAnythingAppModelTest&) = delete;

  void SetUp() override {
    ChromeRenderViewTest::SetUp();
    model_ = std::make_unique<ReadAnythingAppModel>();

    // Create a tree id.
    tree_id_ = ui::AXTreeID::CreateNewAXTreeID();

    // Create simple AXTreeUpdate with a root node and 3 children.
    std::unique_ptr<ui::AXTreeUpdate> snapshot = test::CreateInitialUpdate();
    test::SetUpdateTreeID(snapshot.get(), tree_id_);

    AccessibilityEventReceived({*snapshot});
    model().SetActiveTreeId(tree_id_);
    model().Reset({});
  }

  void SetUpWithoutInitialization() {
    model_ = std::make_unique<ReadAnythingAppModel>();
  }

  ReadAnythingAppModel& model() { return *model_; }
  const ReadAnythingAppModel& model() const { return *model_; }

  bool AreAllPendingUpdatesEmpty() {
    size_t count = 0;
    for (auto const& [tree_id, updates] :
         model_->GetPendingUpdatesForTesting()) {
      count += updates.size();
    }
    return count == 0;
  }

  void AccessibilityEventReceived(const std::vector<ui::AXTreeUpdate>& updates,
                                  bool speech_playing = false) {
    AccessibilityEventReceived(updates[0].tree_data.tree_id, updates,
                               speech_playing);
  }

  void AccessibilityEventReceived(const ui::AXTreeID& tree_id,
                                  const std::vector<ui::AXTreeUpdate>& updates,
                                  bool speech_playing = false) {
    std::vector<ui::AXEvent> events;
    model_->AccessibilityEventReceived(
        tree_id, const_cast<std::vector<ui::AXTreeUpdate>&>(updates), events,
        speech_playing);
  }

  void ProcessDisplayNodes(const std::vector<ui::AXNodeID>& content_node_ids) {
    model().Reset(content_node_ids);
    model_->ComputeDisplayNodeIdsForDistilledTree();
  }

  std::vector<int> SendSimpleUpdateAndGetChildIds() {
    // Set the name of each node to be its id.
    ui::AXTreeUpdate initial_update;
    test::SetUpdateTreeID(&initial_update, tree_id_);
    initial_update.root_id = 1;
    initial_update.nodes.resize(3);
    std::vector<int> child_ids;
    for (int i = 0; i < 3; i++) {
      int id = i + 2;
      child_ids.push_back(id);
      initial_update.nodes[i] = test::TextNodeWithTextFromId(id);
    }
    AccessibilityEventReceived({initial_update});
    return child_ids;
  }

  ui::AXTreeID tree_id_;

 private:
  std::unique_ptr<ReadAnythingAppModel> model_ = nullptr;
};

TEST_F(ReadAnythingAppModelTest, IsDocs_FalseBeforeTreeInitialization) {
  EXPECT_FALSE(model().IsDocs());
  SetUpWithoutInitialization();
  EXPECT_FALSE(model().IsDocs());
}

TEST_F(ReadAnythingAppModelTest, FontName) {
  EXPECT_NE(model().font_name(), std::string());

  std::string font_name = "Montserrat";
  model().set_font_name(font_name);
  EXPECT_EQ(font_name, model().font_name());
}

TEST_F(ReadAnythingAppModelTest, OnSettingsRestoredFromPrefs) {
  auto line_spacing = read_anything::mojom::LineSpacing::kDefaultValue;
  auto letter_spacing = read_anything::mojom::LetterSpacing::kDefaultValue;
  std::string font_name = "Roboto";
  double font_size = 3.0;
  bool links_enabled = false;
  bool images_enabled = true;
  auto color = read_anything::mojom::Colors::kDefaultValue;
  int color_value = 0;

  model().OnSettingsRestoredFromPrefs(line_spacing, letter_spacing, font_name,
                                      font_size, links_enabled, images_enabled,
                                      color);

  EXPECT_EQ(static_cast<int>(line_spacing), model().line_spacing());
  EXPECT_EQ(static_cast<int>(letter_spacing), model().letter_spacing());
  EXPECT_EQ(font_name, model().font_name());
  EXPECT_EQ(font_size, model().font_size());
  EXPECT_EQ(links_enabled, model().links_enabled());
  EXPECT_EQ(images_enabled, model().images_enabled());
  EXPECT_EQ(color_value, model().color_theme());
}

TEST_F(ReadAnythingAppModelTest, IsNodeIgnoredForReadAnything) {
  ui::AXTreeUpdate update;
  test::SetUpdateTreeID(&update, tree_id_);
  ui::AXNodeData static_text_node = test::TextNode(/* id = */ 2);

  ui::AXNodeData combobox_node;
  combobox_node.id = 3;
  combobox_node.role = ax::mojom::Role::kComboBoxGrouping;

  ui::AXNodeData button_node;
  button_node.id = 4;
  button_node.role = ax::mojom::Role::kButton;
  update.nodes = {static_text_node, combobox_node, button_node};

  AccessibilityEventReceived({update});
  EXPECT_EQ(false, a11y::IsNodeIgnoredForReadAnything(model().GetAXNode(2),
                                                      model().is_pdf()));
  EXPECT_EQ(true, a11y::IsNodeIgnoredForReadAnything(model().GetAXNode(3),
                                                     model().is_pdf()));
  EXPECT_EQ(true, a11y::IsNodeIgnoredForReadAnything(model().GetAXNode(4),
                                                     model().is_pdf()));
}

TEST_F(ReadAnythingAppModelTest,
       IsNodeIgnoredForReadAnything_TextFieldsNotIgnored) {
  ui::AXTreeUpdate update;
  test::SetUpdateTreeID(&update, tree_id_);
  ui::AXNodeData tree_node;
  tree_node.id = 2;
  tree_node.role = ax::mojom::Role::kTree;

  ui::AXNodeData textfield_with_combobox_node;
  textfield_with_combobox_node.id = 3;
  textfield_with_combobox_node.role = ax::mojom::Role::kTextFieldWithComboBox;

  ui::AXNodeData textfield_node;
  textfield_node.id = 4;
  textfield_node.role = ax::mojom::Role::kTextField;
  update.nodes = {tree_node, textfield_with_combobox_node, textfield_node};

  AccessibilityEventReceived({update});
  EXPECT_EQ(true, a11y::IsNodeIgnoredForReadAnything(model().GetAXNode(2),
                                                     model().is_pdf()));
  EXPECT_EQ(false, a11y::IsNodeIgnoredForReadAnything(model().GetAXNode(3),
                                                      model().is_pdf()));
  EXPECT_EQ(false, a11y::IsNodeIgnoredForReadAnything(model().GetAXNode(4),
                                                      model().is_pdf()));
}

TEST_F(ReadAnythingAppModelTest,
       IsNodeIgnoredForReadAnything_InaccessiblePDFPageNodes) {
  model().set_is_pdf(true);

  // PDF OCR output contains kBanner and kContentInfo (each with a static text
  // node child) to mark page start/end.
  ui::AXTreeUpdate update;
  test::SetUpdateTreeID(&update, tree_id_);
  ui::AXNodeData banner_node;
  banner_node.id = 2;
  banner_node.role = ax::mojom::Role::kBanner;

  ui::AXNodeData static_text_start_node = test::TextNode(
      /* id= */ 3, l10n_util::GetStringUTF16(IDS_PDF_OCR_RESULT_BEGIN));
  banner_node.child_ids = {static_text_start_node.id};

  ui::AXNodeData content_info_node;
  content_info_node.id = 4;
  content_info_node.role = ax::mojom::Role::kContentInfo;

  ui::AXNodeData static_text_end_node = test::TextNode(
      /* id= */ 5, l10n_util::GetStringUTF16(IDS_PDF_OCR_RESULT_END));
  content_info_node.child_ids = {static_text_end_node.id};

  ui::AXNodeData root;
  root.id = 1;
  root.child_ids = {banner_node.id, content_info_node.id};
  root.role = ax::mojom::Role::kPdfRoot;
  update.root_id = root.id;
  update.nodes = {root, banner_node, static_text_start_node, content_info_node,
                  static_text_end_node};

  AccessibilityEventReceived({update});
  EXPECT_EQ(true, a11y::IsNodeIgnoredForReadAnything(model().GetAXNode(2),
                                                     model().is_pdf()));
  EXPECT_EQ(true, a11y::IsNodeIgnoredForReadAnything(model().GetAXNode(3),
                                                     model().is_pdf()));
  EXPECT_EQ(false, a11y::IsNodeIgnoredForReadAnything(model().GetAXNode(4),
                                                      model().is_pdf()));
  EXPECT_EQ(true, a11y::IsNodeIgnoredForReadAnything(model().GetAXNode(5),
                                                     model().is_pdf()));
}

TEST_F(ReadAnythingAppModelTest, ModelUpdatesTreeState) {
  // Set up trees.
  ui::AXTreeID tree_id_2 = ui::AXTreeID::CreateNewAXTreeID();
  ui::AXTreeID tree_id_3 = ui::AXTreeID::CreateNewAXTreeID();

  model().AddTree(tree_id_2, std::make_unique<ui::AXSerializableTree>());
  model().AddTree(tree_id_3, std::make_unique<ui::AXSerializableTree>());

  ASSERT_EQ(3u, model().GetTreesForTesting()->size());
  ASSERT_TRUE(model().ContainsTree(tree_id_2));
  ASSERT_TRUE(model().ContainsTree(tree_id_3));
  ASSERT_TRUE(model().ContainsTree(tree_id_));

  // Remove one tree.
  model().EraseTreeForTesting(tree_id_2);
  ASSERT_EQ(2u, model().GetTreesForTesting()->size());
  ASSERT_TRUE(model().ContainsTree(tree_id_3));
  ASSERT_FALSE(model().ContainsTree(tree_id_2));
  ASSERT_TRUE(model().ContainsTree(tree_id_));

  // Remove the second tree.
  model().EraseTreeForTesting(tree_id_);
  ASSERT_EQ(1u, model().GetTreesForTesting()->size());
  ASSERT_TRUE(model().ContainsTree(tree_id_3));
  ASSERT_FALSE(model().ContainsTree(tree_id_2));
  ASSERT_FALSE(model().ContainsTree(tree_id_));

  // Remove the last tree.
  model().EraseTreeForTesting(tree_id_3);
  ASSERT_EQ(0u, model().GetTreesForTesting()->size());
  ASSERT_FALSE(model().ContainsTree(tree_id_3));
  ASSERT_FALSE(model().ContainsTree(tree_id_2));
  ASSERT_FALSE(model().ContainsTree(tree_id_));
}

TEST_F(ReadAnythingAppModelTest, AddAndRemoveTrees) {
  // Create two new trees with new tree IDs.
  std::vector<ui::AXTreeID> tree_ids = {ui::AXTreeID::CreateNewAXTreeID(),
                                        ui::AXTreeID::CreateNewAXTreeID()};
  std::vector<ui::AXTreeUpdate> updates;
  for (int i = 0; i < 2; i++) {
    ui::AXTreeUpdate update;
    test::SetUpdateTreeID(&update, tree_ids[i]);
    ui::AXNodeData node;
    node.id = 1;
    update.nodes = {node};
    update.root_id = node.id;
    updates.push_back(update);
  }

  // Start with 1 tree (the tree created in SetUp).
  ASSERT_EQ(1u, model().GetTreesForTesting()->size());
  ASSERT_TRUE(model().ContainsTree(tree_id_));

  // Add the two trees.
  AccessibilityEventReceived({updates[0]});
  ASSERT_EQ(2u, model().GetTreesForTesting()->size());
  ASSERT_TRUE(model().ContainsTree(tree_id_));
  ASSERT_TRUE(model().ContainsTree(tree_ids[0]));
  AccessibilityEventReceived({updates[1]});
  ASSERT_EQ(3u, model().GetTreesForTesting()->size());
  ASSERT_TRUE(model().ContainsTree(tree_id_));
  ASSERT_TRUE(model().ContainsTree(tree_ids[0]));
  ASSERT_TRUE(model().ContainsTree(tree_ids[1]));

  // Remove all of the trees.
  model().EraseTreeForTesting(tree_id_);
  ASSERT_EQ(2u, model().GetTreesForTesting()->size());
  ASSERT_TRUE(model().ContainsTree(tree_ids[0]));
  ASSERT_TRUE(model().ContainsTree(tree_ids[1]));
  model().EraseTreeForTesting(tree_ids[0]);
  ASSERT_EQ(1u, model().GetTreesForTesting()->size());
  ASSERT_TRUE(model().ContainsTree(tree_ids[1]));
  model().EraseTreeForTesting(tree_ids[1]);
  ASSERT_EQ(0u, model().GetTreesForTesting()->size());
}

TEST_F(ReadAnythingAppModelTest,
       DistillationInProgress_TreeUpdateReceivedOnInactiveTree) {
  EXPECT_EQ(0u, model().GetPendingUpdatesForTesting()[tree_id_].size());

  // Create a new tree.
  ui::AXTreeID tree_id_2 = ui::AXTreeID::CreateNewAXTreeID();
  ui::AXTreeUpdate update_2;
  test::SetUpdateTreeID(&update_2, tree_id_2);
  ui::AXNodeData node;
  node.id = 1;
  update_2.root_id = node.id;
  update_2.nodes = {node};

  // Updates on inactive trees are processed immediately and are not marked as
  // pending.
  AccessibilityEventReceived({update_2});
  EXPECT_EQ(0u, model().GetPendingUpdatesForTesting()[tree_id_].size());
}

TEST_F(ReadAnythingAppModelTest,
       AddPendingUpdatesAfterUnserializingOnSameTree_DoesNotCrash) {
  std::vector<int> child_ids = SendSimpleUpdateAndGetChildIds();
  std::vector<ui::AXTreeUpdate> updates =
      test::CreateSimpleUpdateList(child_ids, tree_id_);

  // Send update 0, which starts distillation.
  AccessibilityEventReceived({updates[0]});
  EXPECT_EQ(0u, model().GetPendingUpdatesForTesting()[tree_id_].size());
  ASSERT_TRUE(AreAllPendingUpdatesEmpty());

  // Send update 1. Since distillation is in progress, this will not be
  // unserialized yet.
  model().set_distillation_in_progress(true);
  AccessibilityEventReceived({updates[1]});
  EXPECT_EQ(1u, model().GetPendingUpdatesForTesting()[tree_id_].size());

  // Ensure that there are no crashes after an accessibility event is received
  // immediately after unserializing.
  model().UnserializePendingUpdates(tree_id_);
  model().set_distillation_in_progress(true);
  AccessibilityEventReceived({updates[2]});
  EXPECT_EQ(1u, model().GetPendingUpdatesForTesting()[tree_id_].size());
  ASSERT_FALSE(AreAllPendingUpdatesEmpty());
}

TEST_F(ReadAnythingAppModelTest, OnTreeErased_ClearsPendingUpdates) {
  std::vector<int> child_ids = SendSimpleUpdateAndGetChildIds();
  std::vector<ui::AXTreeUpdate> updates =
      test::CreateSimpleUpdateList(child_ids, tree_id_);

  // Send update 0, which starts distillation.
  AccessibilityEventReceived({updates[0]});
  EXPECT_EQ(0u, model().GetPendingUpdatesForTesting()[tree_id_].size());
  ASSERT_TRUE(AreAllPendingUpdatesEmpty());

  // Send update 1. Since distillation is in progress, this will not be
  // unserialized yet.
  model().set_distillation_in_progress(true);
  AccessibilityEventReceived({updates[1]});
  EXPECT_EQ(1u, model().GetPendingUpdatesForTesting()[tree_id_].size());

  // Destroy the tree.
  model().EraseTreeForTesting(tree_id_);
  EXPECT_EQ(0u, model().GetPendingUpdatesForTesting()[tree_id_].size());
}

TEST_F(ReadAnythingAppModelTest,
       DistillationInProgress_TreeUpdateReceivedOnActiveTree) {
  std::vector<int> child_ids = SendSimpleUpdateAndGetChildIds();
  std::vector<ui::AXTreeUpdate> updates =
      test::CreateSimpleUpdateList(child_ids, tree_id_);

  // Send update 0, which starts distillation.
  AccessibilityEventReceived({updates[0]});
  EXPECT_EQ(0u, model().GetPendingUpdatesForTesting()[tree_id_].size());
  ASSERT_TRUE(AreAllPendingUpdatesEmpty());

  // Send update 1. Since distillation is in progress, this will not be
  // unserialized yet.
  model().set_distillation_in_progress(true);
  AccessibilityEventReceived({updates[1]});
  EXPECT_EQ(1u, model().GetPendingUpdatesForTesting()[tree_id_].size());

  // Send update 2. This is still not unserialized yet.
  AccessibilityEventReceived({updates[2]});
  EXPECT_EQ(2u, model().GetPendingUpdatesForTesting()[tree_id_].size());

  // Complete distillation which unserializes the pending updates and distills
  // them.
  model().UnserializePendingUpdates(tree_id_);
  EXPECT_EQ(0u, model().GetPendingUpdatesForTesting()[tree_id_].size());
  ASSERT_TRUE(AreAllPendingUpdatesEmpty());
}

TEST_F(ReadAnythingAppModelTest, SpeechPlaying_TreeUpdateReceivedOnActiveTree) {
  std::vector<int> child_ids = SendSimpleUpdateAndGetChildIds();
  std::vector<ui::AXTreeUpdate> updates =
      test::CreateSimpleUpdateList(child_ids, tree_id_);

  // Send update 0, which starts distillation.
  AccessibilityEventReceived({updates[0]});
  EXPECT_EQ(0u, model().GetPendingUpdatesForTesting()[tree_id_].size());
  ASSERT_TRUE(AreAllPendingUpdatesEmpty());

  // Send update 1. Since speech is in progress, this will not be
  // unserialized yet.
  AccessibilityEventReceived({updates[1]}, /*speech_playing=*/true);
  EXPECT_EQ(1u, model().GetPendingUpdatesForTesting()[tree_id_].size());

  // Send update 2. This is still not unserialized yet.
  AccessibilityEventReceived({updates[2]}, /*speech_playing=*/true);
  EXPECT_EQ(2u, model().GetPendingUpdatesForTesting()[tree_id_].size());

  // Complete distillation which unserializes the pending updates and distills
  // them.
  model().UnserializePendingUpdates(tree_id_);
  EXPECT_EQ(0u, model().GetPendingUpdatesForTesting()[tree_id_].size());
  ASSERT_TRUE(AreAllPendingUpdatesEmpty());
}

TEST_F(ReadAnythingAppModelTest, ClearPendingUpdates_DeletesPendingUpdates) {
  EXPECT_EQ(0u, model().GetPendingUpdatesForTesting()[tree_id_].size());

  // Create a couple of updates which add additional nodes to the tree.
  std::vector<int> child_ids = {2, 3, 4};
  std::vector<ui::AXTreeUpdate> updates =
      test::CreateSimpleUpdateList(child_ids, tree_id_);

  AccessibilityEventReceived({updates[0]});
  EXPECT_EQ(0u, model().GetPendingUpdatesForTesting()[tree_id_].size());
  model().set_distillation_in_progress(true);
  AccessibilityEventReceived({updates[1]});
  EXPECT_EQ(1u, model().GetPendingUpdatesForTesting()[tree_id_].size());
  AccessibilityEventReceived({updates[2]});
  EXPECT_EQ(2u, model().GetPendingUpdatesForTesting()[tree_id_].size());

  // Clearing the pending updates correctly deletes the pending updates.
  model().ClearPendingUpdates();
  ASSERT_TRUE(AreAllPendingUpdatesEmpty());
}

TEST_F(ReadAnythingAppModelTest, ChangeActiveTreeWithPendingUpdates_UnknownID) {
  EXPECT_EQ(0u, model().GetPendingUpdatesForTesting()[tree_id_].size());
  ASSERT_TRUE(AreAllPendingUpdatesEmpty());

  // Create a couple of updates which add additional nodes to the tree.
  std::vector<int> child_ids = {2, 3, 4};
  std::vector<ui::AXTreeUpdate> updates =
      test::CreateSimpleUpdateList(child_ids, tree_id_);

  // Create an update which has no tree id.
  ui::AXTreeUpdate update;
  ui::AXNodeData node = test::GenericContainerNode(/* id= */ 1);
  update.nodes = {node};
  updates.push_back(update);

  // Add the three updates.
  AccessibilityEventReceived({updates[0]});
  EXPECT_EQ(0u, model().GetPendingUpdatesForTesting()[tree_id_].size());
  ASSERT_TRUE(AreAllPendingUpdatesEmpty());
  model().set_distillation_in_progress(true);
  AccessibilityEventReceived(tree_id_, {updates[1], updates[2]});
  EXPECT_EQ(2u, model().GetPendingUpdatesForTesting()[tree_id_].size());

  // Switch to a new active tree. Should not crash.
  model().SetActiveTreeId(ui::AXTreeIDUnknown());
}

TEST_F(ReadAnythingAppModelTest, DisplayNodeIdsContains_ContentNodes) {
  ui::AXTreeUpdate update;
  test::SetUpdateTreeID(&update, tree_id_);
  ui::AXNodeData node1;
  node1.id = 5;

  ui::AXNodeData node2;
  node2.id = 6;

  ui::AXNodeData parent_node;
  parent_node.id = 4;
  parent_node.child_ids = {node1.id, node2.id};
  update.nodes = {parent_node, node1, node2};

  // This update changes the structure of the tree. When the controller receives
  // it in AccessibilityEventReceived, it will re-distill the tree.
  AccessibilityEventReceived({update});
  ProcessDisplayNodes({3, 4});
  EXPECT_TRUE(base::Contains(model().display_node_ids(), 1));
  EXPECT_FALSE(base::Contains(model().display_node_ids(), 2));
  EXPECT_TRUE(base::Contains(model().display_node_ids(), 3));
  EXPECT_TRUE(base::Contains(model().display_node_ids(), 4));
  EXPECT_TRUE(base::Contains(model().display_node_ids(), 5));
  EXPECT_TRUE(base::Contains(model().display_node_ids(), 6));
}

TEST_F(ReadAnythingAppModelTest,
       DisplayNodeIdsDoesNotContain_InvisibleOrIgnoredNodes) {
  ui::AXTreeUpdate update;
  test::SetUpdateTreeID(&update, tree_id_);
  update.nodes.resize(3);
  update.nodes[0].id = 2;
  update.nodes[1].id = 3;
  update.nodes[1].AddState(ax::mojom::State::kInvisible);
  update.nodes[2].id = 4;
  update.nodes[2].AddState(ax::mojom::State::kIgnored);
  AccessibilityEventReceived({update});
  ProcessDisplayNodes({2, 3, 4});
  EXPECT_TRUE(base::Contains(model().display_node_ids(), 1));
  EXPECT_TRUE(base::Contains(model().display_node_ids(), 2));
  EXPECT_FALSE(base::Contains(model().display_node_ids(), 3));
  EXPECT_FALSE(base::Contains(model().display_node_ids(), 4));
}

TEST_F(ReadAnythingAppModelTest,
       DisplayNodeIdsEmpty_WhenContentNodesAreAllHeadings) {
  ui::AXTreeUpdate update;
  test::SetUpdateTreeID(&update, tree_id_);

  // All content nodes are heading nodes.
  update.nodes.resize(3);
  update.nodes[0].id = 2;
  update.nodes[0].role = ax::mojom::Role::kHeading;
  update.nodes[1].id = 3;
  update.nodes[1].role = ax::mojom::Role::kHeading;
  update.nodes[2].id = 4;
  update.nodes[2].role = ax::mojom::Role::kHeading;
  AccessibilityEventReceived({update});
  ProcessDisplayNodes({2, 3, 4});
  EXPECT_TRUE(model().display_node_ids().empty());

  // Content node is static text node with heading parent.
  update.nodes.resize(3);
  update.nodes[0].id = 1;
  update.nodes[0].child_ids = {2};
  update.nodes[1].id = 2;
  update.nodes[1].role = ax::mojom::Role::kHeading;
  update.nodes[1].child_ids = {3};
  update.nodes[2] = test::TextNode(/* id= */ 3);
  AccessibilityEventReceived({update});
  ProcessDisplayNodes({3});
  EXPECT_TRUE(model().display_node_ids().empty());

  // Content node is inline text box with heading grandparent.
  update.nodes.resize(4);
  update.nodes[0].id = 1;
  update.nodes[0].child_ids = {2};
  update.nodes[1].id = 2;
  update.nodes[1].role = ax::mojom::Role::kHeading;
  update.nodes[1].child_ids = {3};
  update.nodes[2] = test::TextNode(/* id= */ 3);
  update.nodes[2].child_ids = {4};
  update.nodes[3].id = 4;
  update.nodes[3].role = ax::mojom::Role::kInlineTextBox;
  AccessibilityEventReceived({update});
  ProcessDisplayNodes({4});
  EXPECT_TRUE(model().display_node_ids().empty());
}

TEST_F(ReadAnythingAppModelTest,
       SelectionNodeIdsContains_SelectionAndNearbyNodes) {
  ui::AXTreeUpdate update;
  test::SetUpdateTreeID(&update, tree_id_);
  update.tree_data.sel_anchor_object_id = 2;
  update.tree_data.sel_focus_object_id = 3;
  update.tree_data.sel_anchor_offset = 0;
  update.tree_data.sel_focus_offset = 0;
  update.tree_data.sel_is_backward = false;

  AccessibilityEventReceived({update});
  model().PostProcessSelection();
  EXPECT_TRUE(base::Contains(model().selection_node_ids(), 1));
  EXPECT_TRUE(base::Contains(model().selection_node_ids(), 2));
  EXPECT_TRUE(base::Contains(model().selection_node_ids(), 3));
  EXPECT_TRUE(base::Contains(model().selection_node_ids(), 4));
}

TEST_F(ReadAnythingAppModelTest,
       SelectionNodeIdsContains_BackwardSelectionAndNearbyNodes) {
  ui::AXTreeUpdate update;
  test::SetUpdateTreeID(&update, tree_id_);
  update.tree_data.sel_anchor_object_id = 3;
  update.tree_data.sel_focus_object_id = 2;
  update.tree_data.sel_anchor_offset = 0;
  update.tree_data.sel_focus_offset = 0;
  update.tree_data.sel_is_backward = true;
  AccessibilityEventReceived({update});
  model().PostProcessSelection();
  EXPECT_TRUE(base::Contains(model().selection_node_ids(), 1));
  EXPECT_TRUE(base::Contains(model().selection_node_ids(), 2));
  EXPECT_TRUE(base::Contains(model().selection_node_ids(), 3));
  EXPECT_TRUE(base::Contains(model().selection_node_ids(), 4));
}

TEST_F(ReadAnythingAppModelTest,
       SelectionNodeIdsDoesNotContain_InvisibleOrIgnoredNodes) {
  ui::AXTreeUpdate update;
  test::SetUpdateTreeID(&update, tree_id_);
  update.nodes.resize(3);
  update.nodes[0].id = 2;
  update.nodes[1].id = 3;
  update.nodes[1].AddState(ax::mojom::State::kInvisible);
  update.nodes[2].id = 4;
  update.nodes[2].AddState(ax::mojom::State::kIgnored);
  update.tree_data.sel_anchor_object_id = 2;
  update.tree_data.sel_focus_object_id = 4;
  update.tree_data.sel_anchor_offset = 0;
  update.tree_data.sel_focus_offset = 0;
  update.tree_data.sel_is_backward = false;

  AccessibilityEventReceived({update});
  model().PostProcessSelection();
  EXPECT_FALSE(base::Contains(model().display_node_ids(), 1));
  EXPECT_FALSE(base::Contains(model().selection_node_ids(), 2));
  EXPECT_FALSE(base::Contains(model().selection_node_ids(), 3));
  EXPECT_FALSE(base::Contains(model().selection_node_ids(), 4));
}

TEST_F(ReadAnythingAppModelTest, Reset_ResetsState) {
  // Initial state.
  ui::AXTreeUpdate update;
  test::SetUpdateTreeID(&update, tree_id_);
  ui::AXNodeData node1;
  node1.id = 5;

  ui::AXNodeData node2;
  node2.id = 6;

  ui::AXNodeData root;
  root.id = 4;
  root.child_ids = {node1.id, node2.id};
  update.nodes = {root, node1, node2};

  AccessibilityEventReceived({update});
  ProcessDisplayNodes({3, 4});
  model().set_distillation_in_progress(true);

  // Assert initial state before model().Resetting.
  ASSERT_TRUE(model().distillation_in_progress());

  ASSERT_TRUE(base::Contains(model().display_node_ids(), 1));
  ASSERT_TRUE(base::Contains(model().display_node_ids(), 3));
  ASSERT_TRUE(base::Contains(model().display_node_ids(), 4));
  ASSERT_TRUE(base::Contains(model().display_node_ids(), 5));
  ASSERT_TRUE(base::Contains(model().display_node_ids(), 6));

  model().Reset({1, 2});

  // Assert model().Reset state.
  ASSERT_FALSE(model().distillation_in_progress());

  ASSERT_TRUE(base::Contains(model().content_node_ids(), 1));
  ASSERT_TRUE(base::Contains(model().content_node_ids(), 2));

  ASSERT_FALSE(base::Contains(model().display_node_ids(), 1));
  ASSERT_FALSE(base::Contains(model().display_node_ids(), 3));
  ASSERT_FALSE(base::Contains(model().display_node_ids(), 4));
  ASSERT_FALSE(base::Contains(model().display_node_ids(), 5));
  ASSERT_FALSE(base::Contains(model().display_node_ids(), 6));

  // Calling model().Reset with different content nodes updates the content
  // nodes.
  model().Reset({5, 4});
  ASSERT_FALSE(base::Contains(model().content_node_ids(), 1));
  ASSERT_FALSE(base::Contains(model().content_node_ids(), 2));
  ASSERT_TRUE(base::Contains(model().content_node_ids(), 5));
  ASSERT_TRUE(base::Contains(model().content_node_ids(), 4));
}

TEST_F(ReadAnythingAppModelTest, Reset_ResetsSelectionState) {
  // Initial state.
  ui::AXTreeUpdate update;
  test::SetUpdateTreeID(&update, tree_id_);
  update.tree_data.sel_anchor_object_id = 3;
  update.tree_data.sel_focus_object_id = 2;
  update.tree_data.sel_anchor_offset = 0;
  update.tree_data.sel_focus_offset = 0;
  update.tree_data.sel_is_backward = true;
  AccessibilityEventReceived({update});
  model().PostProcessSelection();

  // Assert initial selection state.
  ASSERT_TRUE(base::Contains(model().selection_node_ids(), 1));
  ASSERT_TRUE(base::Contains(model().selection_node_ids(), 2));
  ASSERT_TRUE(base::Contains(model().selection_node_ids(), 3));

  ASSERT_TRUE(model().has_selection());

  ASSERT_NE(model().start_offset(), -1);
  ASSERT_NE(model().end_offset(), -1);

  ASSERT_NE(model().start_node_id(), ui::kInvalidAXNodeID);
  ASSERT_NE(model().end_node_id(), ui::kInvalidAXNodeID);

  model().Reset({1, 2});

  // Assert model().Reset selection state.
  ASSERT_FALSE(base::Contains(model().selection_node_ids(), 1));
  ASSERT_FALSE(base::Contains(model().selection_node_ids(), 2));
  ASSERT_FALSE(base::Contains(model().selection_node_ids(), 3));

  ASSERT_FALSE(model().has_selection());

  ASSERT_EQ(model().start_offset(), -1);
  ASSERT_EQ(model().end_offset(), -1);

  ASSERT_EQ(model().start_node_id(), ui::kInvalidAXNodeID);
  ASSERT_EQ(model().end_node_id(), ui::kInvalidAXNodeID);
}

TEST_F(ReadAnythingAppModelTest, PostProcessSelection_SelectionStateCorrect) {
  // Initial state.
  ui::AXTreeUpdate update;
  test::SetUpdateTreeID(&update, tree_id_);
  update.tree_data.sel_anchor_object_id = 2;
  update.tree_data.sel_focus_object_id = 3;
  update.tree_data.sel_anchor_offset = 0;
  update.tree_data.sel_focus_offset = 0;
  update.tree_data.sel_is_backward = false;
  AccessibilityEventReceived({update});
  model().set_requires_post_process_selection(true);
  model().PostProcessSelection();

  ASSERT_FALSE(model().requires_post_process_selection());
  ASSERT_TRUE(model().has_selection());

  ASSERT_TRUE(base::Contains(model().selection_node_ids(), 1));
  ASSERT_TRUE(base::Contains(model().selection_node_ids(), 2));
  ASSERT_TRUE(base::Contains(model().selection_node_ids(), 3));

  ASSERT_EQ(model().start_offset(), 0);
  ASSERT_EQ(model().end_offset(), 0);

  ASSERT_EQ(model().start_node_id(), 2);
  ASSERT_EQ(model().end_node_id(), 3);
}

TEST_F(ReadAnythingAppModelTest, PostProcessSelectionFromAction_DoesNotDraw) {
  // Initial state.
  ui::AXTreeUpdate update;
  test::SetUpdateTreeID(&update, tree_id_);
  update.tree_data.sel_anchor_object_id = 2;
  update.tree_data.sel_focus_object_id = 3;
  update.tree_data.sel_anchor_offset = 0;
  update.tree_data.sel_focus_offset = 0;
  update.tree_data.sel_is_backward = false;
  AccessibilityEventReceived({update});
  ProcessDisplayNodes({2, 3});
  model().set_selection_from_action(true);

  ASSERT_FALSE(model().PostProcessSelection());
}

TEST_F(
    ReadAnythingAppModelTest,
    PostProcessSelection_OnFirstOpen_DoesNotDrawWithNonEmptySelectionInside) {
  ProcessDisplayNodes({2, 3});
  ui::AXTreeUpdate update;
  test::SetUpdateTreeID(&update, tree_id_);
  update.tree_data.sel_anchor_object_id = 2;
  update.tree_data.sel_focus_object_id = 2;
  update.tree_data.sel_anchor_offset = 0;
  update.tree_data.sel_focus_offset = 5;
  update.tree_data.sel_is_backward = false;
  AccessibilityEventReceived({update});
  model().set_selection_from_action(false);

  ASSERT_FALSE(model().PostProcessSelection());
}

TEST_F(ReadAnythingAppModelTest,
       PostProcessSelection_OnFirstOpen_DoesNotDrawWithEmptySelectionInside) {
  ProcessDisplayNodes({2, 3});
  ui::AXTreeUpdate update;
  test::SetUpdateTreeID(&update, tree_id_);
  update.tree_data.sel_anchor_object_id = 2;
  update.tree_data.sel_focus_object_id = 2;
  update.tree_data.sel_anchor_offset = 5;
  update.tree_data.sel_focus_offset = 5;
  update.tree_data.sel_is_backward = false;
  AccessibilityEventReceived({update});
  model().set_selection_from_action(false);

  ASSERT_FALSE(model().PostProcessSelection());
}

TEST_F(ReadAnythingAppModelTest,
       PostProcessSelection_OnFirstOpen_DrawsWithNonEmptySelectionOutside) {
  ProcessDisplayNodes({2, 3});
  ui::AXTreeUpdate update;
  test::SetUpdateTreeID(&update, tree_id_);
  update.tree_data.sel_anchor_object_id = 4;
  update.tree_data.sel_focus_object_id = 4;
  update.tree_data.sel_anchor_offset = 0;
  update.tree_data.sel_focus_offset = 5;
  update.tree_data.sel_is_backward = false;
  AccessibilityEventReceived({update});
  model().set_selection_from_action(false);

  ASSERT_TRUE(model().PostProcessSelection());
}

TEST_F(
    ReadAnythingAppModelTest,
    PostmProcessSelection__OnFirstOpen_DoesNotDrawWithEmptySelectionOutside) {
  ProcessDisplayNodes({2, 3});
  ui::AXTreeUpdate update;
  test::SetUpdateTreeID(&update, tree_id_);
  update.tree_data.sel_anchor_object_id = 4;
  update.tree_data.sel_focus_object_id = 4;
  update.tree_data.sel_anchor_offset = 0;
  update.tree_data.sel_focus_offset = 0;
  update.tree_data.sel_is_backward = false;
  AccessibilityEventReceived({update});
  model().set_selection_from_action(false);

  ASSERT_FALSE(model().PostProcessSelection());
}

TEST_F(ReadAnythingAppModelTest,
       PostProcessSelection_EmptyInside_AfterNonEmptyInside_DoesNotDraw) {
  ProcessDisplayNodes({2, 3});

  // Non-empty selection inside display nodes.
  ui::AXTreeUpdate update1;
  test::SetUpdateTreeID(&update1, tree_id_);
  update1.tree_data.sel_anchor_object_id = 2;
  update1.tree_data.sel_focus_object_id = 2;
  update1.tree_data.sel_anchor_offset = 0;
  update1.tree_data.sel_focus_offset = 5;
  update1.tree_data.sel_is_backward = false;
  AccessibilityEventReceived({update1});
  model().set_selection_from_action(false);
  model().PostProcessSelection();

  // Empty selection inside display nodes.
  ui::AXTreeUpdate update2;
  test::SetUpdateTreeID(&update2, tree_id_);
  update2.tree_data.sel_anchor_object_id = 3;
  update2.tree_data.sel_focus_object_id = 3;
  update2.tree_data.sel_anchor_offset = 2;
  update2.tree_data.sel_focus_offset = 2;
  update2.tree_data.sel_is_backward = false;
  AccessibilityEventReceived({update2});
  model().set_selection_from_action(false);

  ASSERT_FALSE(model().PostProcessSelection());
}

TEST_F(ReadAnythingAppModelTest,
       PostProcessSelection_EmptyInside_AfterEmptyInside_DoesNotDraw) {
  ProcessDisplayNodes({2, 3});

  // Empty selection inside display nodes.
  ui::AXTreeUpdate update1;
  test::SetUpdateTreeID(&update1, tree_id_);
  update1.tree_data.sel_anchor_object_id = 2;
  update1.tree_data.sel_focus_object_id = 2;
  update1.tree_data.sel_anchor_offset = 0;
  update1.tree_data.sel_focus_offset = 0;
  update1.tree_data.sel_is_backward = false;
  AccessibilityEventReceived({update1});
  model().set_selection_from_action(false);
  model().PostProcessSelection();

  // Different empty selection inside display nodes.
  ui::AXTreeUpdate update2;
  test::SetUpdateTreeID(&update2, tree_id_);
  update2.tree_data.sel_anchor_object_id = 3;
  update2.tree_data.sel_focus_object_id = 3;
  update2.tree_data.sel_anchor_offset = 2;
  update2.tree_data.sel_focus_offset = 2;
  update2.tree_data.sel_is_backward = false;
  AccessibilityEventReceived({update2});
  model().set_selection_from_action(false);

  ASSERT_FALSE(model().PostProcessSelection());
}

TEST_F(ReadAnythingAppModelTest,
       PostProcessSelection_NonEmptyInside_AfterEmptyInside_DoesNotDraw) {
  ProcessDisplayNodes({2, 3});

  // Empty selection inside display nodes.
  ui::AXTreeUpdate update1;
  test::SetUpdateTreeID(&update1, tree_id_);
  update1.tree_data.sel_anchor_object_id = 3;
  update1.tree_data.sel_focus_object_id = 3;
  update1.tree_data.sel_anchor_offset = 2;
  update1.tree_data.sel_focus_offset = 2;
  update1.tree_data.sel_is_backward = false;
  AccessibilityEventReceived({update1});
  model().set_selection_from_action(false);
  model().PostProcessSelection();

  // Non-empty selection inside display nodes.
  ui::AXTreeUpdate update2;
  test::SetUpdateTreeID(&update2, tree_id_);
  update2.tree_data.sel_anchor_object_id = 2;
  update2.tree_data.sel_focus_object_id = 2;
  update2.tree_data.sel_anchor_offset = 0;
  update2.tree_data.sel_focus_offset = 5;
  update2.tree_data.sel_is_backward = false;
  AccessibilityEventReceived({update2});
  model().set_selection_from_action(false);

  ASSERT_FALSE(model().PostProcessSelection());
}

TEST_F(ReadAnythingAppModelTest,
       PostProcessSelection_NonEmptyInside_AfterNonEmptyInside_DoesNotDraw) {
  ProcessDisplayNodes({2, 3});

  // Non-empty selection inside display nodes.
  ui::AXTreeUpdate update1;
  test::SetUpdateTreeID(&update1, tree_id_);
  update1.tree_data.sel_anchor_object_id = 3;
  update1.tree_data.sel_focus_object_id = 3;
  update1.tree_data.sel_anchor_offset = 2;
  update1.tree_data.sel_focus_offset = 6;
  update1.tree_data.sel_is_backward = false;
  AccessibilityEventReceived({update1});
  model().set_selection_from_action(false);
  model().PostProcessSelection();

  // Different non-empty selection inside display nodes.
  ui::AXTreeUpdate update2;
  test::SetUpdateTreeID(&update2, tree_id_);
  update2.tree_data.sel_anchor_object_id = 2;
  update2.tree_data.sel_focus_object_id = 3;
  update2.tree_data.sel_anchor_offset = 0;
  update2.tree_data.sel_focus_offset = 5;
  update2.tree_data.sel_is_backward = false;
  AccessibilityEventReceived({update2});
  model().set_selection_from_action(false);

  ASSERT_FALSE(model().PostProcessSelection());
}

TEST_F(ReadAnythingAppModelTest,
       PostProcessSelection_EmptyOutside_AfterNonEmptyOutside_DoesDraw) {
  ProcessDisplayNodes({2, 3});

  // Non-empty selection outside display nodes.
  ui::AXTreeUpdate update1;
  test::SetUpdateTreeID(&update1, tree_id_);
  update1.tree_data.sel_anchor_object_id = 4;
  update1.tree_data.sel_focus_object_id = 4;
  update1.tree_data.sel_anchor_offset = 0;
  update1.tree_data.sel_focus_offset = 5;
  update1.tree_data.sel_is_backward = false;
  AccessibilityEventReceived({update1});
  model().set_selection_from_action(false);
  model().PostProcessSelection();

  // Empty selection outside display nodes.
  ui::AXTreeUpdate update2;
  test::SetUpdateTreeID(&update2, tree_id_);
  update2.tree_data.sel_anchor_object_id = 4;
  update2.tree_data.sel_focus_object_id = 4;
  update2.tree_data.sel_anchor_offset = 2;
  update2.tree_data.sel_focus_offset = 2;
  update2.tree_data.sel_is_backward = false;
  AccessibilityEventReceived({update2});
  model().set_selection_from_action(false);

  ASSERT_TRUE(model().PostProcessSelection());
}

TEST_F(ReadAnythingAppModelTest,
       PostProcessSelection_EmptyOutside_AfterEmptyOutside_DoesNotDraw) {
  ProcessDisplayNodes({2, 3});

  // Empty selection outside display nodes.
  ui::AXTreeUpdate update1;
  test::SetUpdateTreeID(&update1, tree_id_);
  update1.tree_data.sel_anchor_object_id = 4;
  update1.tree_data.sel_focus_object_id = 4;
  update1.tree_data.sel_anchor_offset = 0;
  update1.tree_data.sel_focus_offset = 0;
  update1.tree_data.sel_is_backward = false;
  AccessibilityEventReceived({update1});
  model().set_selection_from_action(false);
  model().PostProcessSelection();

  // Different empty selection outside display nodes.
  ui::AXTreeUpdate update2;
  test::SetUpdateTreeID(&update2, tree_id_);
  update2.tree_data.sel_anchor_object_id = 4;
  update2.tree_data.sel_focus_object_id = 4;
  update2.tree_data.sel_anchor_offset = 2;
  update2.tree_data.sel_focus_offset = 2;
  update2.tree_data.sel_is_backward = false;
  AccessibilityEventReceived({update2});
  model().set_selection_from_action(false);

  ASSERT_FALSE(model().PostProcessSelection());
}

TEST_F(ReadAnythingAppModelTest,
       PostProcessSelection_NonEmptyOutside_AfterEmptyOutside_DoesDraw) {
  ProcessDisplayNodes({2, 3});

  // Empty selection outside display nodes.
  ui::AXTreeUpdate update1;
  test::SetUpdateTreeID(&update1, tree_id_);
  update1.tree_data.sel_anchor_object_id = 4;
  update1.tree_data.sel_focus_object_id = 4;
  update1.tree_data.sel_anchor_offset = 2;
  update1.tree_data.sel_focus_offset = 2;
  update1.tree_data.sel_is_backward = false;
  AccessibilityEventReceived({update1});
  model().set_selection_from_action(false);
  model().PostProcessSelection();

  // Non-empty selection outside display nodes.
  ui::AXTreeUpdate update2;
  test::SetUpdateTreeID(&update2, tree_id_);
  update2.tree_data.sel_anchor_object_id = 4;
  update2.tree_data.sel_focus_object_id = 4;
  update2.tree_data.sel_anchor_offset = 0;
  update2.tree_data.sel_focus_offset = 5;
  update2.tree_data.sel_is_backward = false;
  AccessibilityEventReceived({update2});
  model().set_selection_from_action(false);

  ASSERT_TRUE(model().PostProcessSelection());
}

TEST_F(ReadAnythingAppModelTest,
       PostProcessSelection_NonEmptyOutside_AfterNonEmptyOutside_DoesDraw) {
  ProcessDisplayNodes({2, 3});

  // Non-empty selection outside display nodes.
  ui::AXTreeUpdate update1;
  test::SetUpdateTreeID(&update1, tree_id_);
  update1.tree_data.sel_anchor_object_id = 4;
  update1.tree_data.sel_focus_object_id = 4;
  update1.tree_data.sel_anchor_offset = 2;
  update1.tree_data.sel_focus_offset = 6;
  update1.tree_data.sel_is_backward = false;
  AccessibilityEventReceived({update1});
  model().set_selection_from_action(false);
  model().PostProcessSelection();

  // Different non-empty selection outside display nodes.
  ui::AXTreeUpdate update2;
  test::SetUpdateTreeID(&update2, tree_id_);
  update2.tree_data.sel_anchor_object_id = 4;
  update2.tree_data.sel_focus_object_id = 4;
  update2.tree_data.sel_anchor_offset = 0;
  update2.tree_data.sel_focus_offset = 5;
  update2.tree_data.sel_is_backward = false;
  AccessibilityEventReceived({update2});
  model().set_selection_from_action(false);

  ASSERT_TRUE(model().PostProcessSelection());
}

TEST_F(ReadAnythingAppModelTest,
       PostProcessSelection_EmptyInside_AfterNonEmptyOutside_DoesDraw) {
  ProcessDisplayNodes({2, 3});

  // Non-empty selection outside display nodes.
  ui::AXTreeUpdate update1;
  test::SetUpdateTreeID(&update1, tree_id_);
  update1.tree_data.sel_anchor_object_id = 4;
  update1.tree_data.sel_focus_object_id = 4;
  update1.tree_data.sel_anchor_offset = 0;
  update1.tree_data.sel_focus_offset = 5;
  update1.tree_data.sel_is_backward = false;
  AccessibilityEventReceived({update1});
  model().set_selection_from_action(false);
  model().PostProcessSelection();

  // Empty selection inside display nodes.
  ui::AXTreeUpdate update2;
  test::SetUpdateTreeID(&update2, tree_id_);
  update2.tree_data.sel_anchor_object_id = 2;
  update2.tree_data.sel_focus_object_id = 2;
  update2.tree_data.sel_anchor_offset = 2;
  update2.tree_data.sel_focus_offset = 2;
  update2.tree_data.sel_is_backward = false;
  AccessibilityEventReceived({update2});
  model().set_selection_from_action(false);

  ASSERT_TRUE(model().PostProcessSelection());
}

TEST_F(ReadAnythingAppModelTest,
       PostProcessSelection_EmptyInside_AfterEmptyOutside_DoesNotDraw) {
  ProcessDisplayNodes({2, 3});

  // Empty selection outside display nodes.
  ui::AXTreeUpdate update1;
  test::SetUpdateTreeID(&update1, tree_id_);
  update1.tree_data.sel_anchor_object_id = 4;
  update1.tree_data.sel_focus_object_id = 4;
  update1.tree_data.sel_anchor_offset = 0;
  update1.tree_data.sel_focus_offset = 0;
  update1.tree_data.sel_is_backward = false;
  AccessibilityEventReceived({update1});
  model().set_selection_from_action(false);
  model().PostProcessSelection();

  // Empty selection inside display nodes.
  ui::AXTreeUpdate update2;
  test::SetUpdateTreeID(&update2, tree_id_);
  update2.tree_data.sel_anchor_object_id = 2;
  update2.tree_data.sel_focus_object_id = 2;
  update2.tree_data.sel_anchor_offset = 2;
  update2.tree_data.sel_focus_offset = 2;
  update2.tree_data.sel_is_backward = false;
  AccessibilityEventReceived({update2});
  model().set_selection_from_action(false);

  ASSERT_FALSE(model().PostProcessSelection());
}

TEST_F(ReadAnythingAppModelTest,
       PostProcessSelection_NonEmptyInside_AfterEmptyOutside_DoesNotDraw) {
  ProcessDisplayNodes({2, 3});

  // Empty selection outside display nodes.
  ui::AXTreeUpdate update1;
  test::SetUpdateTreeID(&update1, tree_id_);
  update1.tree_data.sel_anchor_object_id = 4;
  update1.tree_data.sel_focus_object_id = 4;
  update1.tree_data.sel_anchor_offset = 2;
  update1.tree_data.sel_focus_offset = 2;
  update1.tree_data.sel_is_backward = false;
  AccessibilityEventReceived({update1});
  model().set_selection_from_action(false);
  model().PostProcessSelection();

  // Non-empty selection inside display nodes.
  ui::AXTreeUpdate update2;
  test::SetUpdateTreeID(&update2, tree_id_);
  update2.tree_data.sel_anchor_object_id = 2;
  update2.tree_data.sel_focus_object_id = 3;
  update2.tree_data.sel_anchor_offset = 0;
  update2.tree_data.sel_focus_offset = 5;
  update2.tree_data.sel_is_backward = false;
  AccessibilityEventReceived({update2});
  model().set_selection_from_action(false);

  ASSERT_FALSE(model().PostProcessSelection());
}

TEST_F(ReadAnythingAppModelTest,
       PostProcessSelection_NonEmptyInside_AfterNonEmptyOutside_DoesDraw) {
  ProcessDisplayNodes({2, 3});

  // Non-empty selection outside display nodes.
  ui::AXTreeUpdate update1;
  test::SetUpdateTreeID(&update1, tree_id_);
  update1.tree_data.sel_anchor_object_id = 4;
  update1.tree_data.sel_focus_object_id = 4;
  update1.tree_data.sel_anchor_offset = 2;
  update1.tree_data.sel_focus_offset = 6;
  update1.tree_data.sel_is_backward = false;
  AccessibilityEventReceived({update1});
  model().set_selection_from_action(false);
  model().PostProcessSelection();

  // Non-empty selection inside display nodes.
  ui::AXTreeUpdate update2;
  test::SetUpdateTreeID(&update2, tree_id_);
  update2.tree_data.sel_anchor_object_id = 2;
  update2.tree_data.sel_focus_object_id = 2;
  update2.tree_data.sel_anchor_offset = 0;
  update2.tree_data.sel_focus_offset = 5;
  update2.tree_data.sel_is_backward = false;
  AccessibilityEventReceived({update2});
  model().set_selection_from_action(false);

  ASSERT_TRUE(model().PostProcessSelection());
}

TEST_F(ReadAnythingAppModelTest,
       PostProcessSelection_EmptyOutside_AfterNonEmptyInside_DoesNotDraw) {
  ProcessDisplayNodes({2, 3});

  // Non-empty selection inside display nodes.
  ui::AXTreeUpdate update1;
  test::SetUpdateTreeID(&update1, tree_id_);
  update1.tree_data.sel_anchor_object_id = 2;
  update1.tree_data.sel_focus_object_id = 3;
  update1.tree_data.sel_anchor_offset = 0;
  update1.tree_data.sel_focus_offset = 5;
  update1.tree_data.sel_is_backward = false;
  AccessibilityEventReceived({update1});
  model().set_selection_from_action(false);
  model().PostProcessSelection();

  // Empty selection outside display nodes.
  ui::AXTreeUpdate update2;
  test::SetUpdateTreeID(&update2, tree_id_);
  update2.tree_data.sel_anchor_object_id = 4;
  update2.tree_data.sel_focus_object_id = 4;
  update2.tree_data.sel_anchor_offset = 2;
  update2.tree_data.sel_focus_offset = 2;
  update2.tree_data.sel_is_backward = false;
  AccessibilityEventReceived({update2});
  model().set_selection_from_action(false);

  ASSERT_FALSE(model().PostProcessSelection());
}

TEST_F(ReadAnythingAppModelTest,
       PostProcessSelection_EmptyOutside_AfterEmptyInside_DoesNotDraw) {
  ProcessDisplayNodes({2, 3});

  // Empty selection inside display nodes.
  ui::AXTreeUpdate update1;
  test::SetUpdateTreeID(&update1, tree_id_);
  update1.tree_data.sel_anchor_object_id = 2;
  update1.tree_data.sel_focus_object_id = 2;
  update1.tree_data.sel_anchor_offset = 0;
  update1.tree_data.sel_focus_offset = 0;
  update1.tree_data.sel_is_backward = false;
  AccessibilityEventReceived({update1});
  model().set_selection_from_action(false);
  model().PostProcessSelection();

  // Empty selection outside display nodes.
  ui::AXTreeUpdate update2;
  test::SetUpdateTreeID(&update2, tree_id_);
  update2.tree_data.sel_anchor_object_id = 4;
  update2.tree_data.sel_focus_object_id = 4;
  update2.tree_data.sel_anchor_offset = 2;
  update2.tree_data.sel_focus_offset = 2;
  update2.tree_data.sel_is_backward = false;
  AccessibilityEventReceived({update2});
  model().set_selection_from_action(false);

  ASSERT_FALSE(model().PostProcessSelection());
}

TEST_F(ReadAnythingAppModelTest,
       PostProcessSelection_NonEmptyOutside_AfterEmptyInside_DoesDraw) {
  ProcessDisplayNodes({2, 3});

  // Empty selection inside display nodes.
  ui::AXTreeUpdate update1;
  test::SetUpdateTreeID(&update1, tree_id_);
  update1.tree_data.sel_anchor_object_id = 2;
  update1.tree_data.sel_focus_object_id = 2;
  update1.tree_data.sel_anchor_offset = 2;
  update1.tree_data.sel_focus_offset = 2;
  update1.tree_data.sel_is_backward = false;
  AccessibilityEventReceived({update1});
  model().set_selection_from_action(false);
  model().PostProcessSelection();

  // Non-empty selection outside display nodes.
  ui::AXTreeUpdate update2;
  test::SetUpdateTreeID(&update2, tree_id_);
  update2.tree_data.sel_anchor_object_id = 4;
  update2.tree_data.sel_focus_object_id = 4;
  update2.tree_data.sel_anchor_offset = 0;
  update2.tree_data.sel_focus_offset = 5;
  update2.tree_data.sel_is_backward = false;
  AccessibilityEventReceived({update2});
  model().set_selection_from_action(false);

  ASSERT_TRUE(model().PostProcessSelection());
}

TEST_F(ReadAnythingAppModelTest,
       PostProcessSelection_NonEmptyOutside_AfterNonEmptyInside_DoesDraw) {
  ProcessDisplayNodes({2, 3});

  // Non-empty selection inside display nodes.
  ui::AXTreeUpdate update1;
  test::SetUpdateTreeID(&update1, tree_id_);
  update1.tree_data.sel_anchor_object_id = 2;
  update1.tree_data.sel_focus_object_id = 2;
  update1.tree_data.sel_anchor_offset = 2;
  update1.tree_data.sel_focus_offset = 6;
  update1.tree_data.sel_is_backward = false;
  AccessibilityEventReceived({update1});
  model().set_selection_from_action(false);
  model().PostProcessSelection();

  // Non-empty selection outside display nodes.
  ui::AXTreeUpdate update2;
  test::SetUpdateTreeID(&update2, tree_id_);
  update2.tree_data.sel_anchor_object_id = 4;
  update2.tree_data.sel_focus_object_id = 4;
  update2.tree_data.sel_anchor_offset = 0;
  update2.tree_data.sel_focus_offset = 5;
  update2.tree_data.sel_is_backward = false;
  AccessibilityEventReceived({update2});
  model().set_selection_from_action(false);

  ASSERT_TRUE(model().PostProcessSelection());
}

TEST_F(ReadAnythingAppModelTest,
       StartAndEndNodesHaveDifferentParents_SelectionStateCorrect) {
  ui::AXTreeUpdate update;
  test::SetUpdateTreeID(&update, tree_id_);

  ui::AXNodeData static_text_node1 = test::TextNode(/* id= */ 2);
  ui::AXNodeData static_text_node2 = test::TextNode(/* id= */ 3);
  ui::AXNodeData generic_container_node =
      test::GenericContainerNode(/*id= */ 4);
  ui::AXNodeData static_text_child_node1 = test::TextNode(/* id= */ 5);
  ui::AXNodeData static_text_child_node2 = test::TextNode(/* id= */ 6);

  ui::AXNodeData parent_node = test::TextNode(/* id= */ 1);
  parent_node.child_ids = {static_text_node1.id, static_text_node2.id,
                           generic_container_node.id};
  generic_container_node.child_ids = {static_text_child_node1.id,
                                      static_text_child_node2.id};
  update.nodes = {parent_node,
                  static_text_node1,
                  static_text_node2,
                  generic_container_node,
                  static_text_child_node1,
                  static_text_child_node2};

  AccessibilityEventReceived({update});

  update.tree_data.sel_anchor_object_id = 2;
  update.tree_data.sel_focus_object_id = 5;
  update.tree_data.sel_anchor_offset = 0;
  update.tree_data.sel_focus_offset = 0;
  update.tree_data.sel_is_backward = false;
  AccessibilityEventReceived({update});
  model().PostProcessSelection();

  ASSERT_TRUE(model().has_selection());
  ASSERT_EQ(model().start_node_id(), 2);
  ASSERT_EQ(model().end_node_id(), 5);

  // 1 and 3 are ancestors, so they are included as selection nodes..
  ASSERT_TRUE(base::Contains(model().selection_node_ids(), 1));
  ASSERT_TRUE(base::Contains(model().selection_node_ids(), 3));

  ASSERT_TRUE(base::Contains(model().selection_node_ids(), 5));
  ASSERT_TRUE(base::Contains(model().selection_node_ids(), 6));

  // Even though 3 is a generic container with more than one child, its
  // sibling nodes are included in the selection because the start node
  // includes it.
  ASSERT_TRUE(base::Contains(model().selection_node_ids(), 2));
  ASSERT_TRUE(base::Contains(model().selection_node_ids(), 3));
}

TEST_F(ReadAnythingAppModelTest,
       SelectionParentIsLinkAndInlineBlock_SelectionStateCorrect) {
  ui::AXTreeUpdate update;
  test::SetUpdateTreeID(&update, tree_id_);

  ui::AXNodeData static_text_node = test::TextNode(/* id= */ 2);

  ui::AXNodeData link_node;
  link_node.id = 3;
  link_node.role = ax::mojom::Role::kLink;
  link_node.AddStringAttribute(ax::mojom::StringAttribute::kDisplay, "block");

  ui::AXNodeData inline_block_node = test::TextNode(/* id= */ 4);
  inline_block_node.AddStringAttribute(ax::mojom::StringAttribute::kDisplay,
                                       "inline-block");
  link_node.child_ids = {inline_block_node.id};

  ui::AXNodeData root = test::TextNode(/* id= */ 1);
  root.child_ids = {static_text_node.id, link_node.id};
  update.nodes = {root, static_text_node, link_node, inline_block_node};

  AccessibilityEventReceived({update});

  update.tree_data.sel_anchor_object_id = 4;
  update.tree_data.sel_focus_object_id = 4;
  update.tree_data.sel_anchor_offset = 0;
  update.tree_data.sel_focus_offset = 1;
  update.tree_data.sel_is_backward = false;
  AccessibilityEventReceived({update});
  model().PostProcessSelection();

  ASSERT_TRUE(model().has_selection());
  ASSERT_EQ(model().start_node_id(), 4);
  ASSERT_EQ(model().end_node_id(), 4);

  ASSERT_TRUE(base::Contains(model().selection_node_ids(), 1));
  ASSERT_FALSE(base::Contains(model().selection_node_ids(), 2));
  ASSERT_TRUE(base::Contains(model().selection_node_ids(), 3));
  ASSERT_TRUE(base::Contains(model().selection_node_ids(), 4));
}

TEST_F(ReadAnythingAppModelTest,
       SelectionParentIsListItem_SelectionStateCorrect) {
  ui::AXTreeUpdate update;
  test::SetUpdateTreeID(&update, tree_id_);

  ui::AXNodeData static_text_node = test::TextNode(/* id= */ 2);

  ui::AXNodeData link_node;
  link_node.id = 3;
  link_node.role = ax::mojom::Role::kLink;
  link_node.AddStringAttribute(ax::mojom::StringAttribute::kDisplay, "block");

  ui::AXNodeData static_text_list_node = test::TextNode(/* id= */ 4);
  static_text_list_node.AddStringAttribute(ax::mojom::StringAttribute::kDisplay,
                                           "list-item");
  link_node.child_ids = {static_text_list_node.id};

  ui::AXNodeData parent_node = test::TextNode(/* id= */ 1);
  parent_node.child_ids = {static_text_node.id, link_node.id};
  update.nodes = {parent_node, static_text_node, link_node,
                  static_text_list_node};

  AccessibilityEventReceived({update});

  update.tree_data.sel_anchor_object_id = 4;
  update.tree_data.sel_focus_object_id = 4;
  update.tree_data.sel_anchor_offset = 0;
  update.tree_data.sel_focus_offset = 1;
  update.tree_data.sel_is_backward = false;
  AccessibilityEventReceived({update});
  model().PostProcessSelection();

  ASSERT_TRUE(model().has_selection());
  ASSERT_EQ(model().start_node_id(), 4);
  ASSERT_EQ(model().end_node_id(), 4);

  ASSERT_TRUE(base::Contains(model().selection_node_ids(), 1));
  ASSERT_FALSE(base::Contains(model().selection_node_ids(), 2));
  ASSERT_TRUE(base::Contains(model().selection_node_ids(), 3));
  ASSERT_TRUE(base::Contains(model().selection_node_ids(), 4));
}

TEST_F(ReadAnythingAppModelTest,
       SelectionParentIsGenericContainerAndInline_SelectionStateCorrect) {
  ui::AXTreeUpdate update;
  test::SetUpdateTreeID(&update, tree_id_);
  ui::AXNodeData static_text_node = test::TextNode(/* id= */ 2);

  ui::AXNodeData generic_container_node =
      test::GenericContainerNode(/*id= */ 3);
  generic_container_node.AddStringAttribute(
      ax::mojom::StringAttribute::kDisplay, "block");
  ui::AXNodeData inline_node = test::TextNode(/* id= */ 4);
  inline_node.AddStringAttribute(ax::mojom::StringAttribute::kDisplay,
                                 "inline");
  generic_container_node.child_ids = {inline_node.id};

  ui::AXNodeData parent_node = test::TextNode(/* id= */ 1);
  parent_node.child_ids = {static_text_node.id, generic_container_node.id};
  update.nodes = {parent_node, static_text_node, generic_container_node,
                  inline_node};

  AccessibilityEventReceived({update});

  update.tree_data.sel_anchor_object_id = 4;
  update.tree_data.sel_focus_object_id = 4;
  update.tree_data.sel_anchor_offset = 0;
  update.tree_data.sel_focus_offset = 1;
  update.tree_data.sel_is_backward = true;
  AccessibilityEventReceived({update});
  model().PostProcessSelection();

  ASSERT_TRUE(model().has_selection());
  ASSERT_EQ(model().start_node_id(), 4);
  ASSERT_EQ(model().end_node_id(), 4);

  ASSERT_TRUE(base::Contains(model().selection_node_ids(), 1));
  ASSERT_FALSE(base::Contains(model().selection_node_ids(), 2));
  ASSERT_TRUE(base::Contains(model().selection_node_ids(), 3));
  ASSERT_TRUE(base::Contains(model().selection_node_ids(), 4));
}

TEST_F(
    ReadAnythingAppModelTest,
    SelectionParentIsGenericContainerWithMultipleChildren_SelectionStateCorrect) {
  ui::AXTreeUpdate update;
  test::SetUpdateTreeID(&update, tree_id_);
  ui::AXNodeData static_text_node = test::TextNode(/* id= */ 2);
  ui::AXNodeData generic_container_node =
      test::GenericContainerNode(/* id= */ 3);
  ui::AXNodeData static_text_child_node1 = test::TextNode(/* id= */ 4);
  ui::AXNodeData static_text_child_node2 = test::TextNode(/* id= */ 5);
  generic_container_node.child_ids = {static_text_child_node1.id,
                                      static_text_child_node2.id};

  ui::AXNodeData parent_node = test::TextNode(/* id= */ 1);
  parent_node.child_ids = {static_text_node.id, generic_container_node.id};
  update.nodes = {parent_node, static_text_node, generic_container_node,
                  static_text_child_node1, static_text_child_node2};

  AccessibilityEventReceived({update});

  update.tree_data.sel_anchor_object_id = 4;
  update.tree_data.sel_focus_object_id = 5;
  update.tree_data.sel_anchor_offset = 0;
  update.tree_data.sel_focus_offset = 0;
  update.tree_data.sel_is_backward = false;
  AccessibilityEventReceived({update});
  model().PostProcessSelection();

  ASSERT_TRUE(model().has_selection());
  ASSERT_EQ(model().start_node_id(), 4);
  ASSERT_EQ(model().end_node_id(), 5);

  // 1 and 3 are ancestors, so they are included as selection nodes..
  ASSERT_TRUE(base::Contains(model().selection_node_ids(), 1));
  ASSERT_TRUE(base::Contains(model().selection_node_ids(), 3));
  ASSERT_TRUE(base::Contains(model().selection_node_ids(), 4));
  ASSERT_TRUE(base::Contains(model().selection_node_ids(), 5));

  // Since 3 is a generic container with more than one child, its sibling nodes
  // are not included, so 2 is ignored.
  ASSERT_FALSE(base::Contains(model().selection_node_ids(), 2));
}

TEST_F(ReadAnythingAppModelTest, ResetTextSize_ReturnsTextSizeToDefault) {
  const double default_font_size = model().font_size();

  model().AdjustTextSize(3);
  EXPECT_GT(model().font_size(), default_font_size);

  model().ResetTextSize();
  EXPECT_EQ(model().font_size(), default_font_size);

  model().AdjustTextSize(-3);
  EXPECT_LT(model().font_size(), default_font_size);

  model().ResetTextSize();
  EXPECT_EQ(model().font_size(), default_font_size);
}

TEST_F(ReadAnythingAppModelTest, LanguageCode_ReturnsCorrectCode) {
  ASSERT_EQ(model().base_language_code(), "en");

  model().SetBaseLanguageCode("es");
  ASSERT_EQ(model().base_language_code(), "es");
}

TEST_F(ReadAnythingAppModelTest,
       SupportedFonts_InvalidLanguageCode_ReturnsDefaultFonts) {
  model().SetBaseLanguageCode("qr");
  EXPECT_THAT(model().supported_fonts(), ElementsAre("Sans-serif", "Serif"));
}

TEST_F(ReadAnythingAppModelTest,
       SupportedFonts_BeforeLanguageSet_ReturnsDefaultFonts) {
  EXPECT_THAT(model().supported_fonts(),
              ElementsAre("Poppins", "Sans-serif", "Serif", "Comic Neue",
                          "Lexend Deca", "EB Garamond", "STIX Two Text",
                          "Andika", "Atkinson Hyperlegible"));
}

TEST_F(ReadAnythingAppModelTest,
       SupportedFonts_SetLanguageCode_ReturnsExpectedDefaultFonts) {
  // Spanish
  model().SetBaseLanguageCode("es");
  EXPECT_THAT(model().supported_fonts(),
              ElementsAre("Poppins", "Sans-serif", "Serif", "Comic Neue",
                          "Lexend Deca", "EB Garamond", "STIX Two Text",
                          "Andika", "Atkinson Hyperlegible"));

  // Bulgarian
  model().SetBaseLanguageCode("bg");
  EXPECT_THAT(model().supported_fonts(),
              ElementsAre("Sans-serif", "Serif", "EB Garamond", "STIX Two Text",
                          "Andika"));

  // Hindi
  model().SetBaseLanguageCode("hi");
  EXPECT_THAT(model().supported_fonts(),
              ElementsAre("Poppins", "Sans-serif", "Serif"));
}

TEST_F(ReadAnythingAppModelTest, PdfEvents_SetRequiresDistillation) {
  model().set_is_pdf(true);

  ui::AXTreeUpdate initial_update;
  test::SetUpdateTreeID(&initial_update, tree_id_);
  initial_update.root_id = 1;
  ui::AXNodeData embedded_node;
  embedded_node.id = 2;
  embedded_node.role = ax::mojom::Role::kEmbeddedObject;

  ui::AXNodeData pdf_root_node;
  pdf_root_node.id = 1;
  pdf_root_node.role = ax::mojom::Role::kPdfRoot;
  pdf_root_node.child_ids = {embedded_node.id};
  initial_update.nodes = {pdf_root_node, embedded_node};
  AccessibilityEventReceived({initial_update});

  // Update with no new nodes added to the tree.
  ui::AXTreeUpdate update;
  test::SetUpdateTreeID(&update, tree_id_);
  update.root_id = 1;
  ui::AXNodeData node;
  node.id = 1;
  node.role = ax::mojom::Role::kPdfRoot;
  node.SetNameChecked("example.pdf");
  update.nodes = {node};
  AccessibilityEventReceived({update});
  ASSERT_FALSE(model().requires_distillation());

  // Tree update with PDF contents (new nodes added).
  ui::AXTreeUpdate update2;
  test::SetUpdateTreeID(&update2, tree_id_);
  update2.root_id = 1;
  ui::AXNodeData static_text_node1 = test::TextNode(/* id= */ 1);

  ui::AXNodeData updated_embedded_node;
  updated_embedded_node.id = 2;
  updated_embedded_node.role = ax::mojom::Role::kEmbeddedObject;
  static_text_node1.child_ids = {updated_embedded_node.id};

  ui::AXNodeData static_text_node2 = test::TextNode(/* id= */ 3);
  updated_embedded_node.child_ids = {static_text_node2.id};
  update2.nodes = {static_text_node1, updated_embedded_node, static_text_node2};

  AccessibilityEventReceived({update2});
  ASSERT_TRUE(model().requires_distillation());
}

TEST_F(ReadAnythingAppModelTest, PdfEvents_DontSetRequiresDistillation) {
  model().set_is_pdf(true);

  ui::AXTreeUpdate initial_update;
  test::SetUpdateTreeID(&initial_update, tree_id_);
  initial_update.root_id = 1;
  ui::AXNodeData node;
  node.id = 1;
  node.role = ax::mojom::Role::kPdfRoot;
  initial_update.nodes = {node};
  AccessibilityEventReceived({initial_update});

  // Updates that don't create a new subtree, for example, a role change, should
  // not set requires_distillation_.
  ui::AXTreeUpdate update;
  test::SetUpdateTreeID(&update, tree_id_);
  ui::AXNodeData static_text_node = test::TextNode(/* id= */ 1);
  update.root_id = static_text_node.id;
  update.nodes = {static_text_node};
  AccessibilityEventReceived({update});
  ASSERT_FALSE(model().requires_distillation());
}

TEST_F(ReadAnythingAppModelTest, OnSelection_HandlesClickAndDragEvents) {
  ui::AXTreeUpdate update;
  test::SetUpdateTreeID(&update, tree_id_);
  update.tree_data.sel_anchor_object_id = 2;
  update.tree_data.sel_focus_object_id = 3;
  update.tree_data.sel_anchor_offset = 0;
  update.tree_data.sel_focus_offset = 0;
  update.tree_data.sel_is_backward = false;
  AccessibilityEventReceived({update});
  model().PostProcessSelection();

  // If there is a click and drag selection (the anchor object id and offset are
  // the same as the prev selection received), the event_from eventually changes
  // from kUser to kPage. Post process selection should be required in either
  // case.
  // model().set_requires_post_process_selection(false) is needed to
  // model().Reset the flag to check that model().OnSelection(...) properly sets
  // (or doesn't set) the flag.
  update.tree_data.sel_anchor_object_id = 2;
  update.tree_data.sel_focus_object_id = 3;
  update.tree_data.sel_anchor_offset = 0;
  update.tree_data.sel_focus_offset = 1;
  update.tree_data.sel_is_backward = false;
  AccessibilityEventReceived({update});

  model().set_requires_post_process_selection(false);
  model().OnSelection(ax::mojom::EventFrom::kUser);
  EXPECT_TRUE(model().requires_post_process_selection());

  model().set_requires_post_process_selection(false);
  model().OnSelection(ax::mojom::EventFrom::kPage);
  EXPECT_TRUE(model().requires_post_process_selection());

  // If the user drags the selection so that it is backwards, post process
  // selection should still be required.
  update.tree_data.sel_anchor_object_id = 2;
  update.tree_data.sel_focus_object_id = 1;
  update.tree_data.sel_anchor_offset = 0;
  update.tree_data.sel_focus_offset = 2;
  update.tree_data.sel_is_backward = true;
  AccessibilityEventReceived({update});
  model().set_requires_post_process_selection(false);
  model().OnSelection(ax::mojom::EventFrom::kPage);
  EXPECT_TRUE(model().requires_post_process_selection());

  // If the anchor changes (the user stopped dragging their cursor) and we
  // receive an event with event_from kPage, post process selection should not
  // be set to true.
  update.tree_data.sel_anchor_object_id = 2;
  update.tree_data.sel_focus_object_id = 3;
  update.tree_data.sel_anchor_offset = 1;
  update.tree_data.sel_focus_offset = 0;
  update.tree_data.sel_is_backward = false;
  AccessibilityEventReceived({update});
  model().set_requires_post_process_selection(false);
  model().OnSelection(ax::mojom::EventFrom::kPage);
  EXPECT_FALSE(model().requires_post_process_selection());
}

TEST_F(ReadAnythingAppModelTest, LastExpandedNodeNamedChanged_TriggersRedraw) {
  ui::AXTreeUpdate initial_update;
  test::SetUpdateTreeID(&initial_update, tree_id_);
  ui::AXNodeData initial_node = test::TextNode(/* id= */ 2, u"Old Name");
  initial_update.nodes = {initial_node};
  AccessibilityEventReceived({initial_update});

  ui::AXTreeUpdate update;
  test::SetUpdateTreeID(&update, tree_id_);
  ui::AXNodeData updated_node = test::TextNode(initial_node.id, u"New Name");
  update.nodes = {updated_node};
  model().set_last_expanded_node_id(initial_node.id);
  EXPECT_EQ(model().last_expanded_node_id(), initial_node.id);
  AccessibilityEventReceived({update});

  EXPECT_FALSE(model().requires_post_process_selection());
  EXPECT_TRUE(model().redraw_required());
  EXPECT_EQ(model().last_expanded_node_id(), ui::kInvalidAXNodeID);
  // Check selection model().Reset.
  EXPECT_FALSE(model().has_selection());
  EXPECT_EQ(model().start_offset(), -1);
  EXPECT_EQ(model().end_offset(), -1);
  EXPECT_EQ(model().start_node_id(), ui::kInvalidAXNodeID);
  EXPECT_EQ(model().end_node_id(), ui::kInvalidAXNodeID);
  EXPECT_TRUE(model().selection_node_ids().empty());
}

TEST_F(ReadAnythingAppModelTest, ContentEditableValueChanged_ResetsDrawTimer) {
  ui::AXTreeUpdate update;
  test::SetUpdateTreeID(&update, tree_id_);
  ui::AXNodeData node1;
  node1.id = 1;
  update.nodes = {node1};
  std::vector<ui::AXTreeUpdate> updates = {std::move(update)};

  ui::AXEvent event;
  event.id = node1.id;
  event.event_type = ax::mojom::Event::kValueChanged;
  std::vector<ui::AXEvent> events = {std::move(event)};
  // This update changes the structure of the tree. When the controller receives
  // it in AccessibilityEventReceived, it will re-distill the tree.
  model().AccessibilityEventReceived(update.tree_data.tree_id, updates, events,
                                     false);
  EXPECT_TRUE(model().reset_draw_timer());
}
