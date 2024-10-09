// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/accessibility/read_anything_app_model.h"

#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/threading/platform_thread.h"
#include "chrome/renderer/accessibility/read_anything_node_utils.h"
#include "chrome/renderer/accessibility/read_anything_test_utils.h"
#include "chrome/test/base/chrome_render_view_test.h"
#include "read_anything_test_utils.h"
#include "services/strings/grit/services_strings.h"
#include "ui/accessibility/ax_enums.mojom-shared.h"
#include "ui/accessibility/ax_event.h"
#include "ui/accessibility/ax_node_id_forward.h"
#include "ui/accessibility/ax_serializable_tree.h"
#include "ui/accessibility/ax_updates_and_events.h"
#include "ui/base/l10n/l10n_util.h"

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
    SetUpdateTreeID(snapshot.get());

    AccessibilityEventReceived({*snapshot});
    SetActiveTreeId(tree_id_);
    Reset({});
  }

  void SetUpWithoutInitialization() {
    model_ = std::make_unique<ReadAnythingAppModel>();
  }

  void SetUpdateTreeID(ui::AXTreeUpdate* update) {
    test::SetUpdateTreeID(update, tree_id_);
  }

  void set_distillation_in_progress(bool distillation) {
    model_->set_distillation_in_progress(distillation);
  }

  void SetLastExpandedNodeId(ui::AXNodeID id) {
    model_->set_last_expanded_node_id(id);
  }

  ui::AXNodeID LastExpandedNodeId() { return model_->last_expanded_node_id(); }

  bool AreAllPendingUpdatesEmpty() {
    size_t count = 0;
    for (auto const& [tree_id, updates] :
         model_->GetPendingUpdatesForTesting()) {
      count += updates.size();
    }
    return count == 0;
  }

  void OnSettingsRestoredFromPrefs(
      read_anything::mojom::LineSpacing line_spacing,
      read_anything::mojom::LetterSpacing letter_spacing,
      const std::string& font,
      double font_size,
      bool links_enabled,
      bool images_enabled,
      read_anything::mojom::Colors color) {
    model_->OnSettingsRestoredFromPrefs(line_spacing, letter_spacing, font,
                                        font_size, links_enabled,
                                        images_enabled, color);
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

  void AccessibilityEventReceived(const ui::AXTreeID& tree_id,
                                  const std::vector<ui::AXTreeUpdate>& updates,
                                  const std::vector<ui::AXEvent>& events,
                                  bool speech_playing = false) {
    model_->AccessibilityEventReceived(
        tree_id, const_cast<std::vector<ui::AXTreeUpdate>&>(updates),
        const_cast<std::vector<ui::AXEvent>&>(events), speech_playing);
  }

  void SetActiveTreeId(ui::AXTreeID tree_id) {
    model_->SetActiveTreeId(tree_id);
  }

  void UnserializePendingUpdates(ui::AXTreeID tree_id) {
    model_->UnserializePendingUpdates(tree_id);
  }

  void ClearPendingUpdates() { model_->ClearPendingUpdates(); }

  std::string FontName() { return model_->font_name(); }

  void SetFontName(std::string font) { model_->set_font_name(font); }

  float FontSize() { return model_->font_size(); }

  bool LinksEnabled() { return model_->links_enabled(); }

  bool ImagesEnabled() { return model_->images_enabled(); }

  int LineSpacing() { return model_->line_spacing(); }

  int LetterSpacing() { return model_->letter_spacing(); }

  int ColorTheme() { return model_->color_theme(); }

  bool DistillationInProgress() { return model_->distillation_in_progress(); }

  bool HasSelection() { return model_->has_selection(); }

  ui::AXNodeID StartNodeId() { return model_->start_node_id(); }
  ui::AXNodeID EndNodeId() { return model_->end_node_id(); }

  int32_t StartOffset() { return model_->start_offset(); }
  int32_t EndOffset() { return model_->end_offset(); }

  bool IsNodeIgnoredForReadAnything(ui::AXNodeID ax_node_id) {
    return a11y::IsNodeIgnoredForReadAnything(model_->GetAXNode(ax_node_id),
                                              model_->is_pdf());
  }

  size_t GetNumTrees() { return model_->GetTreesForTesting()->size(); }

  bool HasTree(ui::AXTreeID tree_id) { return model_->ContainsTree(tree_id); }

  void EraseTree(ui::AXTreeID tree_id) { model_->EraseTreeForTesting(tree_id); }

  void AddTree(ui::AXTreeID tree_id,
               std::unique_ptr<ui::AXSerializableTree> tree) {
    model_->AddTree(tree_id, std::move(tree));
  }

  size_t GetNumPendingUpdates(ui::AXTreeID tree_id) {
    return model_->GetPendingUpdatesForTesting()[tree_id].size();
  }

  void Reset(const std::vector<ui::AXNodeID>& content_node_ids) {
    model_->Reset(content_node_ids);
  }

  bool ContentNodeIdsContains(ui::AXNodeID ax_node_id) {
    return base::Contains(model_->content_node_ids(), ax_node_id);
  }

  bool DisplayNodeIdsContains(ui::AXNodeID ax_node_id) {
    return base::Contains(model_->display_node_ids(), ax_node_id);
  }

  bool DisplayNodeIdsIsEmpty() { return model_->display_node_ids().empty(); }

  bool SelectionNodeIdsContains(ui::AXNodeID ax_node_id) {
    return base::Contains(model_->selection_node_ids(), ax_node_id);
  }

  bool SelectionNodeIdsEmpty() { return model_->selection_node_ids().empty(); }

  void ProcessDisplayNodes(const std::vector<ui::AXNodeID>& content_node_ids) {
    Reset(content_node_ids);
    model_->ComputeDisplayNodeIdsForDistilledTree();
  }

  bool ProcessSelection() { return model_->PostProcessSelection(); }

  bool RequiresDistillation() { return model_->requires_distillation(); }

  bool RequiresRedraw() { return model_->redraw_required(); }

  bool DrawTimerReset() { return model_->reset_draw_timer(); }

  bool RequiresPostProcessSelection() {
    return model_->requires_post_process_selection();
  }
  void SetRequiresPostProcessSelection(bool requires_post_process_selection) {
    model_->set_requires_post_process_selection(
        requires_post_process_selection);
  }
  void SetSelectionFromAction(bool selection_from_action) {
    model_->set_selection_from_action(selection_from_action);
  }

  void OnSelection(ax::mojom::EventFrom event_from) {
    model_->OnSelection(event_from);
  }

  bool IsDocs() { return model_->IsDocs(); }

  void IncreaseTextSize() { model_->IncreaseTextSize(); }

  void DecreaseTextSize() { model_->DecreaseTextSize(); }

  void ResetTextSize() { model_->ResetTextSize(); }

  std::string LanguageCode() { return model_->base_language_code(); }
  void SetLanguageCode(std::string code) { model_->SetBaseLanguageCode(code); }

  std::vector<std::string> GetSupportedFonts() {
    return model_->GetSupportedFonts();
  }

  void set_is_pdf(bool is_pdf) { return model_->set_is_pdf(is_pdf); }

  std::vector<int> SendSimpleUpdateAndGetChildIds() {
    // Set the name of each node to be its id.
    ui::AXTreeUpdate initial_update;
    SetUpdateTreeID(&initial_update);
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

  std::vector<ui::AXTreeUpdate> CreateSimpleUpdateList(
      std::vector<int> child_ids) {
    return test::CreateSimpleUpdateList(child_ids, tree_id_);
  }

  ui::AXTreeID tree_id_;

 private:
  std::unique_ptr<ReadAnythingAppModel> model_ = nullptr;
};

TEST_F(ReadAnythingAppModelTest, IsDocs_FalseBeforeTreeInitialization) {
  EXPECT_FALSE(IsDocs());
  SetUpWithoutInitialization();
  EXPECT_FALSE(IsDocs());
}

TEST_F(ReadAnythingAppModelTest, FontName) {
  EXPECT_EQ(string_constants::kReadAnythingPlaceholderFontName, FontName());

  std::string font_name = "Montserrat";
  SetFontName(font_name);
  EXPECT_EQ(font_name, FontName());
}

TEST_F(ReadAnythingAppModelTest, OnSettingsRestoredFromPrefs) {
  auto line_spacing = read_anything::mojom::LineSpacing::kDefaultValue;
  auto letter_spacing = read_anything::mojom::LetterSpacing::kDefaultValue;
  std::string font_name = "Roboto";
  double font_size = 18.0;
  bool links_enabled = false;
  bool images_enabled = true;
  auto color = read_anything::mojom::Colors::kDefaultValue;
  int color_value = 0;

  OnSettingsRestoredFromPrefs(line_spacing, letter_spacing, font_name,
                              font_size, links_enabled, images_enabled, color);

  EXPECT_EQ(static_cast<int>(line_spacing), LineSpacing());
  EXPECT_EQ(static_cast<int>(letter_spacing), LetterSpacing());
  EXPECT_EQ(font_name, FontName());
  EXPECT_EQ(font_size, FontSize());
  EXPECT_EQ(links_enabled, LinksEnabled());
  EXPECT_EQ(images_enabled, ImagesEnabled());
  EXPECT_EQ(color_value, ColorTheme());
}

TEST_F(ReadAnythingAppModelTest, IsNodeIgnoredForReadAnything) {
  ui::AXTreeUpdate update;
  SetUpdateTreeID(&update);
  ui::AXNodeData static_text_node = test::TextNode(/* id = */ 2);

  ui::AXNodeData combobox_node;
  combobox_node.id = 3;
  combobox_node.role = ax::mojom::Role::kComboBoxGrouping;

  ui::AXNodeData button_node;
  button_node.id = 4;
  button_node.role = ax::mojom::Role::kButton;
  update.nodes = {static_text_node, combobox_node, button_node};

  AccessibilityEventReceived({update});
  EXPECT_EQ(false, IsNodeIgnoredForReadAnything(2));
  EXPECT_EQ(true, IsNodeIgnoredForReadAnything(3));
  EXPECT_EQ(true, IsNodeIgnoredForReadAnything(4));
}

TEST_F(ReadAnythingAppModelTest,
       IsNodeIgnoredForReadAnything_TextFieldsNotIgnored) {
  ui::AXTreeUpdate update;
  SetUpdateTreeID(&update);
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
  EXPECT_EQ(true, IsNodeIgnoredForReadAnything(2));
  EXPECT_EQ(false, IsNodeIgnoredForReadAnything(3));
  EXPECT_EQ(false, IsNodeIgnoredForReadAnything(4));
}

TEST_F(ReadAnythingAppModelTest,
       IsNodeIgnoredForReadAnything_InaccessiblePDFPageNodes) {
  set_is_pdf(true);

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
  EXPECT_EQ(true, IsNodeIgnoredForReadAnything(2));
  EXPECT_EQ(true, IsNodeIgnoredForReadAnything(3));
  EXPECT_EQ(false, IsNodeIgnoredForReadAnything(4));
  EXPECT_EQ(true, IsNodeIgnoredForReadAnything(5));
}

TEST_F(ReadAnythingAppModelTest, ModelUpdatesTreeState) {
  // Set up trees.
  ui::AXTreeID tree_id_2 = ui::AXTreeID::CreateNewAXTreeID();
  ui::AXTreeID tree_id_3 = ui::AXTreeID::CreateNewAXTreeID();

  AddTree(tree_id_2, std::make_unique<ui::AXSerializableTree>());
  AddTree(tree_id_3, std::make_unique<ui::AXSerializableTree>());

  ASSERT_EQ(3u, GetNumTrees());
  ASSERT_TRUE(HasTree(tree_id_2));
  ASSERT_TRUE(HasTree(tree_id_3));
  ASSERT_TRUE(HasTree(tree_id_));

  // Remove one tree.
  EraseTree(tree_id_2);
  ASSERT_EQ(2u, GetNumTrees());
  ASSERT_TRUE(HasTree(tree_id_3));
  ASSERT_FALSE(HasTree(tree_id_2));
  ASSERT_TRUE(HasTree(tree_id_));

  // Remove the second tree.
  EraseTree(tree_id_);
  ASSERT_EQ(1u, GetNumTrees());
  ASSERT_TRUE(HasTree(tree_id_3));
  ASSERT_FALSE(HasTree(tree_id_2));
  ASSERT_FALSE(HasTree(tree_id_));

  // Remove the last tree.
  EraseTree(tree_id_3);
  ASSERT_EQ(0u, GetNumTrees());
  ASSERT_FALSE(HasTree(tree_id_3));
  ASSERT_FALSE(HasTree(tree_id_2));
  ASSERT_FALSE(HasTree(tree_id_));
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
  ASSERT_EQ(1u, GetNumTrees());
  ASSERT_TRUE(HasTree(tree_id_));

  // Add the two trees.
  AccessibilityEventReceived({updates[0]});
  ASSERT_EQ(2u, GetNumTrees());
  ASSERT_TRUE(HasTree(tree_id_));
  ASSERT_TRUE(HasTree(tree_ids[0]));
  AccessibilityEventReceived({updates[1]});
  ASSERT_EQ(3u, GetNumTrees());
  ASSERT_TRUE(HasTree(tree_id_));
  ASSERT_TRUE(HasTree(tree_ids[0]));
  ASSERT_TRUE(HasTree(tree_ids[1]));

  // Remove all of the trees.
  EraseTree(tree_id_);
  ASSERT_EQ(2u, GetNumTrees());
  ASSERT_TRUE(HasTree(tree_ids[0]));
  ASSERT_TRUE(HasTree(tree_ids[1]));
  EraseTree(tree_ids[0]);
  ASSERT_EQ(1u, GetNumTrees());
  ASSERT_TRUE(HasTree(tree_ids[1]));
  EraseTree(tree_ids[1]);
  ASSERT_EQ(0u, GetNumTrees());
}

TEST_F(ReadAnythingAppModelTest,
       DistillationInProgress_TreeUpdateReceivedOnInactiveTree) {
  EXPECT_EQ(0u, GetNumPendingUpdates(tree_id_));

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
  EXPECT_EQ(0u, GetNumPendingUpdates(tree_id_));
}

TEST_F(ReadAnythingAppModelTest,
       AddPendingUpdatesAfterUnserializingOnSameTree_DoesNotCrash) {
  std::vector<int> child_ids = SendSimpleUpdateAndGetChildIds();
  std::vector<ui::AXTreeUpdate> updates = CreateSimpleUpdateList(child_ids);

  // Send update 0, which starts distillation.
  AccessibilityEventReceived({updates[0]});
  EXPECT_EQ(0u, GetNumPendingUpdates(tree_id_));
  ASSERT_TRUE(AreAllPendingUpdatesEmpty());

  // Send update 1. Since distillation is in progress, this will not be
  // unserialized yet.
  set_distillation_in_progress(true);
  AccessibilityEventReceived({updates[1]});
  EXPECT_EQ(1u, GetNumPendingUpdates(tree_id_));

  // Ensure that there are no crashes after an accessibility event is received
  // immediately after unserializing.
  UnserializePendingUpdates(tree_id_);
  set_distillation_in_progress(true);
  AccessibilityEventReceived({updates[2]});
  EXPECT_EQ(1u, GetNumPendingUpdates(tree_id_));
  ASSERT_FALSE(AreAllPendingUpdatesEmpty());
}

TEST_F(ReadAnythingAppModelTest, OnTreeErased_ClearsPendingUpdates) {
  std::vector<int> child_ids = SendSimpleUpdateAndGetChildIds();
  std::vector<ui::AXTreeUpdate> updates = CreateSimpleUpdateList(child_ids);

  // Send update 0, which starts distillation.
  AccessibilityEventReceived({updates[0]});
  EXPECT_EQ(0u, GetNumPendingUpdates(tree_id_));
  ASSERT_TRUE(AreAllPendingUpdatesEmpty());

  // Send update 1. Since distillation is in progress, this will not be
  // unserialized yet.
  set_distillation_in_progress(true);
  AccessibilityEventReceived({updates[1]});
  EXPECT_EQ(1u, GetNumPendingUpdates(tree_id_));

  // Destroy the tree.
  EraseTree(tree_id_);
  EXPECT_EQ(0u, GetNumPendingUpdates(tree_id_));
}

TEST_F(ReadAnythingAppModelTest,
       DistillationInProgress_TreeUpdateReceivedOnActiveTree) {
  std::vector<int> child_ids = SendSimpleUpdateAndGetChildIds();
  std::vector<ui::AXTreeUpdate> updates = CreateSimpleUpdateList(child_ids);

  // Send update 0, which starts distillation.
  AccessibilityEventReceived({updates[0]});
  EXPECT_EQ(0u, GetNumPendingUpdates(tree_id_));
  ASSERT_TRUE(AreAllPendingUpdatesEmpty());

  // Send update 1. Since distillation is in progress, this will not be
  // unserialized yet.
  set_distillation_in_progress(true);
  AccessibilityEventReceived({updates[1]});
  EXPECT_EQ(1u, GetNumPendingUpdates(tree_id_));

  // Send update 2. This is still not unserialized yet.
  AccessibilityEventReceived({updates[2]});
  EXPECT_EQ(2u, GetNumPendingUpdates(tree_id_));

  // Complete distillation which unserializes the pending updates and distills
  // them.
  UnserializePendingUpdates(tree_id_);
  EXPECT_EQ(0u, GetNumPendingUpdates(tree_id_));
  ASSERT_TRUE(AreAllPendingUpdatesEmpty());
}

TEST_F(ReadAnythingAppModelTest, SpeechPlaying_TreeUpdateReceivedOnActiveTree) {
  std::vector<int> child_ids = SendSimpleUpdateAndGetChildIds();
  std::vector<ui::AXTreeUpdate> updates = CreateSimpleUpdateList(child_ids);

  // Send update 0, which starts distillation.
  AccessibilityEventReceived({updates[0]});
  EXPECT_EQ(0u, GetNumPendingUpdates(tree_id_));
  ASSERT_TRUE(AreAllPendingUpdatesEmpty());

  // Send update 1. Since speech is in progress, this will not be
  // unserialized yet.
  AccessibilityEventReceived({updates[1]}, /*speech_playing=*/true);
  EXPECT_EQ(1u, GetNumPendingUpdates(tree_id_));

  // Send update 2. This is still not unserialized yet.
  AccessibilityEventReceived({updates[2]}, /*speech_playing=*/true);
  EXPECT_EQ(2u, GetNumPendingUpdates(tree_id_));

  // Complete distillation which unserializes the pending updates and distills
  // them.
  UnserializePendingUpdates(tree_id_);
  EXPECT_EQ(0u, GetNumPendingUpdates(tree_id_));
  ASSERT_TRUE(AreAllPendingUpdatesEmpty());
}

TEST_F(ReadAnythingAppModelTest, ClearPendingUpdates_DeletesPendingUpdates) {
  EXPECT_EQ(0u, GetNumPendingUpdates(tree_id_));

  // Create a couple of updates which add additional nodes to the tree.
  std::vector<int> child_ids = {2, 3, 4};
  std::vector<ui::AXTreeUpdate> updates = CreateSimpleUpdateList(child_ids);

  AccessibilityEventReceived({updates[0]});
  EXPECT_EQ(0u, GetNumPendingUpdates(tree_id_));
  set_distillation_in_progress(true);
  AccessibilityEventReceived({updates[1]});
  EXPECT_EQ(1u, GetNumPendingUpdates(tree_id_));
  AccessibilityEventReceived({updates[2]});
  EXPECT_EQ(2u, GetNumPendingUpdates(tree_id_));

  // Clearing the pending updates correctly deletes the pending updates.
  ClearPendingUpdates();
  ASSERT_TRUE(AreAllPendingUpdatesEmpty());
}

TEST_F(ReadAnythingAppModelTest, ChangeActiveTreeWithPendingUpdates_UnknownID) {
  EXPECT_EQ(0u, GetNumPendingUpdates(tree_id_));
  ASSERT_TRUE(AreAllPendingUpdatesEmpty());

  // Create a couple of updates which add additional nodes to the tree.
  std::vector<int> child_ids = {2, 3, 4};
  std::vector<ui::AXTreeUpdate> updates = CreateSimpleUpdateList(child_ids);

  // Create an update which has no tree id.
  ui::AXTreeUpdate update;
  ui::AXNodeData node = test::GenericContainerNode(/* id= */ 1);
  update.nodes = {node};
  updates.push_back(update);

  // Add the three updates.
  AccessibilityEventReceived({updates[0]});
  EXPECT_EQ(0u, GetNumPendingUpdates(tree_id_));
  ASSERT_TRUE(AreAllPendingUpdatesEmpty());
  set_distillation_in_progress(true);
  AccessibilityEventReceived(tree_id_, {updates[1], updates[2]});
  EXPECT_EQ(2u, GetNumPendingUpdates(tree_id_));

  // Switch to a new active tree. Should not crash.
  SetActiveTreeId(ui::AXTreeIDUnknown());
}

TEST_F(ReadAnythingAppModelTest, DisplayNodeIdsContains_ContentNodes) {
  ui::AXTreeUpdate update;
  SetUpdateTreeID(&update);
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
  EXPECT_TRUE(DisplayNodeIdsContains(1));
  EXPECT_FALSE(DisplayNodeIdsContains(2));
  EXPECT_TRUE(DisplayNodeIdsContains(3));
  EXPECT_TRUE(DisplayNodeIdsContains(4));
  EXPECT_TRUE(DisplayNodeIdsContains(5));
  EXPECT_TRUE(DisplayNodeIdsContains(6));
}

TEST_F(ReadAnythingAppModelTest,
       DisplayNodeIdsDoesNotContain_InvisibleOrIgnoredNodes) {
  ui::AXTreeUpdate update;
  SetUpdateTreeID(&update);
  update.nodes.resize(3);
  update.nodes[0].id = 2;
  update.nodes[1].id = 3;
  update.nodes[1].AddState(ax::mojom::State::kInvisible);
  update.nodes[2].id = 4;
  update.nodes[2].AddState(ax::mojom::State::kIgnored);
  AccessibilityEventReceived({update});
  ProcessDisplayNodes({2, 3, 4});
  EXPECT_TRUE(DisplayNodeIdsContains(1));
  EXPECT_TRUE(DisplayNodeIdsContains(2));
  EXPECT_FALSE(DisplayNodeIdsContains(3));
  EXPECT_FALSE(DisplayNodeIdsContains(4));
}

TEST_F(ReadAnythingAppModelTest,
       DisplayNodeIdsEmpty_WhenContentNodesAreAllHeadings) {
  ui::AXTreeUpdate update;
  SetUpdateTreeID(&update);

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
  EXPECT_TRUE(DisplayNodeIdsIsEmpty());

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
  EXPECT_TRUE(DisplayNodeIdsIsEmpty());

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
  EXPECT_TRUE(DisplayNodeIdsIsEmpty());
}

TEST_F(ReadAnythingAppModelTest,
       SelectionNodeIdsContains_SelectionAndNearbyNodes) {
  ui::AXTreeUpdate update;
  SetUpdateTreeID(&update);
  update.tree_data.sel_anchor_object_id = 2;
  update.tree_data.sel_focus_object_id = 3;
  update.tree_data.sel_anchor_offset = 0;
  update.tree_data.sel_focus_offset = 0;
  update.tree_data.sel_is_backward = false;

  AccessibilityEventReceived({update});
  ProcessSelection();
  EXPECT_TRUE(SelectionNodeIdsContains(1));
  EXPECT_TRUE(SelectionNodeIdsContains(2));
  EXPECT_TRUE(SelectionNodeIdsContains(3));
  EXPECT_TRUE(SelectionNodeIdsContains(4));
}

TEST_F(ReadAnythingAppModelTest,
       SelectionNodeIdsContains_BackwardSelectionAndNearbyNodes) {
  ui::AXTreeUpdate update;
  SetUpdateTreeID(&update);
  update.tree_data.sel_anchor_object_id = 3;
  update.tree_data.sel_focus_object_id = 2;
  update.tree_data.sel_anchor_offset = 0;
  update.tree_data.sel_focus_offset = 0;
  update.tree_data.sel_is_backward = true;
  AccessibilityEventReceived({update});
  ProcessSelection();
  EXPECT_TRUE(SelectionNodeIdsContains(1));
  EXPECT_TRUE(SelectionNodeIdsContains(2));
  EXPECT_TRUE(SelectionNodeIdsContains(3));
  EXPECT_TRUE(SelectionNodeIdsContains(4));
}

TEST_F(ReadAnythingAppModelTest,
       SelectionNodeIdsDoesNotContain_InvisibleOrIgnoredNodes) {
  ui::AXTreeUpdate update;
  SetUpdateTreeID(&update);
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
  ProcessSelection();
  EXPECT_FALSE(DisplayNodeIdsContains(1));
  EXPECT_FALSE(SelectionNodeIdsContains(2));
  EXPECT_FALSE(SelectionNodeIdsContains(3));
  EXPECT_FALSE(SelectionNodeIdsContains(4));
}

TEST_F(ReadAnythingAppModelTest, Reset_ResetsState) {
  // Initial state.
  ui::AXTreeUpdate update;
  SetUpdateTreeID(&update);
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
  set_distillation_in_progress(true);

  // Assert initial state before resetting.
  ASSERT_TRUE(DistillationInProgress());

  ASSERT_TRUE(DisplayNodeIdsContains(1));
  ASSERT_TRUE(DisplayNodeIdsContains(3));
  ASSERT_TRUE(DisplayNodeIdsContains(4));
  ASSERT_TRUE(DisplayNodeIdsContains(5));
  ASSERT_TRUE(DisplayNodeIdsContains(6));

  Reset({1, 2});

  // Assert reset state.
  ASSERT_FALSE(DistillationInProgress());

  ASSERT_TRUE(ContentNodeIdsContains(1));
  ASSERT_TRUE(ContentNodeIdsContains(2));

  ASSERT_FALSE(DisplayNodeIdsContains(1));
  ASSERT_FALSE(DisplayNodeIdsContains(3));
  ASSERT_FALSE(DisplayNodeIdsContains(4));
  ASSERT_FALSE(DisplayNodeIdsContains(5));
  ASSERT_FALSE(DisplayNodeIdsContains(6));

  // Calling reset with different content nodes updates the content nodes.
  Reset({5, 4});
  ASSERT_FALSE(ContentNodeIdsContains(1));
  ASSERT_FALSE(ContentNodeIdsContains(2));
  ASSERT_TRUE(ContentNodeIdsContains(5));
  ASSERT_TRUE(ContentNodeIdsContains(4));
}

TEST_F(ReadAnythingAppModelTest, Reset_ResetsSelectionState) {
  // Initial state.
  ui::AXTreeUpdate update;
  SetUpdateTreeID(&update);
  update.tree_data.sel_anchor_object_id = 3;
  update.tree_data.sel_focus_object_id = 2;
  update.tree_data.sel_anchor_offset = 0;
  update.tree_data.sel_focus_offset = 0;
  update.tree_data.sel_is_backward = true;
  AccessibilityEventReceived({update});
  ProcessSelection();

  // Assert initial selection state.
  ASSERT_TRUE(SelectionNodeIdsContains(1));
  ASSERT_TRUE(SelectionNodeIdsContains(2));
  ASSERT_TRUE(SelectionNodeIdsContains(3));

  ASSERT_TRUE(HasSelection());

  ASSERT_NE(StartOffset(), -1);
  ASSERT_NE(EndOffset(), -1);

  ASSERT_NE(StartNodeId(), ui::kInvalidAXNodeID);
  ASSERT_NE(EndNodeId(), ui::kInvalidAXNodeID);

  Reset({1, 2});

  // Assert reset selection state.
  ASSERT_FALSE(SelectionNodeIdsContains(1));
  ASSERT_FALSE(SelectionNodeIdsContains(2));
  ASSERT_FALSE(SelectionNodeIdsContains(3));

  ASSERT_FALSE(HasSelection());

  ASSERT_EQ(StartOffset(), -1);
  ASSERT_EQ(EndOffset(), -1);

  ASSERT_EQ(StartNodeId(), ui::kInvalidAXNodeID);
  ASSERT_EQ(EndNodeId(), ui::kInvalidAXNodeID);
}

TEST_F(ReadAnythingAppModelTest, PostProcessSelection_SelectionStateCorrect) {
  // Initial state.
  ui::AXTreeUpdate update;
  SetUpdateTreeID(&update);
  update.tree_data.sel_anchor_object_id = 2;
  update.tree_data.sel_focus_object_id = 3;
  update.tree_data.sel_anchor_offset = 0;
  update.tree_data.sel_focus_offset = 0;
  update.tree_data.sel_is_backward = false;
  AccessibilityEventReceived({update});
  SetRequiresPostProcessSelection(true);
  ProcessSelection();

  ASSERT_FALSE(RequiresPostProcessSelection());
  ASSERT_TRUE(HasSelection());

  ASSERT_TRUE(SelectionNodeIdsContains(1));
  ASSERT_TRUE(SelectionNodeIdsContains(2));
  ASSERT_TRUE(SelectionNodeIdsContains(3));

  ASSERT_EQ(StartOffset(), 0);
  ASSERT_EQ(EndOffset(), 0);

  ASSERT_EQ(StartNodeId(), 2);
  ASSERT_EQ(EndNodeId(), 3);
}

TEST_F(ReadAnythingAppModelTest, PostProcessSelectionFromAction_DoesNotDraw) {
  // Initial state.
  ui::AXTreeUpdate update;
  SetUpdateTreeID(&update);
  update.tree_data.sel_anchor_object_id = 2;
  update.tree_data.sel_focus_object_id = 3;
  update.tree_data.sel_anchor_offset = 0;
  update.tree_data.sel_focus_offset = 0;
  update.tree_data.sel_is_backward = false;
  AccessibilityEventReceived({update});
  ProcessDisplayNodes({2, 3});
  SetSelectionFromAction(true);

  ASSERT_FALSE(ProcessSelection());
}

TEST_F(ReadAnythingAppModelTest,
       PostProcessSelection_OnFirstOpen_DrawsWithNonEmptySelectionInside) {
  ProcessDisplayNodes({2, 3});
  ui::AXTreeUpdate update;
  SetUpdateTreeID(&update);
  update.tree_data.sel_anchor_object_id = 2;
  update.tree_data.sel_focus_object_id = 2;
  update.tree_data.sel_anchor_offset = 0;
  update.tree_data.sel_focus_offset = 5;
  update.tree_data.sel_is_backward = false;
  AccessibilityEventReceived({update});
  SetSelectionFromAction(false);

  ASSERT_TRUE(ProcessSelection());
}

TEST_F(ReadAnythingAppModelTest,
       PostProcessSelection_OnFirstOpen_DrawsWithEmptySelectionInside) {
  ProcessDisplayNodes({2, 3});
  ui::AXTreeUpdate update;
  SetUpdateTreeID(&update);
  update.tree_data.sel_anchor_object_id = 2;
  update.tree_data.sel_focus_object_id = 2;
  update.tree_data.sel_anchor_offset = 5;
  update.tree_data.sel_focus_offset = 5;
  update.tree_data.sel_is_backward = false;
  AccessibilityEventReceived({update});
  SetSelectionFromAction(false);

  ASSERT_TRUE(ProcessSelection());
}

TEST_F(ReadAnythingAppModelTest,
       PostProcessSelection_OnFirstOpen_DrawsWithNonEmptySelectionOutside) {
  ProcessDisplayNodes({2, 3});
  ui::AXTreeUpdate update;
  SetUpdateTreeID(&update);
  update.tree_data.sel_anchor_object_id = 4;
  update.tree_data.sel_focus_object_id = 4;
  update.tree_data.sel_anchor_offset = 0;
  update.tree_data.sel_focus_offset = 5;
  update.tree_data.sel_is_backward = false;
  AccessibilityEventReceived({update});
  SetSelectionFromAction(false);

  ASSERT_TRUE(ProcessSelection());
}

TEST_F(ReadAnythingAppModelTest,
       PostProcessSelection__OnFirstOpen_DrawsWithEmptySelectionOutside) {
  ProcessDisplayNodes({2, 3});
  ui::AXTreeUpdate update;
  SetUpdateTreeID(&update);
  update.tree_data.sel_anchor_object_id = 4;
  update.tree_data.sel_focus_object_id = 4;
  update.tree_data.sel_anchor_offset = 0;
  update.tree_data.sel_focus_offset = 0;
  update.tree_data.sel_is_backward = false;
  AccessibilityEventReceived({update});
  SetSelectionFromAction(false);

  ASSERT_TRUE(ProcessSelection());
}

TEST_F(ReadAnythingAppModelTest,
       PostProcessSelection_EmptyInside_AfterNonEmptyInside_DoesNotDraw) {
  ProcessDisplayNodes({2, 3});

  // Non-empty selection inside display nodes.
  ui::AXTreeUpdate update1;
  SetUpdateTreeID(&update1);
  update1.tree_data.sel_anchor_object_id = 2;
  update1.tree_data.sel_focus_object_id = 2;
  update1.tree_data.sel_anchor_offset = 0;
  update1.tree_data.sel_focus_offset = 5;
  update1.tree_data.sel_is_backward = false;
  AccessibilityEventReceived({update1});
  SetSelectionFromAction(false);
  ProcessSelection();

  // Empty selection inside display nodes.
  ui::AXTreeUpdate update2;
  SetUpdateTreeID(&update2);
  update2.tree_data.sel_anchor_object_id = 3;
  update2.tree_data.sel_focus_object_id = 3;
  update2.tree_data.sel_anchor_offset = 2;
  update2.tree_data.sel_focus_offset = 2;
  update2.tree_data.sel_is_backward = false;
  AccessibilityEventReceived({update2});
  SetSelectionFromAction(false);

  ASSERT_FALSE(ProcessSelection());
}

TEST_F(ReadAnythingAppModelTest,
       PostProcessSelection_EmptyInside_AfterEmptyInside_DoesNotDraw) {
  ProcessDisplayNodes({2, 3});

  // Empty selection inside display nodes.
  ui::AXTreeUpdate update1;
  SetUpdateTreeID(&update1);
  update1.tree_data.sel_anchor_object_id = 2;
  update1.tree_data.sel_focus_object_id = 2;
  update1.tree_data.sel_anchor_offset = 0;
  update1.tree_data.sel_focus_offset = 0;
  update1.tree_data.sel_is_backward = false;
  AccessibilityEventReceived({update1});
  SetSelectionFromAction(false);
  ProcessSelection();

  // Different empty selection inside display nodes.
  ui::AXTreeUpdate update2;
  SetUpdateTreeID(&update2);
  update2.tree_data.sel_anchor_object_id = 3;
  update2.tree_data.sel_focus_object_id = 3;
  update2.tree_data.sel_anchor_offset = 2;
  update2.tree_data.sel_focus_offset = 2;
  update2.tree_data.sel_is_backward = false;
  AccessibilityEventReceived({update2});
  SetSelectionFromAction(false);

  ASSERT_FALSE(ProcessSelection());
}

TEST_F(ReadAnythingAppModelTest,
       PostProcessSelection_NonEmptyInside_AfterEmptyInside_DoesNotDraw) {
  ProcessDisplayNodes({2, 3});

  // Empty selection inside display nodes.
  ui::AXTreeUpdate update1;
  SetUpdateTreeID(&update1);
  update1.tree_data.sel_anchor_object_id = 3;
  update1.tree_data.sel_focus_object_id = 3;
  update1.tree_data.sel_anchor_offset = 2;
  update1.tree_data.sel_focus_offset = 2;
  update1.tree_data.sel_is_backward = false;
  AccessibilityEventReceived({update1});
  SetSelectionFromAction(false);
  ProcessSelection();

  // Non-empty selection inside display nodes.
  ui::AXTreeUpdate update2;
  SetUpdateTreeID(&update2);
  update2.tree_data.sel_anchor_object_id = 2;
  update2.tree_data.sel_focus_object_id = 2;
  update2.tree_data.sel_anchor_offset = 0;
  update2.tree_data.sel_focus_offset = 5;
  update2.tree_data.sel_is_backward = false;
  AccessibilityEventReceived({update2});
  SetSelectionFromAction(false);

  ASSERT_FALSE(ProcessSelection());
}

TEST_F(ReadAnythingAppModelTest,
       PostProcessSelection_NonEmptyInside_AfterNonEmptyInside_DoesNotDraw) {
  ProcessDisplayNodes({2, 3});

  // Non-empty selection inside display nodes.
  ui::AXTreeUpdate update1;
  SetUpdateTreeID(&update1);
  update1.tree_data.sel_anchor_object_id = 3;
  update1.tree_data.sel_focus_object_id = 3;
  update1.tree_data.sel_anchor_offset = 2;
  update1.tree_data.sel_focus_offset = 6;
  update1.tree_data.sel_is_backward = false;
  AccessibilityEventReceived({update1});
  SetSelectionFromAction(false);
  ProcessSelection();

  // Different non-empty selection inside display nodes.
  ui::AXTreeUpdate update2;
  SetUpdateTreeID(&update2);
  update2.tree_data.sel_anchor_object_id = 2;
  update2.tree_data.sel_focus_object_id = 3;
  update2.tree_data.sel_anchor_offset = 0;
  update2.tree_data.sel_focus_offset = 5;
  update2.tree_data.sel_is_backward = false;
  AccessibilityEventReceived({update2});
  SetSelectionFromAction(false);

  ASSERT_FALSE(ProcessSelection());
}

TEST_F(ReadAnythingAppModelTest,
       PostProcessSelection_EmptyOutside_AfterNonEmptyOutside_DoesDraw) {
  ProcessDisplayNodes({2, 3});

  // Non-empty selection outside display nodes.
  ui::AXTreeUpdate update1;
  SetUpdateTreeID(&update1);
  update1.tree_data.sel_anchor_object_id = 4;
  update1.tree_data.sel_focus_object_id = 4;
  update1.tree_data.sel_anchor_offset = 0;
  update1.tree_data.sel_focus_offset = 5;
  update1.tree_data.sel_is_backward = false;
  AccessibilityEventReceived({update1});
  SetSelectionFromAction(false);
  ProcessSelection();

  // Empty selection outside display nodes.
  ui::AXTreeUpdate update2;
  SetUpdateTreeID(&update2);
  update2.tree_data.sel_anchor_object_id = 4;
  update2.tree_data.sel_focus_object_id = 4;
  update2.tree_data.sel_anchor_offset = 2;
  update2.tree_data.sel_focus_offset = 2;
  update2.tree_data.sel_is_backward = false;
  AccessibilityEventReceived({update2});
  SetSelectionFromAction(false);

  ASSERT_TRUE(ProcessSelection());
}

TEST_F(ReadAnythingAppModelTest,
       PostProcessSelection_EmptyOutside_AfterEmptyOutside_DoesNotDraw) {
  ProcessDisplayNodes({2, 3});

  // Empty selection outside display nodes.
  ui::AXTreeUpdate update1;
  SetUpdateTreeID(&update1);
  update1.tree_data.sel_anchor_object_id = 4;
  update1.tree_data.sel_focus_object_id = 4;
  update1.tree_data.sel_anchor_offset = 0;
  update1.tree_data.sel_focus_offset = 0;
  update1.tree_data.sel_is_backward = false;
  AccessibilityEventReceived({update1});
  SetSelectionFromAction(false);
  ProcessSelection();

  // Different empty selection outside display nodes.
  ui::AXTreeUpdate update2;
  SetUpdateTreeID(&update2);
  update2.tree_data.sel_anchor_object_id = 4;
  update2.tree_data.sel_focus_object_id = 4;
  update2.tree_data.sel_anchor_offset = 2;
  update2.tree_data.sel_focus_offset = 2;
  update2.tree_data.sel_is_backward = false;
  AccessibilityEventReceived({update2});
  SetSelectionFromAction(false);

  ASSERT_FALSE(ProcessSelection());
}

TEST_F(ReadAnythingAppModelTest,
       PostProcessSelection_NonEmptyOutside_AfterEmptyOutside_DoesDraw) {
  ProcessDisplayNodes({2, 3});

  // Empty selection outside display nodes.
  ui::AXTreeUpdate update1;
  SetUpdateTreeID(&update1);
  update1.tree_data.sel_anchor_object_id = 4;
  update1.tree_data.sel_focus_object_id = 4;
  update1.tree_data.sel_anchor_offset = 2;
  update1.tree_data.sel_focus_offset = 2;
  update1.tree_data.sel_is_backward = false;
  AccessibilityEventReceived({update1});
  SetSelectionFromAction(false);
  ProcessSelection();

  // Non-empty selection outside display nodes.
  ui::AXTreeUpdate update2;
  SetUpdateTreeID(&update2);
  update2.tree_data.sel_anchor_object_id = 4;
  update2.tree_data.sel_focus_object_id = 4;
  update2.tree_data.sel_anchor_offset = 0;
  update2.tree_data.sel_focus_offset = 5;
  update2.tree_data.sel_is_backward = false;
  AccessibilityEventReceived({update2});
  SetSelectionFromAction(false);

  ASSERT_TRUE(ProcessSelection());
}

TEST_F(ReadAnythingAppModelTest,
       PostProcessSelection_NonEmptyOutside_AfterNonEmptyOutside_DoesDraw) {
  ProcessDisplayNodes({2, 3});

  // Non-empty selection outside display nodes.
  ui::AXTreeUpdate update1;
  SetUpdateTreeID(&update1);
  update1.tree_data.sel_anchor_object_id = 4;
  update1.tree_data.sel_focus_object_id = 4;
  update1.tree_data.sel_anchor_offset = 2;
  update1.tree_data.sel_focus_offset = 6;
  update1.tree_data.sel_is_backward = false;
  AccessibilityEventReceived({update1});
  SetSelectionFromAction(false);
  ProcessSelection();

  // Different non-empty selection outside display nodes.
  ui::AXTreeUpdate update2;
  SetUpdateTreeID(&update2);
  update2.tree_data.sel_anchor_object_id = 4;
  update2.tree_data.sel_focus_object_id = 4;
  update2.tree_data.sel_anchor_offset = 0;
  update2.tree_data.sel_focus_offset = 5;
  update2.tree_data.sel_is_backward = false;
  AccessibilityEventReceived({update2});
  SetSelectionFromAction(false);

  ASSERT_TRUE(ProcessSelection());
}

TEST_F(ReadAnythingAppModelTest,
       PostProcessSelection_EmptyInside_AfterNonEmptyOutside_DoesDraw) {
  ProcessDisplayNodes({2, 3});

  // Non-empty selection outside display nodes.
  ui::AXTreeUpdate update1;
  SetUpdateTreeID(&update1);
  update1.tree_data.sel_anchor_object_id = 4;
  update1.tree_data.sel_focus_object_id = 4;
  update1.tree_data.sel_anchor_offset = 0;
  update1.tree_data.sel_focus_offset = 5;
  update1.tree_data.sel_is_backward = false;
  AccessibilityEventReceived({update1});
  SetSelectionFromAction(false);
  ProcessSelection();

  // Empty selection inside display nodes.
  ui::AXTreeUpdate update2;
  SetUpdateTreeID(&update2);
  update2.tree_data.sel_anchor_object_id = 2;
  update2.tree_data.sel_focus_object_id = 2;
  update2.tree_data.sel_anchor_offset = 2;
  update2.tree_data.sel_focus_offset = 2;
  update2.tree_data.sel_is_backward = false;
  AccessibilityEventReceived({update2});
  SetSelectionFromAction(false);

  ASSERT_TRUE(ProcessSelection());
}

TEST_F(ReadAnythingAppModelTest,
       PostProcessSelection_EmptyInside_AfterEmptyOutside_DoesNotDraw) {
  ProcessDisplayNodes({2, 3});

  // Empty selection outside display nodes.
  ui::AXTreeUpdate update1;
  SetUpdateTreeID(&update1);
  update1.tree_data.sel_anchor_object_id = 4;
  update1.tree_data.sel_focus_object_id = 4;
  update1.tree_data.sel_anchor_offset = 0;
  update1.tree_data.sel_focus_offset = 0;
  update1.tree_data.sel_is_backward = false;
  AccessibilityEventReceived({update1});
  SetSelectionFromAction(false);
  ProcessSelection();

  // Empty selection inside display nodes.
  ui::AXTreeUpdate update2;
  SetUpdateTreeID(&update2);
  update2.tree_data.sel_anchor_object_id = 2;
  update2.tree_data.sel_focus_object_id = 2;
  update2.tree_data.sel_anchor_offset = 2;
  update2.tree_data.sel_focus_offset = 2;
  update2.tree_data.sel_is_backward = false;
  AccessibilityEventReceived({update2});
  SetSelectionFromAction(false);

  ASSERT_FALSE(ProcessSelection());
}

TEST_F(ReadAnythingAppModelTest,
       PostProcessSelection_NonEmptyInside_AfterEmptyOutside_DoesNotDraw) {
  ProcessDisplayNodes({2, 3});

  // Empty selection outside display nodes.
  ui::AXTreeUpdate update1;
  SetUpdateTreeID(&update1);
  update1.tree_data.sel_anchor_object_id = 4;
  update1.tree_data.sel_focus_object_id = 4;
  update1.tree_data.sel_anchor_offset = 2;
  update1.tree_data.sel_focus_offset = 2;
  update1.tree_data.sel_is_backward = false;
  AccessibilityEventReceived({update1});
  SetSelectionFromAction(false);
  ProcessSelection();

  // Non-empty selection inside display nodes.
  ui::AXTreeUpdate update2;
  SetUpdateTreeID(&update2);
  update2.tree_data.sel_anchor_object_id = 2;
  update2.tree_data.sel_focus_object_id = 3;
  update2.tree_data.sel_anchor_offset = 0;
  update2.tree_data.sel_focus_offset = 5;
  update2.tree_data.sel_is_backward = false;
  AccessibilityEventReceived({update2});
  SetSelectionFromAction(false);

  ASSERT_FALSE(ProcessSelection());
}

TEST_F(ReadAnythingAppModelTest,
       PostProcessSelection_NonEmptyInside_AfterNonEmptyOutside_DoesDraw) {
  ProcessDisplayNodes({2, 3});

  // Non-empty selection outside display nodes.
  ui::AXTreeUpdate update1;
  SetUpdateTreeID(&update1);
  update1.tree_data.sel_anchor_object_id = 4;
  update1.tree_data.sel_focus_object_id = 4;
  update1.tree_data.sel_anchor_offset = 2;
  update1.tree_data.sel_focus_offset = 6;
  update1.tree_data.sel_is_backward = false;
  AccessibilityEventReceived({update1});
  SetSelectionFromAction(false);
  ProcessSelection();

  // Non-empty selection inside display nodes.
  ui::AXTreeUpdate update2;
  SetUpdateTreeID(&update2);
  update2.tree_data.sel_anchor_object_id = 2;
  update2.tree_data.sel_focus_object_id = 2;
  update2.tree_data.sel_anchor_offset = 0;
  update2.tree_data.sel_focus_offset = 5;
  update2.tree_data.sel_is_backward = false;
  AccessibilityEventReceived({update2});
  SetSelectionFromAction(false);

  ASSERT_TRUE(ProcessSelection());
}

TEST_F(ReadAnythingAppModelTest,
       PostProcessSelection_EmptyOutside_AfterNonEmptyInside_DoesNotDraw) {
  ProcessDisplayNodes({2, 3});

  // Non-empty selection inside display nodes.
  ui::AXTreeUpdate update1;
  SetUpdateTreeID(&update1);
  update1.tree_data.sel_anchor_object_id = 2;
  update1.tree_data.sel_focus_object_id = 3;
  update1.tree_data.sel_anchor_offset = 0;
  update1.tree_data.sel_focus_offset = 5;
  update1.tree_data.sel_is_backward = false;
  AccessibilityEventReceived({update1});
  SetSelectionFromAction(false);
  ProcessSelection();

  // Empty selection outside display nodes.
  ui::AXTreeUpdate update2;
  SetUpdateTreeID(&update2);
  update2.tree_data.sel_anchor_object_id = 4;
  update2.tree_data.sel_focus_object_id = 4;
  update2.tree_data.sel_anchor_offset = 2;
  update2.tree_data.sel_focus_offset = 2;
  update2.tree_data.sel_is_backward = false;
  AccessibilityEventReceived({update2});
  SetSelectionFromAction(false);

  ASSERT_FALSE(ProcessSelection());
}

TEST_F(ReadAnythingAppModelTest,
       PostProcessSelection_EmptyOutside_AfterEmptyInside_DoesNotDraw) {
  ProcessDisplayNodes({2, 3});

  // Empty selection inside display nodes.
  ui::AXTreeUpdate update1;
  SetUpdateTreeID(&update1);
  update1.tree_data.sel_anchor_object_id = 2;
  update1.tree_data.sel_focus_object_id = 2;
  update1.tree_data.sel_anchor_offset = 0;
  update1.tree_data.sel_focus_offset = 0;
  update1.tree_data.sel_is_backward = false;
  AccessibilityEventReceived({update1});
  SetSelectionFromAction(false);
  ProcessSelection();

  // Empty selection outside display nodes.
  ui::AXTreeUpdate update2;
  SetUpdateTreeID(&update2);
  update2.tree_data.sel_anchor_object_id = 4;
  update2.tree_data.sel_focus_object_id = 4;
  update2.tree_data.sel_anchor_offset = 2;
  update2.tree_data.sel_focus_offset = 2;
  update2.tree_data.sel_is_backward = false;
  AccessibilityEventReceived({update2});
  SetSelectionFromAction(false);

  ASSERT_FALSE(ProcessSelection());
}

TEST_F(ReadAnythingAppModelTest,
       PostProcessSelection_NonEmptyOutside_AfterEmptyInside_DoesDraw) {
  ProcessDisplayNodes({2, 3});

  // Empty selection inside display nodes.
  ui::AXTreeUpdate update1;
  SetUpdateTreeID(&update1);
  update1.tree_data.sel_anchor_object_id = 2;
  update1.tree_data.sel_focus_object_id = 2;
  update1.tree_data.sel_anchor_offset = 2;
  update1.tree_data.sel_focus_offset = 2;
  update1.tree_data.sel_is_backward = false;
  AccessibilityEventReceived({update1});
  SetSelectionFromAction(false);
  ProcessSelection();

  // Non-empty selection outside display nodes.
  ui::AXTreeUpdate update2;
  SetUpdateTreeID(&update2);
  update2.tree_data.sel_anchor_object_id = 4;
  update2.tree_data.sel_focus_object_id = 4;
  update2.tree_data.sel_anchor_offset = 0;
  update2.tree_data.sel_focus_offset = 5;
  update2.tree_data.sel_is_backward = false;
  AccessibilityEventReceived({update2});
  SetSelectionFromAction(false);

  ASSERT_TRUE(ProcessSelection());
}

TEST_F(ReadAnythingAppModelTest,
       PostProcessSelection_NonEmptyOutside_AfterNonEmptyInside_DoesDraw) {
  ProcessDisplayNodes({2, 3});

  // Non-empty selection inside display nodes.
  ui::AXTreeUpdate update1;
  SetUpdateTreeID(&update1);
  update1.tree_data.sel_anchor_object_id = 2;
  update1.tree_data.sel_focus_object_id = 2;
  update1.tree_data.sel_anchor_offset = 2;
  update1.tree_data.sel_focus_offset = 6;
  update1.tree_data.sel_is_backward = false;
  AccessibilityEventReceived({update1});
  SetSelectionFromAction(false);
  ProcessSelection();

  // Non-empty selection outside display nodes.
  ui::AXTreeUpdate update2;
  SetUpdateTreeID(&update2);
  update2.tree_data.sel_anchor_object_id = 4;
  update2.tree_data.sel_focus_object_id = 4;
  update2.tree_data.sel_anchor_offset = 0;
  update2.tree_data.sel_focus_offset = 5;
  update2.tree_data.sel_is_backward = false;
  AccessibilityEventReceived({update2});
  SetSelectionFromAction(false);

  ASSERT_TRUE(ProcessSelection());
}

TEST_F(ReadAnythingAppModelTest,
       StartAndEndNodesHaveDifferentParents_SelectionStateCorrect) {
  ui::AXTreeUpdate update;
  SetUpdateTreeID(&update);

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
  ProcessSelection();

  ASSERT_TRUE(HasSelection());
  ASSERT_EQ(StartNodeId(), 2);
  ASSERT_EQ(EndNodeId(), 5);

  // 1 and 3 are ancestors, so they are included as selection nodes..
  ASSERT_TRUE(SelectionNodeIdsContains(1));
  ASSERT_TRUE(SelectionNodeIdsContains(3));

  ASSERT_TRUE(SelectionNodeIdsContains(5));
  ASSERT_TRUE(SelectionNodeIdsContains(6));

  // Even though 3 is a generic container with more than one child, its
  // sibling nodes are included in the selection because the start node
  // includes it.
  ASSERT_TRUE(SelectionNodeIdsContains(2));
  ASSERT_TRUE(SelectionNodeIdsContains(3));
}

TEST_F(ReadAnythingAppModelTest,
       SelectionParentIsLinkAndInlineBlock_SelectionStateCorrect) {
  ui::AXTreeUpdate update;
  SetUpdateTreeID(&update);

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
  ProcessSelection();

  ASSERT_TRUE(HasSelection());
  ASSERT_EQ(StartNodeId(), 4);
  ASSERT_EQ(EndNodeId(), 4);

  ASSERT_TRUE(SelectionNodeIdsContains(1));
  ASSERT_FALSE(SelectionNodeIdsContains(2));
  ASSERT_TRUE(SelectionNodeIdsContains(3));
  ASSERT_TRUE(SelectionNodeIdsContains(4));
}

TEST_F(ReadAnythingAppModelTest,
       SelectionParentIsListItem_SelectionStateCorrect) {
  ui::AXTreeUpdate update;
  SetUpdateTreeID(&update);

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
  ProcessSelection();

  ASSERT_TRUE(HasSelection());
  ASSERT_EQ(StartNodeId(), 4);
  ASSERT_EQ(EndNodeId(), 4);

  ASSERT_TRUE(SelectionNodeIdsContains(1));
  ASSERT_FALSE(SelectionNodeIdsContains(2));
  ASSERT_TRUE(SelectionNodeIdsContains(3));
  ASSERT_TRUE(SelectionNodeIdsContains(4));
}

TEST_F(ReadAnythingAppModelTest,
       SelectionParentIsGenericContainerAndInline_SelectionStateCorrect) {
  ui::AXTreeUpdate update;
  SetUpdateTreeID(&update);
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
  ProcessSelection();

  ASSERT_TRUE(HasSelection());
  ASSERT_EQ(StartNodeId(), 4);
  ASSERT_EQ(EndNodeId(), 4);

  ASSERT_TRUE(SelectionNodeIdsContains(1));
  ASSERT_FALSE(SelectionNodeIdsContains(2));
  ASSERT_TRUE(SelectionNodeIdsContains(3));
  ASSERT_TRUE(SelectionNodeIdsContains(4));
}
TEST_F(
    ReadAnythingAppModelTest,
    SelectionParentIsGenericContainerWithMultipleChildren_SelectionStateCorrect) {
  ui::AXTreeUpdate update;
  SetUpdateTreeID(&update);
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
  ProcessSelection();

  ASSERT_TRUE(HasSelection());
  ASSERT_EQ(StartNodeId(), 4);
  ASSERT_EQ(EndNodeId(), 5);

  // 1 and 3 are ancestors, so they are included as selection nodes..
  ASSERT_TRUE(SelectionNodeIdsContains(1));
  ASSERT_TRUE(SelectionNodeIdsContains(3));
  ASSERT_TRUE(SelectionNodeIdsContains(4));
  ASSERT_TRUE(SelectionNodeIdsContains(5));

  // Since 3 is a generic container with more than one child, its sibling nodes
  // are not included, so 2 is ignored.
  ASSERT_FALSE(SelectionNodeIdsContains(2));
}

TEST_F(ReadAnythingAppModelTest, ResetTextSize_ReturnsTextSizeToDefault) {
  IncreaseTextSize();
  IncreaseTextSize();
  IncreaseTextSize();
  ASSERT_GT(FontSize(), kReadAnythingDefaultFontScale);

  ResetTextSize();
  ASSERT_EQ(FontSize(), kReadAnythingDefaultFontScale);

  DecreaseTextSize();
  DecreaseTextSize();
  DecreaseTextSize();
  ASSERT_LT(FontSize(), kReadAnythingDefaultFontScale);

  ResetTextSize();
  ASSERT_EQ(FontSize(), kReadAnythingDefaultFontScale);
}

TEST_F(ReadAnythingAppModelTest, LanguageCode_ReturnsCorrectCode) {
  ASSERT_EQ(LanguageCode(), "en");

  SetLanguageCode("es");
  ASSERT_EQ(LanguageCode(), "es");
}

TEST_F(ReadAnythingAppModelTest,
       SupportedFonts_InvalidLanguageCode_ReturnsDefaultFonts) {
  SetLanguageCode("qr");
  std::vector<std::string> expectedFonts = {"Sans-serif", "Serif"};
  std::vector<std::string> fonts = GetSupportedFonts();

  EXPECT_EQ(fonts.size(), expectedFonts.size());
  for (size_t i = 0; i < fonts.size(); i++) {
    ASSERT_EQ(fonts[i], expectedFonts[i]);
  }
}

TEST_F(ReadAnythingAppModelTest,
       SupportedFonts_BeforeLanguageSet_ReturnsDefaultFonts) {
  std::vector<std::string> expectedFonts = {
      "Poppins",       "Sans-serif",  "Serif",
      "Comic Neue",    "Lexend Deca", "EB Garamond",
      "STIX Two Text", "Andika",      "Atkinson Hyperlegible"};
  std::vector<std::string> fonts = GetSupportedFonts();

  EXPECT_EQ(fonts.size(), expectedFonts.size());
  for (size_t i = 0; i < fonts.size(); i++) {
    ASSERT_EQ(fonts[i], expectedFonts[i]);
  }
}

TEST_F(ReadAnythingAppModelTest,
       SupportedFonts_SetLanguageCode_ReturnsExpectedDefaultFonts) {
  // Spanish
  SetLanguageCode("es");
  std::vector<std::string> expectedFonts = {
      "Poppins",       "Sans-serif",  "Serif",
      "Comic Neue",    "Lexend Deca", "EB Garamond",
      "STIX Two Text", "Andika",      "Atkinson Hyperlegible"};
  std::vector<std::string> fonts = GetSupportedFonts();

  EXPECT_EQ(fonts.size(), expectedFonts.size());
  for (size_t i = 0; i < fonts.size(); i++) {
    ASSERT_EQ(fonts[i], expectedFonts[i]);
  }

  // Bulgarian
  SetLanguageCode("bg");
  expectedFonts = {"Sans-serif", "Serif", "EB Garamond", "STIX Two Text",
                   "Andika"};
  fonts = GetSupportedFonts();

  EXPECT_EQ(fonts.size(), expectedFonts.size());
  for (size_t i = 0; i < fonts.size(); i++) {
    ASSERT_EQ(fonts[i], expectedFonts[i]);
  }

  // Hindi
  SetLanguageCode("hi");
  expectedFonts = {"Poppins", "Sans-serif", "Serif"};
  fonts = GetSupportedFonts();

  EXPECT_EQ(fonts.size(), expectedFonts.size());
  for (size_t i = 0; i < fonts.size(); i++) {
    ASSERT_EQ(fonts[i], expectedFonts[i]);
  }
}

TEST_F(ReadAnythingAppModelTest, PdfEvents_SetRequiresDistillation) {
  set_is_pdf(true);

  ui::AXTreeUpdate initial_update;
  SetUpdateTreeID(&initial_update);
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
  SetUpdateTreeID(&update);
  update.root_id = 1;
  ui::AXNodeData node;
  node.id = 1;
  node.role = ax::mojom::Role::kPdfRoot;
  node.SetNameChecked("example.pdf");
  update.nodes = {node};
  AccessibilityEventReceived({update});
  ASSERT_FALSE(RequiresDistillation());

  // Tree update with PDF contents (new nodes added).
  ui::AXTreeUpdate update2;
  SetUpdateTreeID(&update2);
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
  ASSERT_TRUE(RequiresDistillation());
}

TEST_F(ReadAnythingAppModelTest, PdfEvents_DontSetRequiresDistillation) {
  set_is_pdf(true);

  ui::AXTreeUpdate initial_update;
  SetUpdateTreeID(&initial_update);
  initial_update.root_id = 1;
  ui::AXNodeData node;
  node.id = 1;
  node.role = ax::mojom::Role::kPdfRoot;
  initial_update.nodes = {node};
  AccessibilityEventReceived({initial_update});

  // Updates that don't create a new subtree, for example, a role change, should
  // not set requires_distillation_.
  ui::AXTreeUpdate update;
  SetUpdateTreeID(&update);
  ui::AXNodeData static_text_node = test::TextNode(/* id= */ 1);
  update.root_id = static_text_node.id;
  update.nodes = {static_text_node};
  AccessibilityEventReceived({update});
  ASSERT_FALSE(RequiresDistillation());
}

TEST_F(ReadAnythingAppModelTest, OnSelection_HandlesClickAndDragEvents) {
  ui::AXTreeUpdate update;
  SetUpdateTreeID(&update);
  update.tree_data.sel_anchor_object_id = 2;
  update.tree_data.sel_focus_object_id = 3;
  update.tree_data.sel_anchor_offset = 0;
  update.tree_data.sel_focus_offset = 0;
  update.tree_data.sel_is_backward = false;
  AccessibilityEventReceived({update});
  ProcessSelection();

  // If there is a click and drag selection (the anchor object id and offset are
  // the same as the prev selection received), the event_from eventually changes
  // from kUser to kPage. Post process selection should be required in either
  // case.
  // SetRequiresPostProcessSelection(false) is needed to reset the flag to check
  // that OnSelection(...) properly sets (or doesn't set) the flag.
  update.tree_data.sel_anchor_object_id = 2;
  update.tree_data.sel_focus_object_id = 3;
  update.tree_data.sel_anchor_offset = 0;
  update.tree_data.sel_focus_offset = 1;
  update.tree_data.sel_is_backward = false;
  AccessibilityEventReceived({update});

  SetRequiresPostProcessSelection(false);
  OnSelection(ax::mojom::EventFrom::kUser);
  EXPECT_TRUE(RequiresPostProcessSelection());

  SetRequiresPostProcessSelection(false);
  OnSelection(ax::mojom::EventFrom::kPage);
  EXPECT_TRUE(RequiresPostProcessSelection());

  // If the user drags the selection so that it is backwards, post process
  // selection should still be required.
  update.tree_data.sel_anchor_object_id = 2;
  update.tree_data.sel_focus_object_id = 1;
  update.tree_data.sel_anchor_offset = 0;
  update.tree_data.sel_focus_offset = 2;
  update.tree_data.sel_is_backward = true;
  AccessibilityEventReceived({update});
  SetRequiresPostProcessSelection(false);
  OnSelection(ax::mojom::EventFrom::kPage);
  EXPECT_TRUE(RequiresPostProcessSelection());

  // If the anchor changes (the user stopped dragging their cursor) and we
  // receive an event with event_from kPage, post process selection should not
  // be set to true.
  update.tree_data.sel_anchor_object_id = 2;
  update.tree_data.sel_focus_object_id = 3;
  update.tree_data.sel_anchor_offset = 1;
  update.tree_data.sel_focus_offset = 0;
  update.tree_data.sel_is_backward = false;
  AccessibilityEventReceived({update});
  SetRequiresPostProcessSelection(false);
  OnSelection(ax::mojom::EventFrom::kPage);
  EXPECT_FALSE(RequiresPostProcessSelection());
}

TEST_F(ReadAnythingAppModelTest, LastExpandedNodeNamedChanged_TriggersRedraw) {
  ui::AXTreeUpdate initial_update;
  SetUpdateTreeID(&initial_update);
  ui::AXNodeData initial_node = test::TextNode(/* id= */ 2, u"Old Name");
  initial_update.nodes = {initial_node};
  AccessibilityEventReceived({initial_update});

  ui::AXTreeUpdate update;
  SetUpdateTreeID(&update);
  ui::AXNodeData updated_node = test::TextNode(initial_node.id, u"New Name");
  update.nodes = {updated_node};
  SetLastExpandedNodeId(initial_node.id);
  EXPECT_EQ(LastExpandedNodeId(), initial_node.id);
  AccessibilityEventReceived({update});

  EXPECT_FALSE(RequiresPostProcessSelection());
  EXPECT_TRUE(RequiresRedraw());
  EXPECT_EQ(LastExpandedNodeId(), ui::kInvalidAXNodeID);
  // Check selection reset.
  EXPECT_FALSE(HasSelection());
  EXPECT_EQ(StartOffset(), -1);
  EXPECT_EQ(EndOffset(), -1);
  EXPECT_EQ(StartNodeId(), ui::kInvalidAXNodeID);
  EXPECT_EQ(EndNodeId(), ui::kInvalidAXNodeID);
  EXPECT_TRUE(SelectionNodeIdsEmpty());
}

TEST_F(ReadAnythingAppModelTest, ContentEditableValueChanged_ResetsDrawTimer) {
  ui::AXTreeUpdate update;
  SetUpdateTreeID(&update);
  ui::AXNodeData node1;
  node1.id = 1;
  update.nodes = {node1};

  ui::AXEvent event;
  event.id = node1.id;
  event.event_type = ax::mojom::Event::kValueChanged;
  // This update changes the structure of the tree. When the controller receives
  // it in AccessibilityEventReceived, it will re-distill the tree.
  AccessibilityEventReceived(update.tree_data.tree_id, {update}, {event});
  EXPECT_TRUE(DrawTimerReset());
}
