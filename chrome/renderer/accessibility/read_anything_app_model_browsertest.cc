// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/raw_ptr.h"
#include "base/threading/platform_thread.h"
#include "chrome/renderer/accessibility/read_anything_app_model.h"

#include "chrome/test/base/chrome_render_view_test.h"
#include "ui/accessibility/ax_enums.mojom-shared.h"
#include "ui/accessibility/ax_serializable_tree.h"

class ReadAnythingAppModelTest : public ChromeRenderViewTest {
 public:
  ReadAnythingAppModelTest() = default;
  ~ReadAnythingAppModelTest() override = default;
  ReadAnythingAppModelTest(const ReadAnythingAppModelTest&) = delete;
  ReadAnythingAppModelTest& operator=(const ReadAnythingAppModelTest&) = delete;

  void SetUp() override {
    ChromeRenderViewTest::SetUp();
    model_ = new ReadAnythingAppModel();

    // Create a tree id.
    tree_id_ = ui::AXTreeID::CreateNewAXTreeID();

    // Create simple AXTreeUpdate with a root node and 3 children.
    ui::AXTreeUpdate snapshot;
    snapshot.root_id = 1;
    snapshot.nodes.resize(4);
    snapshot.nodes[0].id = 1;
    snapshot.nodes[0].child_ids = {2, 3, 4};
    snapshot.nodes[1].id = 2;
    snapshot.nodes[2].id = 3;
    snapshot.nodes[3].id = 4;
    SetUpdateTreeID(&snapshot);

    AccessibilityEventReceived({snapshot});
    SetActiveTreeId(tree_id_);
    Reset({});
  }

  void SetUpdateTreeID(ui::AXTreeUpdate* update) {
    SetUpdateTreeID(update, tree_id_);
  }

  void SetDistillationInProgress(bool distillation) {
    model_->SetDistillationInProgress(distillation);
  }

  bool AreAllPendingUpdatesEmpty() {
    size_t count = 0;
    for (auto const& [tree_id, updates] :
         model_->GetPendingUpdatesForTesting()) {
      count += updates.size();
    }
    return count == 0;
  }

  void SetUpdateTreeID(ui::AXTreeUpdate* update, ui::AXTreeID tree_id) {
    ui::AXTreeData tree_data;
    tree_data.tree_id = tree_id;
    update->has_tree_data = true;
    update->tree_data = tree_data;
  }

  void SetThemeForTesting(const std::string& font_name,
                          float font_size,
                          SkColor foreground_color,
                          SkColor background_color,
                          int line_spacing,
                          int letter_spacing) {
    auto line_spacing_enum =
        static_cast<read_anything::mojom::LineSpacing>(line_spacing);
    auto letter_spacing_enum =
        static_cast<read_anything::mojom::LetterSpacing>(letter_spacing);
    model_->OnThemeChanged(read_anything::mojom::ReadAnythingTheme::New(
        font_name, font_size, foreground_color, background_color,
        line_spacing_enum, letter_spacing_enum));
  }

  void SetLineAndLetterSpacing(
      read_anything::mojom::LetterSpacing letter_spacing,
      read_anything::mojom::LineSpacing line_spacing) {
    model_->OnThemeChanged(read_anything::mojom::ReadAnythingTheme::New(
        "Arial", 15.0, SkColorSetRGB(0x33, 0x40, 0x36),
        SkColorSetRGB(0xDF, 0xD2, 0x63), line_spacing, letter_spacing));
  }

  void AccessibilityEventReceived(
      const std::vector<ui::AXTreeUpdate>& updates) {
    AccessibilityEventReceived(updates[0].tree_data.tree_id, updates);
  }

  void AccessibilityEventReceived(
      const ui::AXTreeID& tree_id,
      const std::vector<ui::AXTreeUpdate>& updates) {
    model_->AccessibilityEventReceived(tree_id, updates, {});
  }

  void SetActiveTreeId(ui::AXTreeID tree_id) {
    model_->SetActiveTreeId(tree_id);
  }

  void UnserializePendingUpdates(ui::AXTreeID tree_id) {
    model_->UnserializePendingUpdates(tree_id);
  }

  void ClearPendingUpdates() { model_->ClearPendingUpdates(); }

  std::string FontName() { return model_->font_name(); }

  float FontSize() { return model_->font_size(); }

  SkColor ForegroundColor() { return model_->foreground_color(); }

  SkColor BackgroundColor() { return model_->background_color(); }

  float LineSpacing() { return model_->line_spacing(); }

  float LetterSpacing() { return model_->letter_spacing(); }

  bool DistillationInProgress() { return model_->distillation_in_progress(); }

  bool HasSelection() { return model_->has_selection(); }

  ui::AXNodeID StartNodeId() { return model_->start_node_id(); }
  ui::AXNodeID EndNodeId() { return model_->end_node_id(); }

  int32_t StartOffset() { return model_->start_offset(); }
  int32_t EndOffset() { return model_->end_offset(); }

  bool IsNodeIgnoredForReadAnything(ui::AXNodeID ax_node_id) {
    return model_->IsNodeIgnoredForReadAnything(ax_node_id);
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

  bool SelectionNodeIdsContains(ui::AXNodeID ax_node_id) {
    return base::Contains(model_->selection_node_ids(), ax_node_id);
  }

  void ProcessDisplayNodes(const std::vector<ui::AXNodeID>& content_node_ids) {
    Reset(content_node_ids);
    model_->ComputeDisplayNodeIdsForDistilledTree();
  }

  bool ProcessSelection() { return model_->PostProcessSelection(); }

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

  void IncreaseTextSize() { model_->IncreaseTextSize(); }

  void DecreaseTextSize() { model_->DecreaseTextSize(); }

  void ResetTextSize() { model_->ResetTextSize(); }

  std::string DefaultLanguageCode() { return model_->default_language_code(); }
  void SetLanguageCode(std::string code) {
    model_->set_default_language_code(code);
  }

  std::vector<std::string> GetSupportedFonts() {
    return model_->GetSupportedFonts();
  }

  bool IsPDFFormatted() { return model_->IsPDFFormatted(); }
  void SetIsPdf(const GURL& url) { return model_->SetIsPdf(url); }
  bool IsPdf() { return model_->is_pdf(); }
  ui::AXTreeID GetPDFWebContents() { return model_->GetPDFWebContents(); }

  ui::AXTreeID tree_id_;

 private:
  // ReadAnythingAppModel constructor and destructor are private so it's
  // not accessible by std::make_unique.
  raw_ptr<ReadAnythingAppModel, ExperimentalRenderer> model_ = nullptr;
};

TEST_F(ReadAnythingAppModelTest, Theme) {
  std::string font_name = "Roboto";
  float font_size = 18.0;
  SkColor foreground = SkColorSetRGB(0x33, 0x36, 0x39);
  SkColor background = SkColorSetRGB(0xFD, 0xE2, 0x93);
  int letter_spacing =
      static_cast<int>(read_anything::mojom::LetterSpacing::kDefaultValue);
  float letter_spacing_value = 0.0;
  int line_spacing =
      static_cast<int>(read_anything::mojom::LineSpacing::kDefaultValue);
  float line_spacing_value = 1.5;
  SetThemeForTesting(font_name, font_size, foreground, background, line_spacing,
                     letter_spacing);
  EXPECT_EQ(font_name, FontName());
  EXPECT_EQ(font_size, FontSize());
  EXPECT_EQ(foreground, ForegroundColor());
  EXPECT_EQ(background, BackgroundColor());
  EXPECT_EQ(line_spacing_value, LineSpacing());
  EXPECT_EQ(letter_spacing_value, LetterSpacing());
}

TEST_F(ReadAnythingAppModelTest, IsNodeIgnoredForReadAnything) {
  ui::AXTreeUpdate update;
  SetUpdateTreeID(&update);
  update.nodes.resize(3);
  update.nodes[0].id = 2;
  update.nodes[1].id = 3;
  update.nodes[2].id = 4;
  update.nodes[0].role = ax::mojom::Role::kStaticText;
  update.nodes[1].role = ax::mojom::Role::kComboBoxGrouping;
  update.nodes[2].role = ax::mojom::Role::kButton;
  AccessibilityEventReceived({update});
  EXPECT_EQ(false, IsNodeIgnoredForReadAnything(2));
  EXPECT_EQ(true, IsNodeIgnoredForReadAnything(3));
  EXPECT_EQ(true, IsNodeIgnoredForReadAnything(4));
}

TEST_F(ReadAnythingAppModelTest,
       IsNodeIgnoredForReadAnything_TextFieldsNotIgnored) {
  ui::AXTreeUpdate update;
  SetUpdateTreeID(&update);
  update.nodes.resize(3);
  update.nodes[0].id = 2;
  update.nodes[1].id = 3;
  update.nodes[2].id = 4;
  update.nodes[0].role = ax::mojom::Role::kTree;
  update.nodes[1].role = ax::mojom::Role::kTextFieldWithComboBox;
  update.nodes[2].role = ax::mojom::Role::kTextField;
  AccessibilityEventReceived({update});
  EXPECT_EQ(true, IsNodeIgnoredForReadAnything(2));
  EXPECT_EQ(false, IsNodeIgnoredForReadAnything(3));
  EXPECT_EQ(false, IsNodeIgnoredForReadAnything(4));
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
    SetUpdateTreeID(&update, tree_ids[i]);
    update.root_id = 1;
    update.nodes.resize(1);
    update.nodes[0].id = 1;
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
  SetUpdateTreeID(&update_2, tree_id_2);
  update_2.root_id = 1;
  update_2.nodes.resize(1);
  update_2.nodes[0].id = 1;

  // Updates on inactive trees are processed immediately and are not marked as
  // pending.
  AccessibilityEventReceived({update_2});
  EXPECT_EQ(0u, GetNumPendingUpdates(tree_id_));
}

TEST_F(ReadAnythingAppModelTest,
       AddPendingUpdatesAfterUnserializingOnSameTree_DoesNotCrash) {
  // Set the name of each node to be its id.
  ui::AXTreeUpdate initial_update;
  SetUpdateTreeID(&initial_update);
  initial_update.root_id = 1;
  initial_update.nodes.resize(3);
  std::vector<int> child_ids;
  for (int i = 0; i < 3; i++) {
    int id = i + 2;
    child_ids.push_back(id);
    initial_update.nodes[i].id = id;
    initial_update.nodes[i].role = ax::mojom::Role::kStaticText;
    initial_update.nodes[i].SetName(base::NumberToString(id));
    initial_update.nodes[i].SetNameFrom(ax::mojom::NameFrom::kContents);
  }
  AccessibilityEventReceived({initial_update});

  std::vector<ui::AXTreeUpdate> updates;
  for (int i = 0; i < 3; i++) {
    int id = i + 5;
    child_ids.push_back(id);

    ui::AXTreeUpdate update;
    SetUpdateTreeID(&update);
    update.root_id = 1;
    update.nodes.resize(2);
    update.nodes[0].id = 1;
    update.nodes[0].child_ids = child_ids;
    update.nodes[1].id = id;
    update.nodes[1].role = ax::mojom::Role::kStaticText;
    update.nodes[1].SetName(base::NumberToString(id));
    update.nodes[1].SetNameFrom(ax::mojom::NameFrom::kContents);
    updates.push_back(update);
  }

  // Send update 0, which starts distillation.
  AccessibilityEventReceived({updates[0]});
  EXPECT_EQ(0u, GetNumPendingUpdates(tree_id_));
  ASSERT_TRUE(AreAllPendingUpdatesEmpty());

  // Send update 1. Since distillation is in progress, this will not be
  // unserialized yet.
  SetDistillationInProgress(true);
  AccessibilityEventReceived({updates[1]});
  EXPECT_EQ(1u, GetNumPendingUpdates(tree_id_));

  // Ensure that there are no crashes after an accessibility event is received
  // immediately after unserializing.
  UnserializePendingUpdates(tree_id_);
  SetDistillationInProgress(true);
  AccessibilityEventReceived({updates[2]});
  EXPECT_EQ(1u, GetNumPendingUpdates(tree_id_));
  ASSERT_FALSE(AreAllPendingUpdatesEmpty());
}

TEST_F(ReadAnythingAppModelTest, OnTreeErased_ClearsPendingUpdates) {
  // Set the name of each node to be its id.
  ui::AXTreeUpdate initial_update;
  SetUpdateTreeID(&initial_update);
  initial_update.root_id = 1;
  initial_update.nodes.resize(3);
  std::vector<int> child_ids;
  for (int i = 0; i < 3; i++) {
    int id = i + 2;
    child_ids.push_back(id);
    initial_update.nodes[i].id = id;
    initial_update.nodes[i].role = ax::mojom::Role::kStaticText;
    initial_update.nodes[i].SetName(base::NumberToString(id));
    initial_update.nodes[i].SetNameFrom(ax::mojom::NameFrom::kContents);
  }
  AccessibilityEventReceived({initial_update});

  std::vector<ui::AXTreeUpdate> updates;
  for (int i = 0; i < 3; i++) {
    int id = i + 5;
    child_ids.push_back(id);

    ui::AXTreeUpdate update;
    SetUpdateTreeID(&update);
    update.root_id = 1;
    update.nodes.resize(2);
    update.nodes[0].id = 1;
    update.nodes[0].child_ids = child_ids;
    update.nodes[1].id = id;
    update.nodes[1].role = ax::mojom::Role::kStaticText;
    update.nodes[1].SetName(base::NumberToString(id));
    update.nodes[1].SetNameFrom(ax::mojom::NameFrom::kContents);
    updates.push_back(update);
  }

  // Send update 0, which starts distillation.
  AccessibilityEventReceived({updates[0]});
  EXPECT_EQ(0u, GetNumPendingUpdates(tree_id_));
  ASSERT_TRUE(AreAllPendingUpdatesEmpty());

  // Send update 1. Since distillation is in progress, this will not be
  // unserialized yet.
  SetDistillationInProgress(true);
  AccessibilityEventReceived({updates[1]});
  EXPECT_EQ(1u, GetNumPendingUpdates(tree_id_));

  // Destroy the tree.
  EraseTree(tree_id_);
  EXPECT_EQ(0u, GetNumPendingUpdates(tree_id_));
}

TEST_F(ReadAnythingAppModelTest,
       DistillationInProgress_TreeUpdateReceivedOnActiveTree) {
  // Set the name of each node to be its id.
  ui::AXTreeUpdate initial_update;
  SetUpdateTreeID(&initial_update);
  initial_update.root_id = 1;
  initial_update.nodes.resize(3);
  std::vector<int> child_ids;
  for (int i = 0; i < 3; i++) {
    int id = i + 2;
    child_ids.push_back(id);
    initial_update.nodes[i].id = id;
    initial_update.nodes[i].role = ax::mojom::Role::kStaticText;
    initial_update.nodes[i].SetName(base::NumberToString(id));
    initial_update.nodes[i].SetNameFrom(ax::mojom::NameFrom::kContents);
  }
  AccessibilityEventReceived({initial_update});

  std::vector<ui::AXTreeUpdate> updates;
  for (int i = 0; i < 3; i++) {
    int id = i + 5;
    child_ids.push_back(id);

    ui::AXTreeUpdate update;
    SetUpdateTreeID(&update);
    update.root_id = 1;
    update.nodes.resize(2);
    update.nodes[0].id = 1;
    update.nodes[0].child_ids = child_ids;
    update.nodes[1].id = id;
    update.nodes[1].role = ax::mojom::Role::kStaticText;
    update.nodes[1].SetName(base::NumberToString(id));
    update.nodes[1].SetNameFrom(ax::mojom::NameFrom::kContents);
    updates.push_back(update);
  }

  // Send update 0, which starts distillation.
  AccessibilityEventReceived({updates[0]});
  EXPECT_EQ(0u, GetNumPendingUpdates(tree_id_));
  ASSERT_TRUE(AreAllPendingUpdatesEmpty());

  // Send update 1. Since distillation is in progress, this will not be
  // unserialized yet.
  SetDistillationInProgress(true);
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

TEST_F(ReadAnythingAppModelTest, ClearPendingUpdates_DeletesPendingUpdates) {
  EXPECT_EQ(0u, GetNumPendingUpdates(tree_id_));

  // Create a couple of updates which add additional nodes to the tree.
  std::vector<ui::AXTreeUpdate> updates;
  std::vector<int> child_ids = {2, 3, 4};
  for (int i = 0; i < 3; i++) {
    int id = i + 5;
    child_ids.push_back(id);

    ui::AXTreeUpdate update;
    SetUpdateTreeID(&update);
    update.root_id = 1;
    update.nodes.resize(2);
    update.nodes[0].id = 1;
    update.nodes[0].child_ids = child_ids;
    update.nodes[1].id = id;
    update.nodes[1].role = ax::mojom::Role::kStaticText;
    update.nodes[1].SetName(base::NumberToString(id));
    update.nodes[1].SetNameFrom(ax::mojom::NameFrom::kContents);
    updates.push_back(update);
  }

  AccessibilityEventReceived({updates[0]});
  EXPECT_EQ(0u, GetNumPendingUpdates(tree_id_));
  SetDistillationInProgress(true);
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
  std::vector<ui::AXTreeUpdate> updates;
  std::vector<int> child_ids = {2, 3, 4};
  for (int i = 0; i < 2; i++) {
    int id = i + 5;
    child_ids.push_back(id);

    ui::AXTreeUpdate update;
    SetUpdateTreeID(&update);
    update.root_id = 1;
    update.nodes.resize(2);
    update.nodes[0].id = 1;
    update.nodes[0].child_ids = child_ids;
    update.nodes[1].id = id;
    update.nodes[1].role = ax::mojom::Role::kStaticText;
    update.nodes[1].SetName(base::NumberToString(id));
    update.nodes[1].SetNameFrom(ax::mojom::NameFrom::kContents);
    updates.push_back(update);
  }

  // Create an update which has no tree id.
  ui::AXTreeUpdate update;
  update.nodes.resize(1);
  update.nodes[0].id = 1;
  update.nodes[0].role = ax::mojom::Role::kGenericContainer;
  updates.push_back(update);

  // Add the three updates.
  AccessibilityEventReceived({updates[0]});
  EXPECT_EQ(0u, GetNumPendingUpdates(tree_id_));
  ASSERT_TRUE(AreAllPendingUpdatesEmpty());
  SetDistillationInProgress(true);
  AccessibilityEventReceived(tree_id_, {updates[1], updates[2]});
  EXPECT_EQ(2u, GetNumPendingUpdates(tree_id_));

  // Switch to a new active tree. Should not crash.
  SetActiveTreeId(ui::AXTreeIDUnknown());
}

TEST_F(ReadAnythingAppModelTest, DisplayNodeIdsContains_ContentNodes) {
  ui::AXTreeUpdate update;
  SetUpdateTreeID(&update);
  update.nodes.resize(3);
  update.nodes[0].id = 4;
  update.nodes[0].child_ids = {5, 6};
  update.nodes[1].id = 5;
  update.nodes[2].id = 6;
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

TEST_F(ReadAnythingAppModelTest, SetTheme_LineAndLetterSpacingCorrect) {
  SetLineAndLetterSpacing(read_anything::mojom::LetterSpacing::kStandard,
                          read_anything::mojom::LineSpacing::kLoose);
  ASSERT_EQ(LineSpacing(), 1.5);
  ASSERT_EQ(LetterSpacing(), 0);

  // Ensure the line and letter spacing are updated.
  SetLineAndLetterSpacing(read_anything::mojom::LetterSpacing::kWide,
                          read_anything::mojom::LineSpacing::kVeryLoose);
  ASSERT_EQ(LineSpacing(), 2.0);
  ASSERT_EQ(LetterSpacing(), 0.05f);
}

TEST_F(ReadAnythingAppModelTest, Reset_ResetsState) {
  // Initial state.
  ui::AXTreeUpdate update;
  SetUpdateTreeID(&update);
  update.nodes.resize(3);
  update.nodes[0].id = 4;
  update.nodes[0].child_ids = {5, 6};
  update.nodes[1].id = 5;
  update.nodes[2].id = 6;
  AccessibilityEventReceived({update});
  ProcessDisplayNodes({3, 4});
  SetDistillationInProgress(true);

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
       StartAndEndNodesHaveDifferentParents_SelectionStateCorrect) {
  ui::AXTreeUpdate update;
  SetUpdateTreeID(&update);
  update.nodes.resize(6);
  update.nodes[0].id = 1;
  update.nodes[1].id = 2;
  update.nodes[2].id = 3;
  update.nodes[3].id = 4;
  update.nodes[4].id = 5;
  update.nodes[5].id = 6;
  update.nodes[0].child_ids = {2, 3, 4};
  update.nodes[3].child_ids = {5, 6};
  update.nodes[0].role = ax::mojom::Role::kStaticText;
  update.nodes[1].role = ax::mojom::Role::kStaticText;
  update.nodes[2].role = ax::mojom::Role::kStaticText;
  update.nodes[3].role = ax::mojom::Role::kGenericContainer;
  update.nodes[4].role = ax::mojom::Role::kStaticText;
  update.nodes[5].role = ax::mojom::Role::kStaticText;
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

  // Even though 3 is a generic container with more than one child, its sibling
  // nodes are included in the selection because the start node includes it.
  ASSERT_TRUE(SelectionNodeIdsContains(2));
  ASSERT_TRUE(SelectionNodeIdsContains(3));
}

TEST_F(ReadAnythingAppModelTest,
       SelectionParentIsLinkAndInlineBlock_SelectionStateCorrect) {
  ui::AXTreeUpdate update;
  SetUpdateTreeID(&update);
  update.nodes.resize(4);
  update.nodes[0].id = 1;
  update.nodes[1].id = 2;
  update.nodes[2].id = 3;
  update.nodes[3].id = 4;
  update.nodes[0].child_ids = {2, 3};
  update.nodes[2].child_ids = {4};
  update.nodes[0].role = ax::mojom::Role::kStaticText;
  update.nodes[1].role = ax::mojom::Role::kStaticText;
  update.nodes[2].role = ax::mojom::Role::kLink;
  update.nodes[2].AddStringAttribute(ax::mojom::StringAttribute::kDisplay,
                                     "block");
  update.nodes[3].role = ax::mojom::Role::kStaticText;
  update.nodes[3].AddStringAttribute(ax::mojom::StringAttribute::kDisplay,
                                     "inline-block");
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
  update.nodes.resize(4);
  update.nodes[0].id = 1;
  update.nodes[1].id = 2;
  update.nodes[2].id = 3;
  update.nodes[3].id = 4;
  update.nodes[0].child_ids = {2, 3};
  update.nodes[2].child_ids = {4};
  update.nodes[0].role = ax::mojom::Role::kStaticText;
  update.nodes[1].role = ax::mojom::Role::kStaticText;
  update.nodes[2].role = ax::mojom::Role::kLink;
  update.nodes[2].AddStringAttribute(ax::mojom::StringAttribute::kDisplay,
                                     "block");
  update.nodes[3].role = ax::mojom::Role::kStaticText;
  update.nodes[3].AddStringAttribute(ax::mojom::StringAttribute::kDisplay,
                                     "list-item");
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
  update.nodes.resize(4);
  update.nodes[0].id = 1;
  update.nodes[1].id = 2;
  update.nodes[2].id = 3;
  update.nodes[3].id = 4;
  update.nodes[0].child_ids = {2, 3};
  update.nodes[2].child_ids = {4};
  update.nodes[0].role = ax::mojom::Role::kStaticText;
  update.nodes[1].role = ax::mojom::Role::kStaticText;
  update.nodes[2].role = ax::mojom::Role::kGenericContainer;
  update.nodes[2].AddStringAttribute(ax::mojom::StringAttribute::kDisplay,
                                     "block");
  update.nodes[3].role = ax::mojom::Role::kStaticText;
  update.nodes[3].AddStringAttribute(ax::mojom::StringAttribute::kDisplay,
                                     "inline");
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
  update.nodes.resize(5);
  update.nodes[0].id = 1;
  update.nodes[1].id = 2;
  update.nodes[2].id = 3;
  update.nodes[3].id = 4;
  update.nodes[4].id = 5;
  update.nodes[0].child_ids = {2, 3};
  update.nodes[2].child_ids = {4, 5};
  update.nodes[0].role = ax::mojom::Role::kStaticText;
  update.nodes[1].role = ax::mojom::Role::kStaticText;
  update.nodes[2].role = ax::mojom::Role::kGenericContainer;
  update.nodes[3].role = ax::mojom::Role::kStaticText;
  update.nodes[4].role = ax::mojom::Role::kStaticText;
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

TEST_F(ReadAnythingAppModelTest,
       SupportedFonts_SetDefaultLanguageCode_ReturnsCorrectCode) {
  ASSERT_EQ(DefaultLanguageCode(), "en-US");

  SetLanguageCode("es");
  ASSERT_EQ(DefaultLanguageCode(), "es");
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
  std::vector<std::string> expectedFonts = {"Sans-serif", "Serif"};
  std::vector<std::string> fonts = GetSupportedFonts();

  EXPECT_EQ(fonts.size(), expectedFonts.size());
  for (size_t i = 0; i < fonts.size(); i++) {
    ASSERT_EQ(fonts[i], expectedFonts[i]);
  }
}

TEST_F(ReadAnythingAppModelTest,
       SupportedFonts_SetDefaultLanguageCode_ReturnsExpectedDefaultFonts) {
  // English
  SetLanguageCode("en");
  std::vector<std::string> expectedFonts = {
      "Poppins",     "Sans-serif",  "Serif",        "Comic Neue",
      "Lexend Deca", "EB Garamond", "STIX Two Text"};
  std::vector<std::string> fonts = GetSupportedFonts();

  EXPECT_EQ(fonts.size(), expectedFonts.size());
  for (size_t i = 0; i < fonts.size(); i++) {
    ASSERT_EQ(fonts[i], expectedFonts[i]);
  }

  // Bulgarian
  SetLanguageCode("bg");
  expectedFonts = {"Sans-serif", "Serif", "EB Garamond", "STIX Two Text"};
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

TEST_F(ReadAnythingAppModelTest, IsPdf) {
  GURL webpage_url("http://images.google.com/foo.html");
  SetIsPdf(webpage_url);
  ASSERT_FALSE(IsPdf());

  GURL pdf_url("http://www.google.com/foo/bar.pdf");
  SetIsPdf(pdf_url);
  ASSERT_TRUE(IsPdf());
}

TEST_F(ReadAnythingAppModelTest, ValidPDF) {
  // Need to set is_pdf_ for DCHECK in GetPDFWebContents().
  GURL pdf_url("http://www.google.com/foo/bar.pdf");
  SetIsPdf(pdf_url);

  ui::AXTreeID pdf_web_contents_tree_id = ui::AXTreeID::CreateNewAXTreeID();
  ui::AXTreeID pdf_iframe_tree_id = ui::AXTreeID::CreateNewAXTreeID();

  // Main web contents should have one child.
  ui::AXTreeUpdate update;
  update.nodes.resize(1);
  update.nodes[0].id = 1;
  update.nodes[0].AddChildTreeId(pdf_web_contents_tree_id);
  SetUpdateTreeID(&update);
  AccessibilityEventReceived({update});

  // IsPDFFormatted() should return true if tree updates from the pdf web
  // contents and/or the pdf iframe haven't been sent yet.
  ASSERT_TRUE(IsPDFFormatted());

  // Pdf web contents should have one child.
  update.nodes.resize(1);
  update.root_id = 1;
  update.nodes[0].id = 1;
  update.nodes[0].AddChildTreeId(pdf_iframe_tree_id);
  SetUpdateTreeID(&update, pdf_web_contents_tree_id);
  AccessibilityEventReceived({update});

  ASSERT_TRUE(IsPDFFormatted());

  // Send pdf iframe tree to model.
  update.nodes.resize(1);
  update.root_id = 1;
  update.nodes[0].id = 1;
  SetUpdateTreeID(&update, pdf_iframe_tree_id);
  AccessibilityEventReceived({update});

  ASSERT_TRUE(IsPDFFormatted());
  EXPECT_EQ(pdf_web_contents_tree_id, GetPDFWebContents());
}

TEST_F(ReadAnythingAppModelTest, InvalidPDFFormat) {
  // Main web contents should have one child, the pdf web contents.
  ui::AXTreeID pdf_web_contents_tree_id = ui::AXTreeID::CreateNewAXTreeID();
  ui::AXTreeUpdate update;
  update.nodes.resize(1);
  update.nodes[0].id = 1;
  update.nodes[0].AddChildTreeId(pdf_web_contents_tree_id);
  SetUpdateTreeID(&update);
  AccessibilityEventReceived({update});

  // This pdf web contents has no children, so this is an invalid PDF.
  ui::AXTreeUpdate pdf_web_contents_update;
  pdf_web_contents_update.nodes.resize(1);
  pdf_web_contents_update.root_id = 1;
  pdf_web_contents_update.nodes[0].id = 1;
  SetUpdateTreeID(&pdf_web_contents_update, pdf_web_contents_tree_id);
  AccessibilityEventReceived({pdf_web_contents_update});

  ASSERT_FALSE(IsPDFFormatted());
}
