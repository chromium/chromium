// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/accessibility/read_anything_app_model.h"

#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/threading/platform_thread.h"
#include "chrome/test/base/chrome_render_view_test.h"
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
    model_ = new ReadAnythingAppModel();

    // Create a tree id.
    tree_id_ = ui::AXTreeID::CreateNewAXTreeID();

    // Create simple AXTreeUpdate with a root node and 3 children.
    ui::AXTreeUpdate snapshot;
    ui::AXNodeData node1;
    node1.id = 2;

    ui::AXNodeData node2;
    node2.id = 3;

    ui::AXNodeData node3;
    node3.id = 4;

    ui::AXNodeData root;
    root.id = 1;
    root.child_ids = {node1.id, node2.id, node3.id};
    snapshot.root_id = root.id;
    snapshot.nodes = {root, node1, node2, node3};
    SetUpdateTreeID(&snapshot);

    AccessibilityEventReceived({snapshot});
    set_active_tree_id(tree_id_);
    Reset({});
  }

  void SetUpdateTreeID(ui::AXTreeUpdate* update) {
    SetUpdateTreeID(update, tree_id_);
  }

  void SetDistillationInProgress(bool distillation) {
    model_->SetDistillationInProgress(distillation);
  }

  void SetSpeechPlaying(bool speech_playing) {
    model_->set_speech_playing(speech_playing);
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

  void SetUpdateTreeID(ui::AXTreeUpdate* update, ui::AXTreeID tree_id) {
    ui::AXTreeData tree_data;
    tree_data.tree_id = tree_id;
    update->has_tree_data = true;
    update->tree_data = tree_data;
  }

  void SetThemeForTesting(const std::string& font_name,
                          float font_size,
                          bool links_enabled,
                          bool images_enabled,
                          SkColor foreground_color,
                          SkColor background_color,
                          int line_spacing,
                          int letter_spacing) {
    auto line_spacing_enum =
        static_cast<read_anything::mojom::LineSpacing>(line_spacing);
    auto letter_spacing_enum =
        static_cast<read_anything::mojom::LetterSpacing>(letter_spacing);
    model_->OnThemeChanged(read_anything::mojom::ReadAnythingTheme::New(
        font_name, font_size, links_enabled, images_enabled, foreground_color,
        background_color, line_spacing_enum, letter_spacing_enum));
  }

  void SetLineAndLetterSpacing(
      read_anything::mojom::LetterSpacing letter_spacing,
      read_anything::mojom::LineSpacing line_spacing) {
    model_->OnThemeChanged(read_anything::mojom::ReadAnythingTheme::New(
        "Arial", 15.0, false, false, SkColorSetRGB(0x33, 0x40, 0x36),
        SkColorSetRGB(0xDF, 0xD2, 0x63), line_spacing, letter_spacing));
  }

  void AccessibilityEventReceived(
      const std::vector<ui::AXTreeUpdate>& updates) {
    AccessibilityEventReceived(updates[0].tree_data.tree_id, updates);
  }

  void AccessibilityEventReceived(
      const ui::AXTreeID& tree_id,
      const std::vector<ui::AXTreeUpdate>& updates) {
    std::vector<ui::AXEvent> events;
    model_->AccessibilityEventReceived(
        tree_id, const_cast<std::vector<ui::AXTreeUpdate>&>(updates), events);
  }

  void AccessibilityEventReceived(const ui::AXTreeID& tree_id,
                                  const std::vector<ui::AXTreeUpdate>& updates,
                                  const std::vector<ui::AXEvent>& events) {
    model_->AccessibilityEventReceived(
        tree_id, const_cast<std::vector<ui::AXTreeUpdate>&>(updates),
        const_cast<std::vector<ui::AXEvent>&>(events));
  }

  void set_active_tree_id(ui::AXTreeID tree_id) {
    model_->set_active_tree_id(tree_id);
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

  void IncreaseTextSize() { model_->IncreaseTextSize(); }

  void DecreaseTextSize() { model_->DecreaseTextSize(); }

  void ResetTextSize() { model_->ResetTextSize(); }

  std::string LanguageCode() { return model_->base_language_code(); }
  void SetLanguageCode(std::string code) {
    model_->set_base_language_code(code);
  }

  std::string DefaultLanguageCode() { return model_->default_language_code(); }
  void SetDefaultLanguageCode(std::string code) {
    model_->set_default_language_code(code);
  }

  std::vector<std::string> GetSupportedFonts() {
    return model_->GetSupportedFonts();
  }

  void set_is_pdf(bool is_pdf) { return model_->set_is_pdf(is_pdf); }

  void InitAXPosition(const ui::AXNodeID id) {
    model_->InitAXPositionWithNode(id);
  }

  void ResetReadAloudState() { model_->ResetReadAloudState(); }

  ui::AXNodePosition::AXPositionInstance GetNextNodePosition() {
    ReadAnythingAppModel::ReadAloudCurrentGranularity granularity =
        ReadAnythingAppModel::ReadAloudCurrentGranularity();
    return model_->GetNextValidPositionFromCurrentPosition(granularity);
  }

  ui::AXNodePosition::AXPositionInstance GetNextNodePosition(
      ReadAnythingAppModel::ReadAloudCurrentGranularity granularity) {
    return model_->GetNextValidPositionFromCurrentPosition(granularity);
  }

  ReadAnythingAppModel::ReadAloudCurrentGranularity GetNextNodes() {
    return model_->GetNextNodes();
  }

  size_t GetNextSentence(const std::u16string& text) {
    return model_->GetNextSentence(text);
  }

  size_t GetNextWord(const std::u16string& text) {
    return model_->GetNextWord(text);
  }

  ui::AXNodeID GetNodeIdForCurrentSegmentIndex(int index) {
    ui::AXNodeID id = model_->GetNodeIdForCurrentSegmentIndex(index);
    return id;
  }

  int GetWordLength(int index) {
    return model_->GetNextWordHighlightLength(index);
  }

  std::vector<ui::AXNodeID> GetCurrentText() {
    return model_->GetCurrentText();
  }

  void MovePositionToNextGranularity() {
    return model_->MovePositionToNextGranularity();
  }

  void MovePositionToPreviousGranularity() {
    return model_->MovePositionToPreviousGranularity();
  }

  int GetCurrentTextStartIndex(ui::AXNodeID id) {
    return model_->GetCurrentTextStartIndex(id);
  }

  int GetCurrentTextEndIndex(ui::AXNodeID id) {
    return model_->GetCurrentTextEndIndex(id);
  }

  ui::AXTreeID tree_id_;

 private:
  // ReadAnythingAppModel constructor and destructor are private so it's
  // not accessible by std::make_unique.
  raw_ptr<ReadAnythingAppModel> model_ = nullptr;
};

TEST_F(ReadAnythingAppModelTest, FontName) {
  EXPECT_EQ(string_constants::kReadAnythingPlaceholderFontName, FontName());

  std::string font_name = "Montserrat";
  SetFontName(font_name);
  EXPECT_EQ(font_name, FontName());
}

TEST_F(ReadAnythingAppModelTest, Theme) {
  std::string font_name = "Roboto";
  float font_size = 18.0;
  bool links_enabled = false;
  bool images_enabled = true;
  SkColor foreground = SkColorSetRGB(0x33, 0x36, 0x39);
  SkColor background = SkColorSetRGB(0xFD, 0xE2, 0x93);
  int letter_spacing =
      static_cast<int>(read_anything::mojom::LetterSpacing::kDefaultValue);
  float letter_spacing_value = 0.0;
  int line_spacing =
      static_cast<int>(read_anything::mojom::LineSpacing::kDefaultValue);
  float line_spacing_value = 1.5;
  SetThemeForTesting(font_name, font_size, links_enabled, images_enabled,
                     foreground, background, line_spacing, letter_spacing);
  EXPECT_EQ(font_name, FontName());
  EXPECT_EQ(font_size, FontSize());
  EXPECT_EQ(links_enabled, LinksEnabled());
  EXPECT_EQ(images_enabled, ImagesEnabled());
  EXPECT_EQ(foreground, ForegroundColor());
  EXPECT_EQ(background, BackgroundColor());
  EXPECT_EQ(line_spacing_value, LineSpacing());
  EXPECT_EQ(letter_spacing_value, LetterSpacing());
}

TEST_F(ReadAnythingAppModelTest, IsNodeIgnoredForReadAnything) {
  ui::AXTreeUpdate update;
  SetUpdateTreeID(&update);
  ui::AXNodeData static_text_node;
  static_text_node.id = 2;
  static_text_node.role = ax::mojom::Role::kStaticText;

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
  SetUpdateTreeID(&update, tree_id_);
  ui::AXNodeData banner_node;
  banner_node.id = 2;
  banner_node.role = ax::mojom::Role::kBanner;

  ui::AXNodeData static_text_start_node;
  static_text_start_node.id = 3;
  static_text_start_node.role = ax::mojom::Role::kStaticText;
  static_text_start_node.SetNameChecked(
      l10n_util::GetStringUTF8(IDS_PDF_OCR_RESULT_BEGIN));
  banner_node.child_ids = {static_text_start_node.id};

  ui::AXNodeData content_info_node;
  content_info_node.id = 4;
  content_info_node.role = ax::mojom::Role::kContentInfo;

  ui::AXNodeData static_text_end_node;
  static_text_end_node.id = 5;
  static_text_end_node.role = ax::mojom::Role::kStaticText;
  static_text_end_node.SetNameChecked(
      l10n_util::GetStringUTF8(IDS_PDF_OCR_RESULT_END));
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
    SetUpdateTreeID(&update, tree_ids[i]);
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
  SetUpdateTreeID(&update_2, tree_id_2);
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
    initial_update.nodes[i].SetNameChecked(base::NumberToString(id));
  }
  AccessibilityEventReceived({initial_update});

  std::vector<ui::AXTreeUpdate> updates;
  for (int i = 0; i < 3; i++) {
    int id = i + 5;
    child_ids.push_back(id);

    ui::AXTreeUpdate update;
    SetUpdateTreeID(&update);
    update.root_id = 1;
    ui::AXNodeData root;
    root.id = 1;
    root.child_ids = child_ids;

    ui::AXNodeData node;
    node.id = id;
    node.role = ax::mojom::Role::kStaticText;
    node.SetNameChecked(base::NumberToString(id));
    update.nodes = {root, node};
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
    initial_update.nodes[i].SetNameChecked(base::NumberToString(id));
  }
  AccessibilityEventReceived({initial_update});

  std::vector<ui::AXTreeUpdate> updates;
  for (int i = 0; i < 3; i++) {
    int id = i + 5;
    child_ids.push_back(id);

    ui::AXTreeUpdate update;
    SetUpdateTreeID(&update);
    ui::AXNodeData root;
    root.id = 1;
    root.child_ids = child_ids;

    ui::AXNodeData node;
    node.id = id;
    node.role = ax::mojom::Role::kStaticText;
    node.SetNameChecked(base::NumberToString(id));
    update.root_id = root.id;
    update.nodes = {root, node};
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
    initial_update.nodes[i].SetNameChecked(base::NumberToString(id));
  }
  AccessibilityEventReceived({initial_update});

  std::vector<ui::AXTreeUpdate> updates;
  for (int i = 0; i < 3; i++) {
    int id = i + 5;
    child_ids.push_back(id);

    ui::AXTreeUpdate update;
    SetUpdateTreeID(&update);
    ui::AXNodeData root;
    root.id = 1;
    root.child_ids = child_ids;

    ui::AXNodeData node;
    node.id = id;
    node.role = ax::mojom::Role::kStaticText;
    node.SetNameChecked(base::NumberToString(id));
    update.root_id = root.id;
    update.nodes = {root, node};
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

TEST_F(ReadAnythingAppModelTest, SpeechPlaying_TreeUpdateReceivedOnActiveTree) {
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
    initial_update.nodes[i].SetNameChecked(base::NumberToString(id));
  }
  AccessibilityEventReceived({initial_update});

  std::vector<ui::AXTreeUpdate> updates;
  for (int i = 0; i < 3; i++) {
    int id = i + 5;
    child_ids.push_back(id);

    ui::AXTreeUpdate update;
    SetUpdateTreeID(&update);
    ui::AXNodeData root;
    root.id = 1;
    root.child_ids = child_ids;

    ui::AXNodeData node;
    node.id = id;
    node.role = ax::mojom::Role::kStaticText;
    node.SetNameChecked(base::NumberToString(id));
    update.root_id = root.id;
    update.nodes = {root, node};
    updates.push_back(update);
  }

  // Send update 0, which starts distillation.
  AccessibilityEventReceived({updates[0]});
  EXPECT_EQ(0u, GetNumPendingUpdates(tree_id_));
  ASSERT_TRUE(AreAllPendingUpdatesEmpty());

  // Send update 1. Since distillation is in progress, this will not be
  // unserialized yet.
  SetSpeechPlaying(true);
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
    ui::AXNodeData root;
    root.id = 1;
    root.child_ids = child_ids;

    ui::AXNodeData node;
    node.id = id;
    node.role = ax::mojom::Role::kStaticText;
    node.SetNameChecked(base::NumberToString(id));
    update.root_id = root.id;
    update.nodes = {root, node};
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
  std::vector<ui::AXEvent> events;
  std::vector<int> child_ids = {2, 3, 4};
  for (int i = 0; i < 2; i++) {
    int id = i + 5;
    child_ids.push_back(id);

    ui::AXTreeUpdate update;
    SetUpdateTreeID(&update);
    ui::AXNodeData root;
    root.id = 1;
    root.child_ids = child_ids;

    ui::AXNodeData node;
    node.id = id;
    node.role = ax::mojom::Role::kStaticText;
    node.SetNameChecked(base::NumberToString(id));
    update.root_id = root.id;
    update.nodes = {root, node};
    updates.push_back(update);
  }

  // Create an update which has no tree id.
  ui::AXTreeUpdate update;
  ui::AXNodeData node;
  node.id = 1;
  node.role = ax::mojom::Role::kGenericContainer;
  update.nodes = {node};
  updates.push_back(update);

  // Add the three updates.
  AccessibilityEventReceived({updates[0]});
  EXPECT_EQ(0u, GetNumPendingUpdates(tree_id_));
  ASSERT_TRUE(AreAllPendingUpdatesEmpty());
  SetDistillationInProgress(true);
  AccessibilityEventReceived(tree_id_, {updates[1], updates[2]});
  EXPECT_EQ(2u, GetNumPendingUpdates(tree_id_));

  // Switch to a new active tree. Should not crash.
  set_active_tree_id(ui::AXTreeIDUnknown());
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
  update.nodes[2].id = 3;
  update.nodes[2].role = ax::mojom::Role::kStaticText;
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
  update.nodes[2].id = 3;
  update.nodes[2].role = ax::mojom::Role::kStaticText;
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
       PostProcessSelectionFromAction_DoesNotDrawWithNoSelection) {
  // Initial state.
  ui::AXTreeUpdate update;
  SetUpdateTreeID(&update);
  update.tree_data.sel_anchor_object_id = ui::kInvalidAXNodeID;
  update.tree_data.sel_focus_object_id = ui::kInvalidAXNodeID;
  update.tree_data.sel_anchor_offset = 0;
  update.tree_data.sel_focus_offset = 0;
  update.tree_data.sel_is_backward = false;
  AccessibilityEventReceived({update});
  ProcessDisplayNodes({2, 3});
  SetSelectionFromAction(false);

  ASSERT_FALSE(ProcessSelection());
}

TEST_F(ReadAnythingAppModelTest,
       StartAndEndNodesHaveDifferentParents_SelectionStateCorrect) {
  ui::AXTreeUpdate update;
  SetUpdateTreeID(&update);

  ui::AXNodeData static_text_node1;
  static_text_node1.id = 2;
  static_text_node1.role = ax::mojom::Role::kStaticText;

  ui::AXNodeData static_text_node2;
  static_text_node2.id = 3;
  static_text_node2.role = ax::mojom::Role::kStaticText;

  ui::AXNodeData generic_container_node;
  generic_container_node.id = 4;
  generic_container_node.role = ax::mojom::Role::kGenericContainer;

  ui::AXNodeData static_text_child_node1;
  static_text_child_node1.id = 5;
  static_text_child_node1.role = ax::mojom::Role::kStaticText;

  ui::AXNodeData static_text_child_node2;
  static_text_child_node2.id = 6;
  static_text_child_node2.role = ax::mojom::Role::kStaticText;

  ui::AXNodeData parent_node;
  parent_node.id = 1;
  parent_node.child_ids = {static_text_node1.id, static_text_node2.id,
                           generic_container_node.id};
  parent_node.role = ax::mojom::Role::kStaticText;
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

  // Even though 3 is a generic container with more than one child, its sibling
  // nodes are included in the selection because the start node includes it.
  ASSERT_TRUE(SelectionNodeIdsContains(2));
  ASSERT_TRUE(SelectionNodeIdsContains(3));
}

TEST_F(ReadAnythingAppModelTest,
       SelectionParentIsLinkAndInlineBlock_SelectionStateCorrect) {
  ui::AXTreeUpdate update;
  SetUpdateTreeID(&update);

  ui::AXNodeData static_text_node;
  static_text_node.id = 2;
  static_text_node.role = ax::mojom::Role::kStaticText;

  ui::AXNodeData link_node;
  link_node.id = 3;
  link_node.role = ax::mojom::Role::kLink;
  link_node.AddStringAttribute(ax::mojom::StringAttribute::kDisplay, "block");

  ui::AXNodeData inline_block_node;
  inline_block_node.id = 4;
  inline_block_node.role = ax::mojom::Role::kStaticText;
  inline_block_node.AddStringAttribute(ax::mojom::StringAttribute::kDisplay,
                                       "inline-block");
  link_node.child_ids = {inline_block_node.id};

  ui::AXNodeData root;
  root.id = 1;
  root.child_ids = {static_text_node.id, link_node.id};
  root.role = ax::mojom::Role::kStaticText;
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

  ui::AXNodeData static_text_node;
  static_text_node.id = 2;
  static_text_node.role = ax::mojom::Role::kStaticText;

  ui::AXNodeData link_node;
  link_node.id = 3;
  link_node.role = ax::mojom::Role::kLink;
  link_node.AddStringAttribute(ax::mojom::StringAttribute::kDisplay, "block");

  ui::AXNodeData static_text_list_node;
  static_text_list_node.id = 4;
  static_text_list_node.role = ax::mojom::Role::kStaticText;
  static_text_list_node.AddStringAttribute(ax::mojom::StringAttribute::kDisplay,
                                           "list-item");
  link_node.child_ids = {static_text_list_node.id};

  ui::AXNodeData parent_node;
  parent_node.id = 1;
  parent_node.child_ids = {static_text_node.id, link_node.id};
  parent_node.role = ax::mojom::Role::kStaticText;
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
  ui::AXNodeData static_text_node;
  static_text_node.id = 2;
  static_text_node.role = ax::mojom::Role::kStaticText;

  ui::AXNodeData generic_container_node;
  generic_container_node.id = 3;
  generic_container_node.role = ax::mojom::Role::kGenericContainer;
  generic_container_node.AddStringAttribute(
      ax::mojom::StringAttribute::kDisplay, "block");
  ui::AXNodeData inline_node;
  inline_node.id = 4;
  inline_node.role = ax::mojom::Role::kStaticText;
  inline_node.AddStringAttribute(ax::mojom::StringAttribute::kDisplay,
                                 "inline");
  generic_container_node.child_ids = {inline_node.id};

  ui::AXNodeData parent_node;
  parent_node.id = 1;
  parent_node.child_ids = {static_text_node.id, generic_container_node.id};
  parent_node.role = ax::mojom::Role::kStaticText;
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
  ui::AXNodeData static_text_node;
  static_text_node.id = 2;
  static_text_node.role = ax::mojom::Role::kStaticText;

  ui::AXNodeData generic_container_node;
  generic_container_node.role = ax::mojom::Role::kGenericContainer;
  generic_container_node.id = 3;

  ui::AXNodeData static_text_child_node1;
  static_text_child_node1.id = 4;
  static_text_child_node1.role = ax::mojom::Role::kStaticText;

  ui::AXNodeData static_text_child_node2;
  static_text_child_node2.id = 5;
  static_text_child_node2.role = ax::mojom::Role::kStaticText;
  generic_container_node.child_ids = {static_text_child_node1.id,
                                      static_text_child_node2.id};

  ui::AXNodeData parent_node;
  parent_node.id = 1;
  parent_node.role = ax::mojom::Role::kStaticText;
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

TEST_F(ReadAnythingAppModelTest, DefaultLanguageCode_ReturnsCorrectCode) {
  ASSERT_EQ(DefaultLanguageCode(), "en");

  SetDefaultLanguageCode("es");
  ASSERT_EQ(DefaultLanguageCode(), "es");

  // The regular base language code isn't impacted.
  ASSERT_EQ(LanguageCode(), "en");

  // Setting the base language code doesn't impact the default language code.
  SetLanguageCode("jp");
  ASSERT_EQ(LanguageCode(), "jp");
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
  std::vector<std::string> expectedFonts = {
      "Poppins",     "Sans-serif",  "Serif",         "Comic Neue",
      "Lexend Deca", "EB Garamond", "STIX Two Text", "Andika"};
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
      "Poppins",     "Sans-serif",  "Serif",         "Comic Neue",
      "Lexend Deca", "EB Garamond", "STIX Two Text", "Andika"};
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
  ui::AXNodeData static_text_node1;
  static_text_node1.id = 1;
  static_text_node1.role = ax::mojom::Role::kStaticText;

  ui::AXNodeData updated_embedded_node;
  updated_embedded_node.id = 2;
  updated_embedded_node.role = ax::mojom::Role::kEmbeddedObject;
  static_text_node1.child_ids = {updated_embedded_node.id};

  ui::AXNodeData static_text_node2;
  static_text_node2.id = 3;
  static_text_node2.role = ax::mojom::Role::kStaticText;
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
  ui::AXNodeData static_text_node;
  static_text_node.id = 1;
  static_text_node.role = ax::mojom::Role::kStaticText;
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

TEST_F(ReadAnythingAppModelTest, GetNextSentence_ReturnsCorrectIndex) {
  const std::u16string first_sentence = u"This is a normal sentence. ";
  const std::u16string second_sentence = u"This is a second sentence.";

  const std::u16string sentence = first_sentence + second_sentence;
  size_t index = GetNextSentence(sentence);
  EXPECT_EQ(index, first_sentence.length());
  EXPECT_EQ(sentence.substr(0, index), first_sentence);
}

TEST_F(ReadAnythingAppModelTest,
       GetNextSentence_OnlyOneSentence_ReturnsCorrectIndex) {
  const std::u16string sentence = u"Hello, this is a normal sentence.";

  size_t index = GetNextSentence(sentence);
  EXPECT_EQ(index, sentence.length());
  EXPECT_EQ(sentence.substr(0, index), sentence);
}

TEST_F(ReadAnythingAppModelTest, GetNextWord_ReturnsCorrectIndex) {
  const std::u16string first_word = u"onomatopoeia ";
  const std::u16string second_word = u"party";

  const std::u16string segment = first_word + second_word;
  size_t index = GetNextWord(segment);
  EXPECT_EQ(index, first_word.length());
  EXPECT_EQ(segment.substr(0, index), first_word);
}

TEST_F(ReadAnythingAppModelTest, GetNextWord_OnlyOneWord_ReturnsCorrectIndex) {
  const std::u16string word = u"Happiness";

  size_t index = GetNextWord(word);
  EXPECT_EQ(index, word.length());
  EXPECT_EQ(word.substr(0, index), word);
}

TEST_F(ReadAnythingAppModelTest,
       GetNextSentence_NotPDF_DoesNotFilterReturnCharacters) {
  const std::u16string sentence =
      u"Hello, this is\n a sentence \r with line breaks.";

  size_t index = GetNextSentence(sentence);
  EXPECT_EQ(index, sentence.find('\n') + 2);
  EXPECT_EQ(sentence.substr(0, index), u"Hello, this is\n ");

  std::u16string next_sentence = sentence.substr(index);
  index = GetNextSentence(next_sentence);
  EXPECT_EQ(index, next_sentence.find('\r') + 2);
  EXPECT_EQ(next_sentence.substr(0, index), u"a sentence \r ");

  next_sentence = next_sentence.substr(index);
  index = GetNextSentence(next_sentence);
  EXPECT_EQ(index, next_sentence.length());
  EXPECT_EQ(next_sentence.substr(0, index), u"with line breaks.");
}

TEST_F(ReadAnythingAppModelTest, GetNextSentence_PDF_FiltersReturnCharacters) {
  set_is_pdf(true);
  const std::u16string sentence =
      u"Hello, this is\n a sentence \r with line breaks.";

  size_t index = GetNextSentence(sentence);
  EXPECT_EQ(index, sentence.length());
  EXPECT_EQ(sentence.substr(0, index), sentence);
}

TEST_F(ReadAnythingAppModelTest,
       GetNextSentence_PDF_DoesNotFilterReturnCharactersAtEndOfSentence) {
  set_is_pdf(true);
  const std::u16string sentence =
      u"Hello, this is a sentence with line breaks.\r\n";

  size_t index = GetNextSentence(sentence);
  EXPECT_EQ(index, sentence.length());
  EXPECT_EQ(sentence.substr(0, index), sentence);
}

TEST_F(ReadAnythingAppModelTest, GetNextValidPosition) {
  std::u16string sentence1 = u"This is a sentence.";
  std::u16string sentence2 = u"This is another sentence.";
  std::u16string sentence3 = u"And this is yet another sentence.";
  ui::AXTreeUpdate update;
  SetUpdateTreeID(&update);
  ui::AXNodeData static_text1;
  static_text1.id = 2;
  static_text1.role = ax::mojom::Role::kStaticText;
  static_text1.SetNameChecked(sentence1);

  ui::AXNodeData static_text2;
  static_text2.id = 3;
  static_text2.role = ax::mojom::Role::kStaticText;
  static_text2.SetNameChecked(sentence2);

  ui::AXNodeData static_text3;
  static_text3.id = 4;
  static_text3.role = ax::mojom::Role::kStaticText;
  static_text3.SetNameChecked(sentence3);
  update.nodes = {static_text1, static_text2, static_text3};
  AccessibilityEventReceived({update});
  ProcessDisplayNodes({static_text1.id, static_text2.id, static_text3.id});
  InitAXPosition(update.nodes[0].id);
  ui::AXNodePosition::AXPositionInstance new_position = GetNextNodePosition();
  EXPECT_EQ(new_position->anchor_id(), static_text2.id);
  EXPECT_EQ(new_position->GetText(), sentence2);

  // Getting the next node position shouldn't update the current AXPosition.
  new_position = GetNextNodePosition();
  EXPECT_EQ(new_position->anchor_id(), static_text2.id);
  EXPECT_EQ(new_position->GetText(), sentence2);
}

TEST_F(ReadAnythingAppModelTest, GetNextValidPosition_SkipsNonTextNode) {
  std::u16string sentence1 = u"This is a sentence.";
  std::u16string sentence2 = u"This is another sentence.";
  ui::AXTreeUpdate update;
  SetUpdateTreeID(&update);
  ui::AXNodeData static_text1;
  static_text1.id = 2;
  static_text1.role = ax::mojom::Role::kStaticText;
  static_text1.SetNameChecked(sentence1);

  ui::AXNodeData empty_node;
  empty_node.id = 3;

  ui::AXNodeData static_text2;
  static_text2.id = 4;
  static_text2.role = ax::mojom::Role::kStaticText;
  static_text2.SetNameChecked(sentence2);
  update.nodes = {static_text1, empty_node, static_text2};
  AccessibilityEventReceived({update});
  ProcessDisplayNodes({static_text1.id, empty_node.id, static_text2.id});
  InitAXPosition(update.nodes[0].id);
  ui::AXNodePosition::AXPositionInstance new_position = GetNextNodePosition();
  EXPECT_EQ(new_position->anchor_id(), static_text2.id);
  EXPECT_EQ(new_position->GetText(), sentence2);
}

TEST_F(ReadAnythingAppModelTest, GetNextValidPosition_SkipsNonDistilledNode) {
  std::u16string sentence1 = u"This is a sentence.";
  std::u16string sentence2 = u"This is another sentence.";
  std::u16string sentence3 = u"And this is yet another sentence.";
  ui::AXTreeUpdate update;
  SetUpdateTreeID(&update);
  ui::AXNodeData static_text1;
  static_text1.id = 2;
  static_text1.role = ax::mojom::Role::kStaticText;
  static_text1.SetNameChecked(sentence1);

  ui::AXNodeData static_text2;
  static_text2.id = 3;
  static_text2.role = ax::mojom::Role::kStaticText;
  static_text2.SetNameChecked(sentence2);

  ui::AXNodeData static_text3;
  static_text3.id = 4;
  static_text3.role = ax::mojom::Role::kStaticText;
  static_text3.SetName(sentence3);
  update.nodes = {static_text1, static_text2, static_text3};
  AccessibilityEventReceived({update});
  // Don't distill the node with id 3.
  ProcessDisplayNodes({static_text1.id, static_text3.id});
  InitAXPosition(update.nodes[0].id);
  ui::AXNodePosition::AXPositionInstance new_position = GetNextNodePosition();
  EXPECT_EQ(new_position->anchor_id(), static_text3.id);
  EXPECT_EQ(new_position->GetText(), sentence3);
}

TEST_F(ReadAnythingAppModelTest, GetNextValidPosition_SkipsNodeWithHTMLTag) {
  std::u16string sentence1 = u"This is a sentence.";
  std::u16string sentence2 = u"This is another sentence.";
  std::u16string sentence3 = u"And this is yet another sentence.";
  ui::AXTreeUpdate update;
  SetUpdateTreeID(&update);
  ui::AXNodeData static_text1;
  static_text1.id = 2;
  static_text1.role = ax::mojom::Role::kStaticText;
  static_text1.SetNameChecked(sentence1);

  ui::AXNodeData static_text2;
  static_text2.id = 3;
  static_text2.role = ax::mojom::Role::kStaticText;
  static_text2.AddStringAttribute(ax::mojom::StringAttribute::kHtmlTag, "h1");
  static_text2.SetNameChecked(sentence2);

  ui::AXNodeData static_text3;
  static_text3.id = 4;
  static_text3.role = ax::mojom::Role::kStaticText;
  static_text3.SetNameChecked(sentence3);
  update.nodes = {static_text1, static_text2, static_text3};
  AccessibilityEventReceived({update});
  ProcessDisplayNodes({static_text1.id, static_text2.id, static_text3.id});
  InitAXPosition(update.nodes[0].id);
  ui::AXNodePosition::AXPositionInstance new_position = GetNextNodePosition();
  EXPECT_EQ(new_position->anchor_id(), static_text3.id);
  EXPECT_EQ(new_position->GetText(), sentence3);
}

TEST_F(ReadAnythingAppModelTest,
       GetNextValidPosition_ReturnsNullPositionAtEndOfTree) {
  std::u16string sentence1 = u"This is a sentence.";
  ui::AXTreeUpdate update;
  SetUpdateTreeID(&update);
  ui::AXNodeData static_text;
  static_text.id = 2;
  static_text.role = ax::mojom::Role::kStaticText;
  static_text.SetNameChecked(sentence1);
  ui::AXNodeData empty_node1;
  empty_node1.id = 3;
  ui::AXNodeData empty_node2;
  empty_node2.id = 4;
  update.nodes = {static_text, empty_node1, empty_node2};
  AccessibilityEventReceived({update});
  ProcessDisplayNodes({static_text.id, empty_node1.id, empty_node2.id});
  InitAXPosition(update.nodes[0].id);
  ui::AXNodePosition::AXPositionInstance new_position = GetNextNodePosition();
  EXPECT_TRUE(new_position->IsNullPosition());
}

TEST_F(
    ReadAnythingAppModelTest,
    GetNextValidPosition_AfterGetNextNodesButBeforeGetCurrentText_UsesCurrentGranularity) {
  std::u16string sentence1 = u"But from up here. The ";
  std::u16string sentence2 = u"world ";
  std::u16string sentence3 =
      u"looks so small. And suddenly life seems so clear. And from up here. "
      u"You coast past it all. The obstacles just disappear.";
  ui::AXTreeUpdate update;
  SetUpdateTreeID(&update);
  ui::AXNodeData static_text1;
  static_text1.id = 2;
  static_text1.role = ax::mojom::Role::kStaticText;
  static_text1.SetNameChecked(sentence1);

  ui::AXNodeData static_text2;
  static_text2.id = 3;
  static_text2.role = ax::mojom::Role::kStaticText;
  static_text2.SetNameChecked(sentence2);

  ui::AXNodeData static_text3;
  static_text3.id = 4;
  static_text3.role = ax::mojom::Role::kStaticText;
  static_text3.SetNameChecked(sentence3);
  update.nodes = {static_text1, static_text2, static_text3};
  AccessibilityEventReceived({update});
  ProcessDisplayNodes({static_text1.id, static_text2.id, static_text3.id});
  InitAXPosition(update.nodes[0].id);

  ReadAnythingAppModel::ReadAloudCurrentGranularity current_granularity =
      GetNextNodes();
  // Expect that current_granularity contains static_text1
  // Expect that the indices aren't returned correctly
  // Expect that GetNextValidPosition fails without inserted the granularity.
  // The first segment was returned correctly.
  EXPECT_EQ((int)current_granularity.node_ids.size(), 1);
  EXPECT_TRUE(base::Contains(current_granularity.node_ids, static_text1.id));
  EXPECT_EQ(GetCurrentTextStartIndex(static_text1.id), -1);
  EXPECT_EQ(GetCurrentTextEndIndex(static_text1.id), -1);

  // Get the next position without using the current granularity. This
  // simulates getting the next node position from within GetNextNode if
  // the current granularity hasn't yet been added to the list processed
  // granularities. This should return the ID for static_text1, even though
  // it's already been used because the current granularity isn't being used.
  ui::AXNodePosition::AXPositionInstance new_position = GetNextNodePosition();
  EXPECT_EQ(new_position->anchor_id(), static_text1.id);

  // Now get the next position using the correct current granularity. Thi
  // simulates calling GetNextNodePosition from within GetNextNodes before
  // the nodes have been added to the list of processed granularities. This
  // should correctly return the next node in the tree.
  new_position = GetNextNodePosition(current_granularity);
  EXPECT_EQ(new_position->anchor_id(), static_text2.id);
}

TEST_F(ReadAnythingAppModelTest,
       GetNextNodes_AfterResetReadAloudState_StartsOver) {
  std::u16string sentence1 = u"Where the north wind meets the sea. ";
  std::u16string sentence2 = u"There's a river full of memory. ";
  std::u16string sentence3 = u"Sleep my darling safe and sound. ";
  ui::AXTreeUpdate update;
  SetUpdateTreeID(&update);
  ui::AXNodeData static_text1;
  static_text1.id = 2;
  static_text1.role = ax::mojom::Role::kStaticText;
  static_text1.SetNameChecked(sentence1);

  ui::AXNodeData static_text2;
  static_text2.id = 3;
  static_text2.role = ax::mojom::Role::kStaticText;
  static_text2.SetNameChecked(sentence2);

  ui::AXNodeData static_text3;
  static_text3.id = 4;
  static_text3.role = ax::mojom::Role::kStaticText;
  static_text3.SetNameChecked(sentence3);
  update.nodes = {static_text1, static_text2, static_text3};
  AccessibilityEventReceived({update});
  ProcessDisplayNodes({static_text1.id, static_text2.id, static_text3.id});
  InitAXPosition(update.nodes[0].id);

  // Get first and second granularity.
  ReadAnythingAppModel::ReadAloudCurrentGranularity first_granularity =
      GetNextNodes();
  EXPECT_EQ((int)first_granularity.node_ids.size(), 1);
  EXPECT_TRUE(base::Contains(first_granularity.node_ids, static_text1.id));
  EXPECT_EQ(first_granularity.text, sentence1);
  ReadAnythingAppModel::ReadAloudCurrentGranularity next_granularity =
      GetNextNodes();
  EXPECT_EQ((int)next_granularity.node_ids.size(), 1);
  EXPECT_TRUE(base::Contains(next_granularity.node_ids, static_text2.id));
  EXPECT_EQ(next_granularity.text, sentence2);

  // If we init without resetting we should just go to the next sentence
  InitAXPosition(update.nodes[0].id);
  ReadAnythingAppModel::ReadAloudCurrentGranularity last_granularity =
      GetNextNodes();
  EXPECT_EQ((int)last_granularity.node_ids.size(), 1);
  EXPECT_TRUE(base::Contains(last_granularity.node_ids, static_text3.id));
  EXPECT_EQ(last_granularity.text, sentence3);

  // After reset and then init, we should get the first sentence again.
  ResetReadAloudState();
  InitAXPosition(update.nodes[0].id);
  ReadAnythingAppModel::ReadAloudCurrentGranularity after_reset =
      GetNextNodes();
  EXPECT_EQ((int)after_reset.node_ids.size(), 1);
  EXPECT_TRUE(base::Contains(after_reset.node_ids, static_text1.id));
  EXPECT_EQ(first_granularity.text, sentence1);
}

TEST_F(ReadAnythingAppModelTest,
       GetNodeIdForCurrentSegmentIndex_ReturnsCorrectNodes) {
  std::u16string sentence1 = u"Never feel heavy ";
  std::u16string sentence2 = u"or earthbound, ";
  std::u16string sentence3 = u"no worries or doubts interfere.";
  ui::AXTreeUpdate update;
  SetUpdateTreeID(&update);
  ui::AXNodeData static_text1;
  static_text1.id = 2;
  static_text1.role = ax::mojom::Role::kStaticText;
  static_text1.SetNameChecked(sentence1);

  ui::AXNodeData static_text2;
  static_text2.id = 3;
  static_text2.role = ax::mojom::Role::kStaticText;
  static_text2.SetNameChecked(sentence2);

  ui::AXNodeData static_text3;
  static_text3.id = 4;
  static_text3.role = ax::mojom::Role::kStaticText;
  static_text3.SetNameChecked(sentence3);
  update.nodes = {static_text1, static_text2, static_text3};
  AccessibilityEventReceived({update});
  ProcessDisplayNodes({static_text1.id, static_text2.id, static_text3.id});
  InitAXPosition(update.nodes[0].id);

  // Before there are any processed granularities,
  // GetNodeIdForCurrentSegmentIndex should return an invalid id.
  EXPECT_EQ(GetNodeIdForCurrentSegmentIndex(1), ui::kInvalidAXNodeID);

  std::vector<ui::AXNodeID> node_ids = GetCurrentText();
  EXPECT_EQ((int)node_ids.size(), 3);

  // Spot check that indices 0->sentence1.length() map to the first node id.
  EXPECT_EQ(GetNodeIdForCurrentSegmentIndex(0), static_text1.id);
  EXPECT_EQ(GetNodeIdForCurrentSegmentIndex(7), static_text1.id);
  EXPECT_EQ(GetNodeIdForCurrentSegmentIndex(sentence1.length()),
            static_text1.id);

  // Spot check that indices in sentence 2 map to the second node id.
  EXPECT_EQ(GetNodeIdForCurrentSegmentIndex(sentence1.length() + 1),
            static_text2.id);
  EXPECT_EQ(GetNodeIdForCurrentSegmentIndex(26), static_text2.id);
  EXPECT_EQ(
      GetNodeIdForCurrentSegmentIndex(sentence1.length() + sentence2.length()),
      static_text2.id);

  // Spot check that indices in sentence 3 map to the third node id.
  EXPECT_EQ(GetNodeIdForCurrentSegmentIndex(sentence1.length() +
                                            sentence2.length() + 1),
            static_text3.id);
  EXPECT_EQ(GetNodeIdForCurrentSegmentIndex(40), static_text3.id);
  EXPECT_EQ(GetNodeIdForCurrentSegmentIndex(
                sentence1.length() + sentence2.length() + sentence3.length()),
            static_text3.id);

  // Out-of-bounds nodes return invalid.
  EXPECT_EQ(
      GetNodeIdForCurrentSegmentIndex(sentence1.length() + sentence2.length() +
                                      sentence3.length() + 1),
      ui::kInvalidAXNodeID);
  EXPECT_EQ(GetNodeIdForCurrentSegmentIndex(535), ui::kInvalidAXNodeID);
  EXPECT_EQ(GetNodeIdForCurrentSegmentIndex(-10), ui::kInvalidAXNodeID);
}

TEST_F(ReadAnythingAppModelTest,
       GetNodeIdForCurrentSegmentIndex_AfterNext_ReturnsCorrectNodes) {
  std::u16string sentence1 = u"Never feel heavy or earthbound. ";
  std::u16string sentence2 = u"No worries or doubts ";
  std::u16string sentence3 = u"interfere.";
  ui::AXTreeUpdate update;
  SetUpdateTreeID(&update);
  ui::AXNodeData static_text1;
  static_text1.id = 2;
  static_text1.role = ax::mojom::Role::kStaticText;
  static_text1.SetNameChecked(sentence1);

  ui::AXNodeData static_text2;
  static_text2.id = 3;
  static_text2.role = ax::mojom::Role::kStaticText;
  static_text2.SetNameChecked(sentence2);

  ui::AXNodeData static_text3;
  static_text3.id = 4;
  static_text3.role = ax::mojom::Role::kStaticText;
  static_text3.SetNameChecked(sentence3);
  update.nodes = {static_text1, static_text2, static_text3};
  AccessibilityEventReceived({update});
  ProcessDisplayNodes({static_text1.id, static_text2.id, static_text3.id});
  InitAXPosition(update.nodes[0].id);

  // Before there are any processed granularities,
  // GetNodeIdForCurrentSegmentIndex should return an invalid id.
  EXPECT_EQ(GetNodeIdForCurrentSegmentIndex(1), ui::kInvalidAXNodeID);

  std::vector<ui::AXNodeID> node_ids = GetCurrentText();
  EXPECT_EQ((int)node_ids.size(), 1);

  // Spot check that indices 0->sentence1.length() map to the first node id.
  EXPECT_EQ(GetNodeIdForCurrentSegmentIndex(0), static_text1.id);
  EXPECT_EQ(GetNodeIdForCurrentSegmentIndex(7), static_text1.id);
  EXPECT_EQ(GetNodeIdForCurrentSegmentIndex(sentence1.length()),
            static_text1.id);
  EXPECT_EQ(GetNodeIdForCurrentSegmentIndex(sentence1.length() + 1),
            ui::kInvalidAXNodeID);

  // Move to the next granularity.
  MovePositionToNextGranularity();
  node_ids = GetCurrentText();
  EXPECT_EQ((int)node_ids.size(), 2);

  // Spot check that indices in sentence 2 map to the second node id.
  EXPECT_EQ(GetNodeIdForCurrentSegmentIndex(0), static_text2.id);
  EXPECT_EQ(GetNodeIdForCurrentSegmentIndex(7), static_text2.id);
  EXPECT_EQ(GetNodeIdForCurrentSegmentIndex(sentence2.length()),
            static_text2.id);

  // Spot check that indices in sentence 3 map to the third node id.
  EXPECT_EQ(GetNodeIdForCurrentSegmentIndex(sentence2.length() + 1),
            static_text3.id);
  EXPECT_EQ(GetNodeIdForCurrentSegmentIndex(27), static_text3.id);
  EXPECT_EQ(
      GetNodeIdForCurrentSegmentIndex(sentence2.length() + sentence3.length()),
      static_text3.id);

  // Out-of-bounds nodes return invalid.
  EXPECT_EQ(GetNodeIdForCurrentSegmentIndex(sentence2.length() +
                                            sentence3.length() + 1),
            ui::kInvalidAXNodeID);
}

TEST_F(ReadAnythingAppModelTest,
       GetNodeIdForCurrentSegmentIndex_AfterPrevious_ReturnsCorrectNodes) {
  std::u16string sentence1 = u"There's nothing but you ";
  std::u16string sentence2 = u"looking down on the view from up here. ";
  std::u16string sentence3 = u"Stretch out with the wind behind you.";
  ui::AXTreeUpdate update;
  SetUpdateTreeID(&update);
  ui::AXNodeData static_text1;
  static_text1.id = 2;
  static_text1.role = ax::mojom::Role::kStaticText;
  static_text1.SetNameChecked(sentence1);

  ui::AXNodeData static_text2;
  static_text2.id = 3;
  static_text2.role = ax::mojom::Role::kStaticText;
  static_text2.SetNameChecked(sentence2);

  ui::AXNodeData static_text3;
  static_text3.id = 4;
  static_text3.role = ax::mojom::Role::kStaticText;
  static_text3.SetNameChecked(sentence3);
  update.nodes = {static_text1, static_text2, static_text3};
  AccessibilityEventReceived({update});
  ProcessDisplayNodes({static_text1.id, static_text2.id, static_text3.id});
  InitAXPosition(update.nodes[0].id);

  // Before there are any processed granularities,
  // GetNodeIdForCurrentSegmentIndex should return an invalid id.
  EXPECT_EQ(GetNodeIdForCurrentSegmentIndex(1), ui::kInvalidAXNodeID);

  std::vector<ui::AXNodeID> node_ids = GetCurrentText();
  EXPECT_EQ((int)node_ids.size(), 2);

  // Move forward.
  MovePositionToNextGranularity();
  node_ids = GetCurrentText();
  EXPECT_EQ((int)node_ids.size(), 1);

  // Spot check that indices 0->sentence3.length() map to the third node id.
  EXPECT_EQ(GetNodeIdForCurrentSegmentIndex(0), static_text3.id);
  EXPECT_EQ(GetNodeIdForCurrentSegmentIndex(7), static_text3.id);
  EXPECT_EQ(GetNodeIdForCurrentSegmentIndex(sentence3.length()),
            static_text3.id);

  // Move backwards.
  MovePositionToPreviousGranularity();
  node_ids = GetCurrentText();
  EXPECT_EQ((int)node_ids.size(), 2);

  // Spot check that indices in sentence 1 map to the first node id.
  EXPECT_EQ(GetNodeIdForCurrentSegmentIndex(0), static_text1.id);
  EXPECT_EQ(GetNodeIdForCurrentSegmentIndex(6), static_text1.id);
  EXPECT_EQ(GetNodeIdForCurrentSegmentIndex(sentence1.length()),
            static_text1.id);

  // Spot check that indices in sentence 2 map to the second node id.
  EXPECT_EQ(GetNodeIdForCurrentSegmentIndex(sentence1.length() + 1),
            static_text2.id);
  EXPECT_EQ(GetNodeIdForCurrentSegmentIndex(27), static_text2.id);
  EXPECT_EQ(
      GetNodeIdForCurrentSegmentIndex(sentence1.length() + sentence2.length()),
      static_text2.id);

  // Out-of-bounds nodes return invalid.
  EXPECT_EQ(GetNodeIdForCurrentSegmentIndex(sentence1.length() +
                                            sentence2.length() + 1),
            ui::kInvalidAXNodeID);
}

TEST_F(ReadAnythingAppModelTest,
       GetNextWordHighlightLength_ReturnsCorrectLength) {
  std::u16string word1 = u"Stretch ";
  std::u16string word2 = u"out ";
  std::u16string word3 = u"with ";
  std::u16string word4 = u"the ";
  std::u16string word5 = u"wind ";
  std::u16string word6 = u"behind ";
  std::u16string word7 = u"you.";
  std::u16string sentence =
      word1 + word2 + word3 + word4 + word5 + word6 + word7;
  ui::AXTreeUpdate update;
  SetUpdateTreeID(&update);
  ui::AXNodeData static_text1;
  static_text1.id = 2;
  static_text1.role = ax::mojom::Role::kStaticText;
  static_text1.SetNameChecked(sentence);

  update.nodes = {static_text1};
  AccessibilityEventReceived({update});
  ProcessDisplayNodes({static_text1.id});
  InitAXPosition(update.nodes[0].id);

  // Before there are any processed granularities,
  // GetNodeIdForCurrentSegmentIndex should return an invalid id.
  EXPECT_EQ(GetNodeIdForCurrentSegmentIndex(1), ui::kInvalidAXNodeID);

  std::vector<ui::AXNodeID> node_ids = GetCurrentText();
  EXPECT_EQ((int)node_ids.size(), 1);

  // Throughout first word.
  EXPECT_EQ(GetWordLength(0), (int)word1.length());
  EXPECT_EQ(GetWordLength(2), (int)word1.length() - 2);
  EXPECT_EQ(GetWordLength((int)word1.length()) - 2, 2);

  // Throughout third word.
  int third_word_index = sentence.find(word3);
  EXPECT_EQ(GetWordLength(third_word_index), (int)word3.length());
  EXPECT_EQ(GetWordLength(third_word_index + 2), (int)word3.length() - 2);

  int last_word_index = sentence.find(word7);
  EXPECT_EQ(GetWordLength(last_word_index), (int)word7.length());
  EXPECT_EQ(GetWordLength(last_word_index + 2), (int)word7.length() - 2);

  // Boundary testing.
  EXPECT_EQ(GetWordLength(-5), 0);
  EXPECT_EQ(GetWordLength(sentence.length()), 0);
  EXPECT_EQ(GetWordLength(sentence.length() + 1), 0);
}

TEST_F(ReadAnythingAppModelTest, LastExpandedNodeNamedChanged_TriggersRedraw) {
  ui::AXTreeUpdate inital_update;
  SetUpdateTreeID(&inital_update);
  ui::AXNodeData inital_node;
  inital_node.id = 2;
  inital_node.role = ax::mojom::Role::kStaticText;
  inital_node.SetNameChecked("Old Name");
  inital_update.nodes = {inital_node};
  AccessibilityEventReceived({inital_update});

  ui::AXTreeUpdate update;
  SetUpdateTreeID(&update);
  ui::AXNodeData updated_node;
  updated_node.id = inital_node.id;
  updated_node.role = ax::mojom::Role::kStaticText;
  updated_node.SetNameChecked("New Name");
  update.nodes = {updated_node};
  SetLastExpandedNodeId(inital_node.id);
  EXPECT_EQ(LastExpandedNodeId(), inital_node.id);
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
