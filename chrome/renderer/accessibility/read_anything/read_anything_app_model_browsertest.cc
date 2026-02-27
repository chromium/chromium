// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/accessibility/read_anything/read_anything_app_model.h"

#include <algorithm>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "base/threading/platform_thread.h"
#include "base/time/time.h"
#include "chrome/test/base/chrome_render_view_test.h"
#include "read_anything_app_model.h"
#include "read_anything_test_utils.h"
#include "services/strings/grit/services_strings.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/accessibility/ax_enums.mojom-shared.h"
#include "ui/accessibility/ax_event.h"
#include "ui/accessibility/ax_event_intent.h"
#include "ui/accessibility/ax_node_id_forward.h"
#include "ui/accessibility/ax_serializable_tree.h"
#include "ui/accessibility/ax_updates_and_events.h"
#include "ui/base/l10n/l10n_util.h"

using ::testing::ElementsAre;
using ::testing::UnorderedElementsAre;

class ReadAnythingAppModelNoInitTest : public ChromeRenderViewTest {
 public:
  ReadAnythingAppModelNoInitTest() = default;
  ReadAnythingAppModelNoInitTest(const ReadAnythingAppModelNoInitTest&) =
      delete;
  ReadAnythingAppModelNoInitTest& operator=(
      const ReadAnythingAppModelNoInitTest&) = delete;
  ~ReadAnythingAppModelNoInitTest() override = default;

  const ReadAnythingAppModel& model() const { return model_; }

 private:
  ReadAnythingAppModel model_;
};

TEST_F(ReadAnythingAppModelNoInitTest, IsDocs_FalseBeforeTreeInitialization) {
  EXPECT_FALSE(model().IsDocs());
}

TEST_F(ReadAnythingAppModelNoInitTest, IsReload_FalseBeforeTreeInitialization) {
  EXPECT_FALSE(model().IsReload());
}

class ReadAnythingAppModelTest : public ChromeRenderViewTest {
 public:
  ReadAnythingAppModelTest() = default;
  ReadAnythingAppModelTest(const ReadAnythingAppModelTest&) = delete;
  ReadAnythingAppModelTest& operator=(const ReadAnythingAppModelTest&) = delete;
  ~ReadAnythingAppModelTest() override = default;

  const std::string DOCS_URL =
      "https://docs.google.com/document/d/"
      "1t6x1PQaQWjE8wb9iyYmFaoK1XAEgsl8G1Hx3rzfpoKA/"
      "edit?ouid=103677288878638916900&usp=docs_home&ths=true";

  void SetUp() override {
    ChromeRenderViewTest::SetUp();

    // Create a tree id.
    tree_id_ = ui::AXTreeID::CreateNewAXTreeID();

    // Create simple AXTreeUpdate with a root node and 3 children.
    std::unique_ptr<ui::AXTreeUpdate> snapshot = test::CreateInitialUpdate();
    test::SetUpdateTreeID(snapshot.get(), tree_id_);

    ApplyAccessibilityUpdates(tree_id_, {*snapshot});
    model().SetActiveTreeId(tree_id_);
    model().Reset({});
  }

  ReadAnythingAppModel& model() { return model_; }
  const ReadAnythingAppModel& model() const { return model_; }

  bool AreAllPendingUpdatesEmpty() const {
    return std::ranges::all_of(
        model().pending_updates_for_testing(), [](const auto& pair) {
          const auto& updateList = pair.second;
          return std::ranges::all_of(updateList,
                                     &ReadAnythingAppModel::Updates::empty);
        });
  }

  void ApplyAccessibilityUpdates(const ui::AXTreeID& tree_id,
                                 const std::vector<ui::AXTreeUpdate>& updates) {
    std::vector<ui::AXEvent> events;
    model().ApplyAccessibilityUpdates(
        tree_id, const_cast<std::vector<ui::AXTreeUpdate>&>(updates), events);
  }

  void QueueAccessibilityUpdates(const ui::AXTreeID& tree_id,
                                 const std::vector<ui::AXTreeUpdate>& updates) {
    std::vector<ui::AXEvent> events;
    model().QueueAccessibilityUpdates(
        tree_id, const_cast<std::vector<ui::AXTreeUpdate>&>(updates), events);
  }

  std::set<ui::AXNodeID> GetNotIgnoredIds(base::span<const ui::AXNodeID> ids) {
    std::set<ui::AXNodeID> set;
    for (auto id : ids) {
      model().InsertIdIfNotIgnored(id, set);
    }
    return set;
  }

  void ProcessDisplayNodes(std::vector<ui::AXNodeID> content_node_ids) {
    model().Reset(std::move(content_node_ids));
    model().ComputeDisplayNodeIdsForDistilledTree();
  }

  void SetUrlInformationCallback() { ranSetUrlInformationCallback_ = true; }

  bool RanSetUrlInformationCallback() { return ranSetUrlInformationCallback_; }

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
    ApplyAccessibilityUpdates(tree_id_, {std::move(initial_update)});
    return child_ids;
  }

  ui::AXTreeID tree_id_;

 private:
  ReadAnythingAppModel model_;
  bool ranSetUrlInformationCallback_ = false;
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(ReadAnythingAppModelTest, FontName) {
  EXPECT_NE(model().font_name(), std::string());

  std::string font_name = "Montserrat";
  model().set_font_name(font_name);
  EXPECT_EQ(font_name, model().font_name());
}

TEST_F(ReadAnythingAppModelTest, OnSettingsRestoredFromPrefs) {
  auto line_spacing = read_anything::mojom::LineSpacing::kLoose;
  auto letter_spacing = read_anything::mojom::LetterSpacing::kWide;
  std::string font_name = "Roboto";
  double font_size = 3.0;
  bool links_enabled = false;
  bool images_enabled = true;
  auto color = read_anything::mojom::Colors::kDark;
  auto line_focus_enabled_mode =
      read_anything::mojom::LineFocus::kMediumCursorWindow;

  model().OnSettingsRestoredFromPrefs(line_spacing, letter_spacing, font_name,
                                      font_size, links_enabled, images_enabled,
                                      color, line_focus_enabled_mode, false);

  EXPECT_EQ(line_spacing, model().line_spacing());
  EXPECT_EQ(letter_spacing, model().letter_spacing());
  EXPECT_EQ(font_name, model().font_name());
  EXPECT_EQ(font_size, model().font_size());
  EXPECT_EQ(links_enabled, model().links_enabled());
  EXPECT_EQ(images_enabled, model().images_enabled());
  EXPECT_EQ(color, model().color_theme());
  EXPECT_EQ(line_focus_enabled_mode, model().last_non_disabled_line_focus());
  EXPECT_FALSE(model().line_focus_enabled());
}

TEST_F(ReadAnythingAppModelTest, ResetLineFocusSession_ResetsToStartingValue) {
  const std::optional<base::TimeTicks> start_time =
      model().line_focus_session_start_time();
  const int mouse_distance = model().line_focus_mouse_distance();
  const int scroll_distance = model().line_focus_scroll_distance();
  const int keyboard_lines = model().line_focus_keyboard_lines();
  const int speech_lines = model().line_focus_speech_lines();
  model().set_line_focus_session_start_time(base::TimeTicks::Now());
  model().set_line_focus_mouse_distance(mouse_distance + 100);
  model().set_line_focus_scroll_distance(scroll_distance + 100);
  model().set_line_focus_keyboard_lines(keyboard_lines + 100);
  model().set_line_focus_speech_lines(speech_lines + 100);

  model().ResetLineFocusSession();

  ASSERT_EQ(model().line_focus_session_start_time(), start_time);
  ASSERT_EQ(model().line_focus_mouse_distance(), mouse_distance);
  ASSERT_EQ(model().line_focus_scroll_distance(), scroll_distance);
  ASSERT_EQ(model().line_focus_keyboard_lines(), keyboard_lines);
  ASSERT_EQ(model().line_focus_speech_lines(), speech_lines);
}

TEST_F(ReadAnythingAppModelTest, SetTreeInfoUrlInformation_RunsCallback) {
  ui::AXTreeUpdate update;
  ui::AXTreeID tree_id = ui::AXTreeID::CreateNewAXTreeID();
  test::SetUpdateTreeID(&update, tree_id);
  ui::AXNodeData root = test::LinkNode(/* id= */ 1, DOCS_URL);
  update.root_id = root.id;
  update.nodes = {std::move(root)};
  model().SetUrlInformationCallback(
      base::BindOnce(&ReadAnythingAppModelTest::SetUrlInformationCallback,
                     base::Unretained(this)));
  EXPECT_FALSE(RanSetUrlInformationCallback());

  ApplyAccessibilityUpdates(tree_id, {std::move(update)});
  model().SetActiveTreeId(tree_id);

  EXPECT_TRUE(RanSetUrlInformationCallback());
}

TEST_F(ReadAnythingAppModelTest, SetTreeInfoUrlInformation_IsDocs) {
  ui::AXTreeUpdate update;
  ui::AXTreeID tree_id = ui::AXTreeID::CreateNewAXTreeID();
  test::SetUpdateTreeID(&update, tree_id);
  ui::AXNodeData root = test::LinkNode(/* id= */ 1, DOCS_URL);
  update.root_id = root.id;
  update.nodes = {std::move(root)};

  ApplyAccessibilityUpdates(tree_id, {std::move(update)});
  model().SetActiveTreeId(tree_id);

  EXPECT_TRUE(
      model().tree_infos_for_testing().at(tree_id)->is_url_information_set);
  EXPECT_TRUE(model().IsDocs());
}

TEST_F(ReadAnythingAppModelTest, SetTreeInfoUrlInformation_IsNotDocs) {
  ui::AXTreeUpdate update;
  ui::AXTreeID tree_id = ui::AXTreeID::CreateNewAXTreeID();
  test::SetUpdateTreeID(&update, tree_id);
  ui::AXNodeData root = test::LinkNode(/* id= */ 1, "https://www.google.com");
  update.root_id = root.id;
  update.nodes = {std::move(root)};

  ApplyAccessibilityUpdates(tree_id, {std::move(update)});
  model().SetActiveTreeId(tree_id);

  EXPECT_TRUE(
      model().tree_infos_for_testing().at(tree_id)->is_url_information_set);
  EXPECT_FALSE(model().IsDocs());
}

TEST_F(ReadAnythingAppModelTest,
       SetTreeInfoUrlInformation_FirstTreeIsNotReload) {
  ui::AXTreeUpdate update;
  ui::AXTreeID tree_id = ui::AXTreeID::CreateNewAXTreeID();
  test::SetUpdateTreeID(&update, tree_id);
  ui::AXNodeData root = test::LinkNode(/* id= */ 1, "https://www.google.com");
  update.root_id = root.id;
  update.nodes = {std::move(root)};

  ApplyAccessibilityUpdates(tree_id, {std::move(update)});
  model().SetActiveTreeId(tree_id);

  EXPECT_TRUE(
      model().tree_infos_for_testing().at(tree_id)->is_url_information_set);
  EXPECT_FALSE(model().IsReload());
}

TEST_F(ReadAnythingAppModelTest, SetTreeInfoUrlInformation_IsReload) {
  ui::AXTreeUpdate update1;
  ui::AXTreeID id_1 = ui::AXTreeID::CreateNewAXTreeID();
  test::SetUpdateTreeID(&update1, id_1);
  ui::AXNodeData root1 = test::LinkNode(/* id= */ 1, "https://www.google.com");
  update1.root_id = root1.id;
  update1.nodes = {std::move(root1)};

  ui::AXTreeUpdate update2;
  ui::AXTreeID id_2 = ui::AXTreeID::CreateNewAXTreeID();
  test::SetUpdateTreeID(&update2, id_2);
  ui::AXNodeData root2 = test::LinkNode(/* id= */ 5, "https://www.google.com");
  update2.root_id = root2.id;
  update2.nodes = {std::move(root2)};

  ApplyAccessibilityUpdates(id_1, {std::move(update1)});
  model().SetActiveTreeId(id_1);
  EXPECT_TRUE(
      model().tree_infos_for_testing().at(id_1)->is_url_information_set);
  EXPECT_FALSE(model().IsReload());

  ApplyAccessibilityUpdates(id_2, {std::move(update2)});
  model().SetActiveTreeId(id_2);
  EXPECT_TRUE(
      model().tree_infos_for_testing().at(id_2)->is_url_information_set);
  EXPECT_TRUE(model().IsReload());
}

TEST_F(ReadAnythingAppModelTest, SetTreeInfoUrlInformation_IsNotReload) {
  ui::AXTreeUpdate update1;
  ui::AXTreeID id_1 = ui::AXTreeID::CreateNewAXTreeID();
  test::SetUpdateTreeID(&update1, id_1);
  ui::AXNodeData root1 = test::LinkNode(/* id= */ 1, "https://www.google.com");
  update1.root_id = root1.id;
  update1.nodes = {std::move(root1)};

  ui::AXTreeUpdate update2;
  ui::AXTreeID id_2 = ui::AXTreeID::CreateNewAXTreeID();
  test::SetUpdateTreeID(&update2, id_2);
  ui::AXNodeData root2 = test::LinkNode(/* id= */ 5, "https://www.youtube.com");
  update2.root_id = root2.id;
  update2.nodes = {std::move(root2)};

  ApplyAccessibilityUpdates(id_1, {std::move(update1)});
  model().SetActiveTreeId(id_1);
  EXPECT_TRUE(
      model().tree_infos_for_testing().at(id_1)->is_url_information_set);
  EXPECT_FALSE(model().IsReload());

  ApplyAccessibilityUpdates(id_2, {std::move(update2)});
  model().SetActiveTreeId(id_2);
  EXPECT_TRUE(
      model().tree_infos_for_testing().at(id_2)->is_url_information_set);
  EXPECT_FALSE(model().IsReload());
}

TEST_F(ReadAnythingAppModelTest, InsertIdIfNotIgnored) {
  ui::AXTreeUpdate update;
  test::SetUpdateTreeID(&update, tree_id_);
  ui::AXNodeData static_text_node = test::TextNode(/* id = */ 2);

  ui::AXNodeData combobox_node;
  combobox_node.id = 3;
  combobox_node.role = ax::mojom::Role::kComboBoxGrouping;

  ui::AXNodeData button_node;
  button_node.id = 4;
  button_node.role = ax::mojom::Role::kButton;
  update.nodes = {std::move(static_text_node), std::move(combobox_node),
                  std::move(button_node)};

  ApplyAccessibilityUpdates(tree_id_, {std::move(update)});
  EXPECT_THAT(GetNotIgnoredIds({{2, 3, 4}}), UnorderedElementsAre(2));
}

TEST_F(ReadAnythingAppModelTest, InsertIdIfNotIgnored_TextFieldsNotIgnored) {
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
  update.nodes = {std::move(tree_node), std::move(textfield_with_combobox_node),
                  std::move(textfield_node)};

  ApplyAccessibilityUpdates(tree_id_, {std::move(update)});
  EXPECT_THAT(GetNotIgnoredIds({{2, 3, 4}}), UnorderedElementsAre(3, 4));
}

TEST_F(ReadAnythingAppModelTest,
       InsertIdIfNotIgnored_InaccessiblePDFPageNodes) {
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
  update.nodes = {std::move(root), std::move(banner_node),
                  std::move(static_text_start_node),
                  std::move(content_info_node),
                  std::move(static_text_end_node)};

  ApplyAccessibilityUpdates(tree_id_, {std::move(update)});
  EXPECT_THAT(GetNotIgnoredIds({{2, 3, 4, 5}}), UnorderedElementsAre(4));
}

TEST_F(ReadAnythingAppModelTest, AddAndRemoveTrees) {
  // Start with 1 tree (the tree created in SetUp).
  ASSERT_EQ(1u, model().tree_infos_for_testing().size());
  ASSERT_TRUE(model().ContainsTree(tree_id_));

  // Create two new trees with new tree IDs.
  std::vector<ui::AXTreeID> tree_ids = {ui::AXTreeID::CreateNewAXTreeID(),
                                        ui::AXTreeID::CreateNewAXTreeID()};
  for (size_t i = 0; i < tree_ids.size(); ++i) {
    ui::AXTreeUpdate update;
    test::SetUpdateTreeID(&update, tree_ids[i]);
    ui::AXNodeData node;
    node.id = 1;
    update.root_id = node.id;
    update.nodes = {std::move(node)};
    ApplyAccessibilityUpdates(tree_ids[i], {std::move(update)});
    ASSERT_EQ(i + 2, model().tree_infos_for_testing().size());
    ASSERT_TRUE(model().ContainsTree(tree_id_));
    for (size_t j = 0; j <= i; ++j) {
      ASSERT_TRUE(model().ContainsTree(tree_ids[j]));
    }
  }

  // Remove all of the trees.
  model().OnAXTreeDestroyed(tree_id_);
  ASSERT_EQ(2u, model().tree_infos_for_testing().size());
  ASSERT_TRUE(model().ContainsTree(tree_ids[0]));
  ASSERT_TRUE(model().ContainsTree(tree_ids[1]));
  model().OnAXTreeDestroyed(tree_ids[0]);
  ASSERT_EQ(1u, model().tree_infos_for_testing().size());
  ASSERT_TRUE(model().ContainsTree(tree_ids[1]));
  model().OnAXTreeDestroyed(tree_ids[1]);
  ASSERT_EQ(0u, model().tree_infos_for_testing().size());
}

TEST_F(ReadAnythingAppModelTest, ApplyAccessibilityUpdates_OnInactiveTree) {
  EXPECT_FALSE(model().pending_updates_for_testing().contains(tree_id_));

  // Create a new tree.
  ui::AXTreeID tree_id_2 = ui::AXTreeID::CreateNewAXTreeID();
  ui::AXTreeUpdate update_2;
  test::SetUpdateTreeID(&update_2, tree_id_2);
  ui::AXNodeData node;
  node.id = 1;
  update_2.root_id = node.id;
  update_2.nodes = {std::move(node)};

  // Updates on inactive trees are processed immediately and are not marked as
  // pending.
  ApplyAccessibilityUpdates(tree_id_2, {std::move(update_2)});
  EXPECT_FALSE(model().pending_updates_for_testing().contains(tree_id_));
}

TEST_F(ReadAnythingAppModelTest,
       AddPendingUpdatesAfterUnserializingOnSameTree_DoesNotCrash) {
  std::vector<int> child_ids = SendSimpleUpdateAndGetChildIds();
  ReadAnythingAppModel::Updates updates =
      test::CreateSimpleUpdateList(child_ids, tree_id_);

  // Send update 0, which starts distillation.
  ApplyAccessibilityUpdates(tree_id_, {std::move(updates[0])});
  EXPECT_FALSE(model().pending_updates_for_testing().contains(tree_id_));
  ASSERT_TRUE(AreAllPendingUpdatesEmpty());

  // Queue update 1. This will not be unserialized yet.
  QueueAccessibilityUpdates(tree_id_, {std::move(updates[1])});
  EXPECT_EQ(1u, model().pending_updates_for_testing().at(tree_id_).size());

  // Ensure that there are no crashes after an accessibility event is received
  // immediately after unserializing.
  model().UnserializePendingUpdates(tree_id_);
  QueueAccessibilityUpdates(tree_id_, {std::move(updates[2])});
  EXPECT_EQ(1u, model().pending_updates_for_testing().at(tree_id_).size());
  ASSERT_FALSE(AreAllPendingUpdatesEmpty());
}

TEST_F(ReadAnythingAppModelTest, OnTreeErased_ClearsPendingUpdates) {
  std::vector<int> child_ids = SendSimpleUpdateAndGetChildIds();
  ReadAnythingAppModel::Updates updates =
      test::CreateSimpleUpdateList(child_ids, tree_id_);

  // Send update 0, which starts distillation.
  ApplyAccessibilityUpdates(tree_id_, {std::move(updates[0])});
  EXPECT_FALSE(model().pending_updates_for_testing().contains(tree_id_));
  ASSERT_TRUE(AreAllPendingUpdatesEmpty());

  // Queue update 1. This will not be unserialized yet.
  QueueAccessibilityUpdates(tree_id_, {std::move(updates[1])});
  EXPECT_EQ(1u, model().pending_updates_for_testing().at(tree_id_).size());

  // Destroy the tree.
  model().OnAXTreeDestroyed(tree_id_);
  EXPECT_FALSE(model().pending_updates_for_testing().contains(tree_id_));
}

TEST_F(ReadAnythingAppModelTest,
       QueueAccessibilityUpdates_UnserializesUpdates) {
  std::vector<int> child_ids = SendSimpleUpdateAndGetChildIds();
  ReadAnythingAppModel::Updates updates =
      test::CreateSimpleUpdateList(child_ids, tree_id_);

  // Send update 0, which starts distillation.
  ApplyAccessibilityUpdates(tree_id_, {std::move(updates[0])});
  EXPECT_FALSE(model().pending_updates_for_testing().contains(tree_id_));
  ASSERT_TRUE(AreAllPendingUpdatesEmpty());

  // Queue update 1. This will not be unserialized yet.
  QueueAccessibilityUpdates(tree_id_, {std::move(updates[1])});
  EXPECT_EQ(1u, model().pending_updates_for_testing().at(tree_id_).size());

  // Queue update 2. This is still not unserialized yet.
  QueueAccessibilityUpdates(tree_id_, {std::move(updates[2])});
  EXPECT_EQ(2u, model().pending_updates_for_testing().at(tree_id_).size());

  // Complete distillation which unserializes the pending updates and distills
  // them.
  model().UnserializePendingUpdates(tree_id_);
  EXPECT_FALSE(model().pending_updates_for_testing().contains(tree_id_));
  ASSERT_TRUE(AreAllPendingUpdatesEmpty());
}

TEST_F(ReadAnythingAppModelTest, ClearPendingUpdates_DeletesPendingUpdates) {
  EXPECT_FALSE(model().pending_updates_for_testing().contains(tree_id_));

  // Create a couple of updates which add additional nodes to the tree.
  std::vector<int> child_ids = {2, 3, 4};
  ReadAnythingAppModel::Updates updates =
      test::CreateSimpleUpdateList(child_ids, tree_id_);

  ApplyAccessibilityUpdates(tree_id_, {std::move(updates[0])});
  EXPECT_FALSE(model().pending_updates_for_testing().contains(tree_id_));
  QueueAccessibilityUpdates(tree_id_, {std::move(updates[1])});
  EXPECT_EQ(1u, model().pending_updates_for_testing().at(tree_id_).size());
  QueueAccessibilityUpdates(tree_id_, {std::move(updates[2])});
  EXPECT_EQ(2u, model().pending_updates_for_testing().at(tree_id_).size());

  // Clearing the pending updates correctly deletes the pending updates.
  model().ClearPendingUpdates();
  ASSERT_TRUE(AreAllPendingUpdatesEmpty());
}

TEST_F(ReadAnythingAppModelTest, ChangeActiveTreeWithPendingUpdates_UnknownID) {
  EXPECT_FALSE(model().pending_updates_for_testing().contains(tree_id_));
  ASSERT_TRUE(AreAllPendingUpdatesEmpty());

  // Create a couple of updates which add additional nodes to the tree.
  std::vector<int> child_ids = {2, 3, 4};
  ReadAnythingAppModel::Updates updates =
      test::CreateSimpleUpdateList(child_ids, tree_id_);
  const size_t num_pending_updates = updates.size();

  // Create an update which has no tree id.
  ui::AXTreeUpdate update;
  ui::AXNodeData node = test::GenericContainerNode(/* id= */ 1);
  update.nodes = {std::move(node)};
  updates.push_back(std::move(update));

  // Add the updates.
  ApplyAccessibilityUpdates(tree_id_, {std::move(updates[0])});
  updates.erase(updates.begin());
  EXPECT_FALSE(model().pending_updates_for_testing().contains(tree_id_));
  ASSERT_TRUE(AreAllPendingUpdatesEmpty());
  QueueAccessibilityUpdates(tree_id_, std::move(updates));

  size_t actual_pending_updates = 0;
  std::vector<ReadAnythingAppModel::Updates> pending_updates_for_testing =
      model().pending_updates_for_testing().at(tree_id_);
  // Get the sum of the size of all the updates in the list.
  for (ReadAnythingAppModel::Updates& pending_update :
       pending_updates_for_testing) {
    actual_pending_updates += pending_update.size();
  }
  EXPECT_EQ(num_pending_updates, actual_pending_updates);

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
  update.nodes = {std::move(parent_node), std::move(node1), std::move(node2)};

  // This update changes the structure of the tree. When the controller receives
  // it in AccessibilityEventReceived, it will re-distill the tree.
  ApplyAccessibilityUpdates(tree_id_, {std::move(update)});
  ProcessDisplayNodes({3, 4});
  EXPECT_TRUE(model().display_node_ids().contains(1));
  EXPECT_FALSE(model().display_node_ids().contains(2));
  EXPECT_TRUE(model().display_node_ids().contains(3));
  EXPECT_TRUE(model().display_node_ids().contains(4));
  EXPECT_TRUE(model().display_node_ids().contains(5));
  EXPECT_TRUE(model().display_node_ids().contains(6));
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
  ApplyAccessibilityUpdates(tree_id_, {std::move(update)});
  ProcessDisplayNodes({2, 3, 4});
  EXPECT_TRUE(model().display_node_ids().contains(1));
  EXPECT_TRUE(model().display_node_ids().contains(2));
  EXPECT_FALSE(model().display_node_ids().contains(3));
  EXPECT_FALSE(model().display_node_ids().contains(4));
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
  ApplyAccessibilityUpdates(tree_id_, {update});
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
  ApplyAccessibilityUpdates(tree_id_, {update});
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
  ApplyAccessibilityUpdates(tree_id_, {std::move(update)});
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

  ApplyAccessibilityUpdates(tree_id_, {std::move(update)});
  model().PostProcessSelection();
  EXPECT_TRUE(model().selection_node_ids().contains(1));
  EXPECT_TRUE(model().selection_node_ids().contains(2));
  EXPECT_TRUE(model().selection_node_ids().contains(3));
  EXPECT_TRUE(model().selection_node_ids().contains(4));
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
  ApplyAccessibilityUpdates(tree_id_, {std::move(update)});
  model().PostProcessSelection();
  EXPECT_TRUE(model().selection_node_ids().contains(1));
  EXPECT_TRUE(model().selection_node_ids().contains(2));
  EXPECT_TRUE(model().selection_node_ids().contains(3));
  EXPECT_TRUE(model().selection_node_ids().contains(4));
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

  ApplyAccessibilityUpdates(tree_id_, {std::move(update)});
  model().PostProcessSelection();
  EXPECT_FALSE(model().display_node_ids().contains(1));
  EXPECT_FALSE(model().selection_node_ids().contains(2));
  EXPECT_FALSE(model().selection_node_ids().contains(3));
  EXPECT_FALSE(model().selection_node_ids().contains(4));
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
  update.nodes = {std::move(root), std::move(node1), std::move(node2)};

  ApplyAccessibilityUpdates(tree_id_, {std::move(update)});
  ProcessDisplayNodes({3, 4});
  model().set_screen2x_distiller_running(true);

  // Assert initial state before model().Resetting.
  ASSERT_TRUE(model().screen2x_distiller_running());

  ASSERT_TRUE(model().display_node_ids().contains(1));
  ASSERT_TRUE(model().display_node_ids().contains(3));
  ASSERT_TRUE(model().display_node_ids().contains(4));
  ASSERT_TRUE(model().display_node_ids().contains(5));
  ASSERT_TRUE(model().display_node_ids().contains(6));

  model().Reset({1, 2});

  // Assert model().Reset state.
  ASSERT_FALSE(model().screen2x_distiller_running());

  ASSERT_TRUE(std::ranges::contains(model().content_node_ids(), 1));
  ASSERT_TRUE(std::ranges::contains(model().content_node_ids(), 2));

  ASSERT_FALSE(model().display_node_ids().contains(1));
  ASSERT_FALSE(model().display_node_ids().contains(3));
  ASSERT_FALSE(model().display_node_ids().contains(4));
  ASSERT_FALSE(model().display_node_ids().contains(5));
  ASSERT_FALSE(model().display_node_ids().contains(6));

  // Calling model().Reset with different content nodes updates the content
  // nodes.
  model().Reset({5, 4});
  ASSERT_FALSE(std::ranges::contains(model().content_node_ids(), 1));
  ASSERT_FALSE(std::ranges::contains(model().content_node_ids(), 2));
  ASSERT_TRUE(std::ranges::contains(model().content_node_ids(), 5));
  ASSERT_TRUE(std::ranges::contains(model().content_node_ids(), 4));
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
  ApplyAccessibilityUpdates(tree_id_, {std::move(update)});
  model().PostProcessSelection();

  // Assert initial selection state.
  ASSERT_TRUE(model().selection_node_ids().contains(1));
  ASSERT_TRUE(model().selection_node_ids().contains(2));
  ASSERT_TRUE(model().selection_node_ids().contains(3));

  ASSERT_TRUE(model().has_selection());

  ASSERT_NE(model().start_offset(), -1);
  ASSERT_NE(model().end_offset(), -1);

  ASSERT_NE(model().start_node_id(), ui::kInvalidAXNodeID);
  ASSERT_NE(model().end_node_id(), ui::kInvalidAXNodeID);

  model().Reset({1, 2});

  // Assert model().Reset selection state.
  ASSERT_FALSE(model().selection_node_ids().contains(1));
  ASSERT_FALSE(model().selection_node_ids().contains(2));
  ASSERT_FALSE(model().selection_node_ids().contains(3));

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
  ApplyAccessibilityUpdates(tree_id_, {std::move(update)});
  ASSERT_TRUE(model().requires_post_process_selection());
  model().PostProcessSelection();

  ASSERT_FALSE(model().requires_post_process_selection());
  ASSERT_TRUE(model().has_selection());

  ASSERT_TRUE(model().selection_node_ids().contains(1));
  ASSERT_TRUE(model().selection_node_ids().contains(2));
  ASSERT_TRUE(model().selection_node_ids().contains(3));

  ASSERT_EQ(model().start_offset(), 0);
  ASSERT_EQ(model().end_offset(), 0);

  ASSERT_EQ(model().start_node_id(), 2);
  ASSERT_EQ(model().end_node_id(), 3);
}

TEST_F(ReadAnythingAppModelTest,
       PostProcessSelectionFromReadingMode_DoesNotDraw) {
  // Initial state.
  ui::AXTreeUpdate update;
  test::SetUpdateTreeID(&update, tree_id_);
  update.tree_data.sel_anchor_object_id = 2;
  update.tree_data.sel_focus_object_id = 3;
  update.tree_data.sel_anchor_offset = 0;
  update.tree_data.sel_focus_offset = 0;
  update.tree_data.sel_is_backward = false;
  ApplyAccessibilityUpdates(tree_id_, {std::move(update)});
  ProcessDisplayNodes({2, 3});
  model().increment_selections_from_reading_mode();

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
  ApplyAccessibilityUpdates(tree_id_, {std::move(update)});

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
  ApplyAccessibilityUpdates(tree_id_, {std::move(update)});

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
  ApplyAccessibilityUpdates(tree_id_, {std::move(update)});

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
  ApplyAccessibilityUpdates(tree_id_, {std::move(update)});

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
  ApplyAccessibilityUpdates(tree_id_, {std::move(update1)});
  model().PostProcessSelection();

  // Empty selection inside display nodes.
  ui::AXTreeUpdate update2;
  test::SetUpdateTreeID(&update2, tree_id_);
  update2.tree_data.sel_anchor_object_id = 3;
  update2.tree_data.sel_focus_object_id = 3;
  update2.tree_data.sel_anchor_offset = 2;
  update2.tree_data.sel_focus_offset = 2;
  update2.tree_data.sel_is_backward = false;
  ApplyAccessibilityUpdates(tree_id_, {std::move(update2)});

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
  ApplyAccessibilityUpdates(tree_id_, {std::move(update1)});
  model().PostProcessSelection();

  // Different empty selection inside display nodes.
  ui::AXTreeUpdate update2;
  test::SetUpdateTreeID(&update2, tree_id_);
  update2.tree_data.sel_anchor_object_id = 3;
  update2.tree_data.sel_focus_object_id = 3;
  update2.tree_data.sel_anchor_offset = 2;
  update2.tree_data.sel_focus_offset = 2;
  update2.tree_data.sel_is_backward = false;
  ApplyAccessibilityUpdates(tree_id_, {std::move(update2)});

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
  ApplyAccessibilityUpdates(tree_id_, {std::move(update1)});
  model().PostProcessSelection();

  // Non-empty selection inside display nodes.
  ui::AXTreeUpdate update2;
  test::SetUpdateTreeID(&update2, tree_id_);
  update2.tree_data.sel_anchor_object_id = 2;
  update2.tree_data.sel_focus_object_id = 2;
  update2.tree_data.sel_anchor_offset = 0;
  update2.tree_data.sel_focus_offset = 5;
  update2.tree_data.sel_is_backward = false;
  ApplyAccessibilityUpdates(tree_id_, {std::move(update2)});

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
  ApplyAccessibilityUpdates(tree_id_, {std::move(update1)});
  model().PostProcessSelection();

  // Different non-empty selection inside display nodes.
  ui::AXTreeUpdate update2;
  test::SetUpdateTreeID(&update2, tree_id_);
  update2.tree_data.sel_anchor_object_id = 2;
  update2.tree_data.sel_focus_object_id = 3;
  update2.tree_data.sel_anchor_offset = 0;
  update2.tree_data.sel_focus_offset = 5;
  update2.tree_data.sel_is_backward = false;
  ApplyAccessibilityUpdates(tree_id_, {std::move(update2)});

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
  ApplyAccessibilityUpdates(tree_id_, {std::move(update1)});
  model().PostProcessSelection();

  // Empty selection outside display nodes.
  ui::AXTreeUpdate update2;
  test::SetUpdateTreeID(&update2, tree_id_);
  update2.tree_data.sel_anchor_object_id = 4;
  update2.tree_data.sel_focus_object_id = 4;
  update2.tree_data.sel_anchor_offset = 2;
  update2.tree_data.sel_focus_offset = 2;
  update2.tree_data.sel_is_backward = false;
  ApplyAccessibilityUpdates(tree_id_, {std::move(update2)});

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
  ApplyAccessibilityUpdates(tree_id_, {std::move(update1)});
  model().PostProcessSelection();

  // Different empty selection outside display nodes.
  ui::AXTreeUpdate update2;
  test::SetUpdateTreeID(&update2, tree_id_);
  update2.tree_data.sel_anchor_object_id = 4;
  update2.tree_data.sel_focus_object_id = 4;
  update2.tree_data.sel_anchor_offset = 2;
  update2.tree_data.sel_focus_offset = 2;
  update2.tree_data.sel_is_backward = false;
  ApplyAccessibilityUpdates(tree_id_, {std::move(update2)});

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
  ApplyAccessibilityUpdates(tree_id_, {std::move(update1)});
  model().PostProcessSelection();

  // Non-empty selection outside display nodes.
  ui::AXTreeUpdate update2;
  test::SetUpdateTreeID(&update2, tree_id_);
  update2.tree_data.sel_anchor_object_id = 4;
  update2.tree_data.sel_focus_object_id = 4;
  update2.tree_data.sel_anchor_offset = 0;
  update2.tree_data.sel_focus_offset = 5;
  update2.tree_data.sel_is_backward = false;
  ApplyAccessibilityUpdates(tree_id_, {std::move(update2)});

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
  ApplyAccessibilityUpdates(tree_id_, {std::move(update1)});
  model().PostProcessSelection();

  // Different non-empty selection outside display nodes.
  ui::AXTreeUpdate update2;
  test::SetUpdateTreeID(&update2, tree_id_);
  update2.tree_data.sel_anchor_object_id = 4;
  update2.tree_data.sel_focus_object_id = 4;
  update2.tree_data.sel_anchor_offset = 0;
  update2.tree_data.sel_focus_offset = 5;
  update2.tree_data.sel_is_backward = false;
  ApplyAccessibilityUpdates(tree_id_, {std::move(update2)});

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
  ApplyAccessibilityUpdates(tree_id_, {std::move(update1)});
  model().PostProcessSelection();

  // Empty selection inside display nodes.
  ui::AXTreeUpdate update2;
  test::SetUpdateTreeID(&update2, tree_id_);
  update2.tree_data.sel_anchor_object_id = 2;
  update2.tree_data.sel_focus_object_id = 2;
  update2.tree_data.sel_anchor_offset = 2;
  update2.tree_data.sel_focus_offset = 2;
  update2.tree_data.sel_is_backward = false;
  ApplyAccessibilityUpdates(tree_id_, {std::move(update2)});

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
  ApplyAccessibilityUpdates(tree_id_, {std::move(update1)});
  model().PostProcessSelection();

  // Empty selection inside display nodes.
  ui::AXTreeUpdate update2;
  test::SetUpdateTreeID(&update2, tree_id_);
  update2.tree_data.sel_anchor_object_id = 2;
  update2.tree_data.sel_focus_object_id = 2;
  update2.tree_data.sel_anchor_offset = 2;
  update2.tree_data.sel_focus_offset = 2;
  update2.tree_data.sel_is_backward = false;
  ApplyAccessibilityUpdates(tree_id_, {std::move(update2)});

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
  ApplyAccessibilityUpdates(tree_id_, {std::move(update1)});
  model().PostProcessSelection();

  // Non-empty selection inside display nodes.
  ui::AXTreeUpdate update2;
  test::SetUpdateTreeID(&update2, tree_id_);
  update2.tree_data.sel_anchor_object_id = 2;
  update2.tree_data.sel_focus_object_id = 3;
  update2.tree_data.sel_anchor_offset = 0;
  update2.tree_data.sel_focus_offset = 5;
  update2.tree_data.sel_is_backward = false;
  ApplyAccessibilityUpdates(tree_id_, {std::move(update2)});

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
  ApplyAccessibilityUpdates(tree_id_, {std::move(update1)});
  model().PostProcessSelection();

  // Non-empty selection inside display nodes.
  ui::AXTreeUpdate update2;
  test::SetUpdateTreeID(&update2, tree_id_);
  update2.tree_data.sel_anchor_object_id = 2;
  update2.tree_data.sel_focus_object_id = 2;
  update2.tree_data.sel_anchor_offset = 0;
  update2.tree_data.sel_focus_offset = 5;
  update2.tree_data.sel_is_backward = false;
  ApplyAccessibilityUpdates(tree_id_, {std::move(update2)});

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
  ApplyAccessibilityUpdates(tree_id_, {std::move(update1)});
  model().PostProcessSelection();

  // Empty selection outside display nodes.
  ui::AXTreeUpdate update2;
  test::SetUpdateTreeID(&update2, tree_id_);
  update2.tree_data.sel_anchor_object_id = 4;
  update2.tree_data.sel_focus_object_id = 4;
  update2.tree_data.sel_anchor_offset = 2;
  update2.tree_data.sel_focus_offset = 2;
  update2.tree_data.sel_is_backward = false;
  ApplyAccessibilityUpdates(tree_id_, {std::move(update2)});

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
  ApplyAccessibilityUpdates(tree_id_, {std::move(update1)});
  model().PostProcessSelection();

  // Empty selection outside display nodes.
  ui::AXTreeUpdate update2;
  test::SetUpdateTreeID(&update2, tree_id_);
  update2.tree_data.sel_anchor_object_id = 4;
  update2.tree_data.sel_focus_object_id = 4;
  update2.tree_data.sel_anchor_offset = 2;
  update2.tree_data.sel_focus_offset = 2;
  update2.tree_data.sel_is_backward = false;
  ApplyAccessibilityUpdates(tree_id_, {std::move(update2)});

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
  ApplyAccessibilityUpdates(tree_id_, {std::move(update1)});
  model().PostProcessSelection();

  // Non-empty selection outside display nodes.
  ui::AXTreeUpdate update2;
  test::SetUpdateTreeID(&update2, tree_id_);
  update2.tree_data.sel_anchor_object_id = 4;
  update2.tree_data.sel_focus_object_id = 4;
  update2.tree_data.sel_anchor_offset = 0;
  update2.tree_data.sel_focus_offset = 5;
  update2.tree_data.sel_is_backward = false;
  ApplyAccessibilityUpdates(tree_id_, {std::move(update2)});

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
  ApplyAccessibilityUpdates(tree_id_, {std::move(update1)});
  model().PostProcessSelection();

  // Non-empty selection outside display nodes.
  ui::AXTreeUpdate update2;
  test::SetUpdateTreeID(&update2, tree_id_);
  update2.tree_data.sel_anchor_object_id = 4;
  update2.tree_data.sel_focus_object_id = 4;
  update2.tree_data.sel_anchor_offset = 0;
  update2.tree_data.sel_focus_offset = 5;
  update2.tree_data.sel_is_backward = false;
  ApplyAccessibilityUpdates(tree_id_, {std::move(update2)});

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
  update.nodes = {std::move(parent_node),
                  std::move(static_text_node1),
                  std::move(static_text_node2),
                  std::move(generic_container_node),
                  std::move(static_text_child_node1),
                  std::move(static_text_child_node2)};

  ApplyAccessibilityUpdates(tree_id_, {update});

  update.tree_data.sel_anchor_object_id = 2;
  update.tree_data.sel_focus_object_id = 5;
  update.tree_data.sel_anchor_offset = 0;
  update.tree_data.sel_focus_offset = 0;
  update.tree_data.sel_is_backward = false;
  ApplyAccessibilityUpdates(tree_id_, {std::move(update)});
  model().PostProcessSelection();

  ASSERT_TRUE(model().has_selection());
  ASSERT_EQ(model().start_node_id(), 2);
  ASSERT_EQ(model().end_node_id(), 5);

  // 1 and 3 are ancestors, so they are included as selection nodes..
  ASSERT_TRUE(model().selection_node_ids().contains(1));
  ASSERT_TRUE(model().selection_node_ids().contains(3));

  ASSERT_TRUE(model().selection_node_ids().contains(5));
  ASSERT_TRUE(model().selection_node_ids().contains(6));

  // Even though 3 is a generic container with more than one child, its
  // sibling nodes are included in the selection because the start node
  // includes it.
  ASSERT_TRUE(model().selection_node_ids().contains(2));
  ASSERT_TRUE(model().selection_node_ids().contains(3));
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
  update.nodes = {std::move(root), std::move(static_text_node),
                  std::move(link_node), std::move(inline_block_node)};

  ApplyAccessibilityUpdates(tree_id_, {update});

  update.tree_data.sel_anchor_object_id = 4;
  update.tree_data.sel_focus_object_id = 4;
  update.tree_data.sel_anchor_offset = 0;
  update.tree_data.sel_focus_offset = 1;
  update.tree_data.sel_is_backward = false;
  ApplyAccessibilityUpdates(tree_id_, {std::move(update)});
  model().PostProcessSelection();

  ASSERT_TRUE(model().has_selection());
  ASSERT_EQ(model().start_node_id(), 4);
  ASSERT_EQ(model().end_node_id(), 4);

  ASSERT_TRUE(model().selection_node_ids().contains(1));
  ASSERT_FALSE(model().selection_node_ids().contains(2));
  ASSERT_TRUE(model().selection_node_ids().contains(3));
  ASSERT_TRUE(model().selection_node_ids().contains(4));
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
  update.nodes = {std::move(parent_node), std::move(static_text_node),
                  std::move(link_node), std::move(static_text_list_node)};

  ApplyAccessibilityUpdates(tree_id_, {update});

  update.tree_data.sel_anchor_object_id = 4;
  update.tree_data.sel_focus_object_id = 4;
  update.tree_data.sel_anchor_offset = 0;
  update.tree_data.sel_focus_offset = 1;
  update.tree_data.sel_is_backward = false;
  ApplyAccessibilityUpdates(tree_id_, {std::move(update)});
  model().PostProcessSelection();

  ASSERT_TRUE(model().has_selection());
  ASSERT_EQ(model().start_node_id(), 4);
  ASSERT_EQ(model().end_node_id(), 4);

  ASSERT_TRUE(model().selection_node_ids().contains(1));
  ASSERT_FALSE(model().selection_node_ids().contains(2));
  ASSERT_TRUE(model().selection_node_ids().contains(3));
  ASSERT_TRUE(model().selection_node_ids().contains(4));
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
  update.nodes = {std::move(parent_node), std::move(static_text_node),
                  std::move(generic_container_node), std::move(inline_node)};

  ApplyAccessibilityUpdates(tree_id_, {update});

  update.tree_data.sel_anchor_object_id = 4;
  update.tree_data.sel_focus_object_id = 4;
  update.tree_data.sel_anchor_offset = 0;
  update.tree_data.sel_focus_offset = 1;
  update.tree_data.sel_is_backward = true;
  ApplyAccessibilityUpdates(tree_id_, {std::move(update)});
  model().PostProcessSelection();

  ASSERT_TRUE(model().has_selection());
  ASSERT_EQ(model().start_node_id(), 4);
  ASSERT_EQ(model().end_node_id(), 4);

  ASSERT_TRUE(model().selection_node_ids().contains(1));
  ASSERT_FALSE(model().selection_node_ids().contains(2));
  ASSERT_TRUE(model().selection_node_ids().contains(3));
  ASSERT_TRUE(model().selection_node_ids().contains(4));
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
  update.nodes = {std::move(parent_node), std::move(static_text_node),
                  std::move(generic_container_node),
                  std::move(static_text_child_node1),
                  std::move(static_text_child_node2)};

  ApplyAccessibilityUpdates(tree_id_, {update});

  update.tree_data.sel_anchor_object_id = 4;
  update.tree_data.sel_focus_object_id = 5;
  update.tree_data.sel_anchor_offset = 0;
  update.tree_data.sel_focus_offset = 0;
  update.tree_data.sel_is_backward = false;
  ApplyAccessibilityUpdates(tree_id_, {std::move(update)});
  model().PostProcessSelection();

  ASSERT_TRUE(model().has_selection());
  ASSERT_EQ(model().start_node_id(), 4);
  ASSERT_EQ(model().end_node_id(), 5);

  // 1 and 3 are ancestors, so they are included as selection nodes..
  ASSERT_TRUE(model().selection_node_ids().contains(1));
  ASSERT_TRUE(model().selection_node_ids().contains(3));
  ASSERT_TRUE(model().selection_node_ids().contains(4));
  ASSERT_TRUE(model().selection_node_ids().contains(5));

  // Since 3 is a generic container with more than one child, its sibling nodes
  // are not included, so 2 is ignored.
  ASSERT_FALSE(model().selection_node_ids().contains(2));
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

TEST_F(ReadAnythingAppModelTest, BaseLanguageCode_ReturnsCorrectCode) {
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
                          "Andika", "Atkinson Hyperlegible Next"));
}

TEST_F(ReadAnythingAppModelTest,
       SupportedFonts_SetBaseLanguageCode_ReturnsExpectedDefaultFonts) {
  // Spanish
  model().SetBaseLanguageCode("es");
  EXPECT_THAT(model().supported_fonts(),
              ElementsAre("Poppins", "Sans-serif", "Serif", "Comic Neue",
                          "Lexend Deca", "EB Garamond", "STIX Two Text",
                          "Andika", "Atkinson Hyperlegible Next"));

  // Bulgarian
  model().SetBaseLanguageCode("bg");
  EXPECT_THAT(model().supported_fonts(),
              ElementsAre("Sans-serif", "Serif", "EB Garamond", "STIX Two Text",
                          "Andika"));

  // Hindi
  model().SetBaseLanguageCode("hi");
  EXPECT_THAT(model().supported_fonts(),
              ElementsAre("Poppins", "Sans-serif", "Serif"));

  // Welsh
  model().SetBaseLanguageCode("cy");
  EXPECT_THAT(model().supported_fonts(),
              ElementsAre("Sans-serif", "Serif", "Atkinson Hyperlegible Next"));
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
  initial_update.nodes = {std::move(pdf_root_node), std::move(embedded_node)};
  ApplyAccessibilityUpdates(tree_id_, {std::move(initial_update)});

  // Update with no new nodes added to the tree.
  ui::AXTreeUpdate update;
  test::SetUpdateTreeID(&update, tree_id_);
  update.root_id = 1;
  ui::AXNodeData node;
  node.id = 1;
  node.role = ax::mojom::Role::kPdfRoot;
  node.SetNameChecked("example.pdf");
  update.nodes = {std::move(node)};
  ApplyAccessibilityUpdates(tree_id_, {std::move(update)});
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
  update2.nodes = {std::move(static_text_node1),
                   std::move(updated_embedded_node),
                   std::move(static_text_node2)};

  ApplyAccessibilityUpdates(tree_id_, {std::move(update2)});
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
  initial_update.nodes = {std::move(node)};
  ApplyAccessibilityUpdates(tree_id_, {std::move(initial_update)});

  // Updates that don't create a new subtree, for example, a role change, should
  // not set requires_distillation_.
  ui::AXTreeUpdate update;
  test::SetUpdateTreeID(&update, tree_id_);
  ui::AXNodeData static_text_node = test::TextNode(/* id= */ 1);
  update.root_id = static_text_node.id;
  update.nodes = {std::move(static_text_node)};
  ApplyAccessibilityUpdates(tree_id_, {std::move(update)});
  ASSERT_FALSE(model().requires_distillation());
}

TEST_F(ReadAnythingAppModelTest, Expand_NodeDoesNotExist_Redistills) {
  ui::AXTreeUpdate initial_update;
  test::SetUpdateTreeID(&initial_update, tree_id_);
  static constexpr int kInitialId = 2;
  ui::AXNodeData initial_node = test::GenericContainerNode(kInitialId);
  initial_update.nodes = {std::move(initial_node)};
  ApplyAccessibilityUpdates(tree_id_, {std::move(initial_update)});
  model().Reset({kInitialId});

  EXPECT_FALSE(model().requires_distillation());
  EXPECT_FALSE(model().redraw_required());

  ui::AXTreeUpdate update;
  test::SetUpdateTreeID(&update, tree_id_);
  static constexpr int kExpandedId = 4;
  ui::AXNodeData updated_node = test::GenericContainerNode(kExpandedId);
  updated_node.AddState(ax::mojom::State::kExpanded);
  update.nodes = {std::move(updated_node)};
  ApplyAccessibilityUpdates(tree_id_, {std::move(update)});

  EXPECT_TRUE(model().requires_distillation());
  EXPECT_FALSE(model().redraw_required());
}

TEST_F(ReadAnythingAppModelTest, Expand_NodeDoesExist_Redraws) {
  ui::AXTreeUpdate initial_update;
  test::SetUpdateTreeID(&initial_update, tree_id_);
  static constexpr int kInitialId = 2;
  ui::AXNodeData initial_node = test::GenericContainerNode(kInitialId);
  initial_update.nodes = {initial_node};
  ApplyAccessibilityUpdates(tree_id_, {std::move(initial_update)});
  model().Reset({kInitialId});

  EXPECT_FALSE(model().requires_distillation());
  EXPECT_FALSE(model().redraw_required());

  ui::AXTreeUpdate update;
  test::SetUpdateTreeID(&update, tree_id_);
  initial_node.AddState(ax::mojom::State::kExpanded);
  update.nodes = {std::move(initial_node)};
  ApplyAccessibilityUpdates(tree_id_, {std::move(update)});

  EXPECT_FALSE(model().requires_distillation());
  EXPECT_TRUE(model().redraw_required());
}

TEST_F(ReadAnythingAppModelTest, Collapse_Redraws) {
  ui::AXTreeUpdate initial_update;
  test::SetUpdateTreeID(&initial_update, tree_id_);
  static constexpr int kInitialId = 2;
  ui::AXNodeData initial_node = test::GenericContainerNode(kInitialId);
  initial_node.AddState(ax::mojom::State::kExpanded);
  initial_update.nodes = {initial_node};
  ApplyAccessibilityUpdates(tree_id_, {std::move(initial_update)});
  model().Reset({kInitialId});

  ui::AXTreeUpdate update;
  test::SetUpdateTreeID(&update, tree_id_);
  initial_node.AddState(ax::mojom::State::kCollapsed);
  initial_node.RemoveState(ax::mojom::State::kExpanded);
  update.nodes = {std::move(initial_node)};
  ApplyAccessibilityUpdates(tree_id_, {std::move(update)});

  EXPECT_TRUE(model().redraw_required());
  EXPECT_FALSE(model().requires_post_process_selection());
  EXPECT_FALSE(model().has_selection());
  EXPECT_TRUE(model().selection_node_ids().empty());
}

TEST_F(ReadAnythingAppModelTest, ContentEditableValueChanged_ResetsDrawTimer) {
  ui::AXTreeUpdate update;
  test::SetUpdateTreeID(&update, tree_id_);
  ui::AXNodeData node1;
  static constexpr int kId = 1;
  node1.id = kId;
  update.nodes = {std::move(node1)};
  ReadAnythingAppModel::Updates updates = {std::move(update)};

  ui::AXEvent event;
  event.id = kId;
  event.event_type = ax::mojom::Event::kValueChanged;
  event.event_from = ax::mojom::EventFrom::kUser;
  ui::AXEventIntent eventIntent;
  event.event_intents = {std::move(eventIntent)};
  std::vector<ui::AXEvent> events = {std::move(event)};
  // This update changes the structure of the tree. When the controller receives
  // it in AccessibilityEventReceived, it will re-distill the tree.
  model().ApplyAccessibilityUpdates(tree_id_, updates, events);
  ASSERT_TRUE(model().reset_draw_timer());
}

TEST_F(ReadAnythingAppModelTest,
       ContentEditableValueChanged_FromPage_DoesNotResetDrawTimer) {
  ui::AXTreeUpdate update;
  test::SetUpdateTreeID(&update, tree_id_);
  ui::AXNodeData node1;
  static constexpr int kId = 1;
  node1.id = kId;
  update.nodes = {std::move(node1)};
  ReadAnythingAppModel::Updates updates = {std::move(update)};

  ui::AXEvent event;
  event.id = kId;
  event.event_type = ax::mojom::Event::kValueChanged;
  event.event_from = ax::mojom::EventFrom::kPage;
  ui::AXEventIntent eventIntent;
  event.event_intents = {std::move(eventIntent)};
  std::vector<ui::AXEvent> events = {std::move(event)};
  // This update changes the structure of the tree. When the controller receives
  // it in AccessibilityEventReceived, it will re-distill the tree.
  model().ApplyAccessibilityUpdates(tree_id_, updates, events);
  ASSERT_FALSE(model().reset_draw_timer());
}

TEST_F(ReadAnythingAppModelTest,
       ContentEditableValueChanged_NoIntents_DoesNotResetDrawTimer) {
  ui::AXTreeUpdate update;
  test::SetUpdateTreeID(&update, tree_id_);
  ui::AXNodeData node1;
  static constexpr int kId = 1;
  node1.id = kId;
  update.nodes = {std::move(node1)};
  ReadAnythingAppModel::Updates updates = {std::move(update)};

  ui::AXEvent event;
  event.id = kId;
  event.event_type = ax::mojom::Event::kValueChanged;
  event.event_from = ax::mojom::EventFrom::kUser;
  std::vector<ui::AXEvent> events = {std::move(event)};
  // This update changes the structure of the tree. When the controller receives
  // it in AccessibilityEventReceived, it will re-distill the tree.
  model().ApplyAccessibilityUpdates(tree_id_, updates, events);
  ASSERT_FALSE(model().reset_draw_timer());
}

TEST_F(ReadAnythingAppModelTest, SetUkmSourceId_TreeExists) {
  ui::AXTreeID tree_id = ui::AXTreeID::CreateNewAXTreeID();
  ui::AXTreeUpdate update;
  test::SetUpdateTreeID(&update, tree_id);
  ui::AXNodeData node1;
  static constexpr int kId = 1;
  node1.id = kId;
  update.root_id = node1.id;
  update.nodes = {std::move(node1)};

  ukm::SourceId source_id = ukm::AssignNewSourceId();

  // The UKM source should be invalid before the tree is made active.
  ApplyAccessibilityUpdates(tree_id, {std::move(update)});
  EXPECT_EQ(model().GetUkmSourceId(), ukm::kInvalidSourceId);

  // After the tree is made active, the UKM source should be valid.
  model().SetActiveTreeId(tree_id);
  model().SetUkmSourceIdForTree(tree_id, source_id);
  EXPECT_EQ(model().GetUkmSourceId(), source_id);
}

TEST_F(ReadAnythingAppModelTest, SetUkmSourceId_TreeDoesNotExistInitially) {
  ui::AXTreeID tree_id = ui::AXTreeID::CreateNewAXTreeID();
  ui::AXTreeUpdate update;
  test::SetUpdateTreeID(&update, tree_id);
  ui::AXNodeData node1;
  static constexpr int kId = 1;
  node1.id = kId;
  update.root_id = node1.id;
  update.nodes = {std::move(node1)};

  ukm::SourceId source_id = ukm::AssignNewSourceId();

  // The UKM source should be invalid when the tree is made active but before
  // a representation of it is actually stored.
  model().SetActiveTreeId(tree_id);
  model().SetUkmSourceIdForTree(tree_id, source_id);
  EXPECT_EQ(model().GetUkmSourceId(), ukm::kInvalidSourceId);

  // The UKM source should be valid once an accessibility event is received for
  // the active tree.
  ApplyAccessibilityUpdates(tree_id, {std::move(update)});
  EXPECT_EQ(model().GetUkmSourceId(), source_id);
}

TEST_F(ReadAnythingAppModelTest, SelectionNodesContainedInDistilledContent) {
  // content_node_ids = {3, 4}.
  // display_node_ids will be computed from this, and will include ancestors,
  // so {1, 3, 4}.
  ProcessDisplayNodes({3, 4});

  // Selection is outside content/display nodes: {2}.
  // This will populate selection_node_ids with {1, 2}.
  ui::AXTreeUpdate update1;
  test::SetUpdateTreeID(&update1, tree_id_);
  update1.tree_data.sel_anchor_object_id = 2;
  update1.tree_data.sel_focus_object_id = 2;
  update1.tree_data.sel_anchor_offset = 0;
  update1.tree_data.sel_focus_offset = 1;
  update1.tree_data.sel_is_backward = false;
  ApplyAccessibilityUpdates(tree_id_, {std::move(update1)});
  model().PostProcessSelection();

  // selection_node_ids_ is {1, 2}. content_node_ids_ is {3, 4}. The new method
  // should return false.
  EXPECT_FALSE(model().SelectionNodesContainedInDistilledContent());

  // Now, let's test the true case.
  // content_node_ids = {1, 2, 3, 4}.
  model().Reset({1, 2, 3, 4});

  // Selection is {2}.
  ui::AXTreeUpdate update2;
  test::SetUpdateTreeID(&update2, tree_id_);
  update2.tree_data.sel_anchor_object_id = 2;
  update2.tree_data.sel_focus_object_id = 2;
  update2.tree_data.sel_anchor_offset = 0;
  update2.tree_data.sel_focus_offset = 1;
  update2.tree_data.sel_is_backward = false;
  ApplyAccessibilityUpdates(tree_id_, {std::move(update2)});
  model().PostProcessSelection();

  // selection_node_ids_ will be {1, 2}. content_node_ids_ is {1, 2, 3, 4}.
  // The new method should return true.
  EXPECT_TRUE(model().SelectionNodesContainedInDistilledContent());
}

TEST_F(ReadAnythingAppModelTest,
       AccessibilityEventReceived_ChildTreeFound_RequiresDistillation) {
  // Create a parent tree and a child tree.
  ui::AXTreeID parent_tree_id = ui::AXTreeID::CreateNewAXTreeID();
  ui::AXTreeUpdate parent_update;
  test::SetUpdateTreeID(&parent_update, parent_tree_id);
  ui::AXNodeData root_node;
  root_node.id = 1;
  ui::AXNodeData child_host_node;
  child_host_node.id = 2;
  root_node.child_ids = {child_host_node.id};

  ui::AXTreeID child_tree_id = ui::AXTreeID::CreateNewAXTreeID();
  child_host_node.AddChildTreeId(child_tree_id);
  parent_update.root_id = root_node.id;
  parent_update.nodes = {root_node, child_host_node};

  ui::AXNodeData child_root;
  child_root.id = 10;

  // Send an update for the child.
  ui::AXTreeUpdate early_child_update;
  test::SetUpdateTreeID(&early_child_update, child_tree_id);
  early_child_update.root_id = child_root.id;
  early_child_update.nodes = {child_root};
  early_child_update.tree_data.parent_tree_id = parent_tree_id;
  ApplyAccessibilityUpdates(child_tree_id, {early_child_update});

  // Send event for parent tree to create it in the model.
  ApplyAccessibilityUpdates(parent_tree_id, {parent_update});

  // Set parent tree as active tree.
  model().SetRootTreeId(parent_tree_id);
  EXPECT_EQ(model().active_tree_id(), parent_tree_id);

  // Enable child tree usage. This will populate child_tree_ids_.
  model().AllowChildTreeForActiveTree(true);

  // Create an update for the child tree.
  ui::AXTreeUpdate child_update;
  test::SetUpdateTreeID(&child_update, child_tree_id);
  child_update.root_id = child_root.id;
  child_update.nodes = {child_root};
  child_update.tree_data.parent_tree_id = parent_tree_id;

  // Send event for child tree.
  ApplyAccessibilityUpdates(child_tree_id, {child_update});

  // Assert requires_distillation is true and the active tree has changed.
  EXPECT_TRUE(model().requires_distillation());
  EXPECT_EQ(model().active_tree_id(), child_tree_id);
}

class ReadAnythingAppModelReadabilityTest : public ReadAnythingAppModelTest {
 public:
  ReadAnythingAppModelReadabilityTest() {
    scoped_feature_list_.InitWithFeatures(
        {features::kReadAnythingWithReadability,
         features::kReadAnythingWithReadabilityAllowLinks},
        {});
  }
  ~ReadAnythingAppModelReadabilityTest() override = default;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(ReadAnythingAppModelReadabilityTest,
       GetAXTreeAnchors_ExtractsBasicLink) {
  std::string url = "https://www.google.com";
  std::string link_text = "Ir a Google";

  ui::AXNodeData link_node;
  link_node.id = 2;
  link_node.role = ax::mojom::Role::kLink;
  link_node.SetName(link_text);
  link_node.AddStringAttribute(ax::mojom::StringAttribute::kUrl, url);
  link_node.AddStringAttribute(ax::mojom::StringAttribute::kHtmlId,
                               "link-id-1");
  link_node.AddStringAttribute(ax::mojom::StringAttribute::kLinkTarget,
                               "_blank");

  ui::AXNodeData root;
  root.id = 1;
  root.role = ax::mojom::Role::kRootWebArea;
  root.child_ids = {link_node.id};

  ui::AXTreeUpdate update;
  test::SetUpdateTreeID(&update, tree_id_);
  update.root_id = root.id;
  update.nodes = {std::move(root), std::move(link_node)};

  ApplyAccessibilityUpdates(tree_id_, {std::move(update)});
  model().SetActiveTreeId(tree_id_);

  model().set_should_extract_anchors_from_tree_for_readability(true);
  model().ProcessAXTreeAnchors();
  const auto& result = model().ax_tree_anchors();

  // Validates that only one link is processed
  ASSERT_EQ(1u, result.size());
  ASSERT_TRUE(result.count(url));

  const auto& links = result.at(url);
  // Validates a single link is mapped to the given URL
  ASSERT_EQ(1u, links.size());
  EXPECT_EQ(2, links[0].id);
  EXPECT_EQ(link_text, links[0].name);
  EXPECT_EQ("link-id-1", links[0].html_id);
  EXPECT_EQ("_blank", links[0].target);
}

TEST_F(ReadAnythingAppModelReadabilityTest, GetAXTreeAnchors_MultipleLinks) {
  std::string google_url = "https://www.google.com";
  std::string wikipedia_url = "https://www.wikipedia.org";

  ui::AXNodeData text_prev_1;
  text_prev_1.id = 2;
  text_prev_1.role = ax::mojom::Role::kStaticText;
  text_prev_1.SetName("Visit ");

  ui::AXNodeData link_google_1;
  link_google_1.id = 3;
  link_google_1.role = ax::mojom::Role::kLink;
  link_google_1.SetName("Google Homepage");
  link_google_1.AddStringAttribute(ax::mojom::StringAttribute::kUrl,
                                   google_url);
  link_google_1.AddStringAttribute(ax::mojom::StringAttribute::kHtmlId, "g1");

  ui::AXNodeData text_next_1;
  text_next_1.id = 4;
  text_next_1.role = ax::mojom::Role::kStaticText;
  text_next_1.SetName(" now.");

  ui::AXNodeData paragraph_1;
  paragraph_1.id = 10;
  paragraph_1.role = ax::mojom::Role::kParagraph;
  paragraph_1.child_ids = {text_prev_1.id, link_google_1.id, text_next_1.id};

  // Setup 2nd Google Anchor
  ui::AXNodeData link_google_2;
  link_google_2.id = 5;
  link_google_2.role = ax::mojom::Role::kLink;
  link_google_2.SetName("Google Footer");
  link_google_2.AddStringAttribute(ax::mojom::StringAttribute::kUrl,
                                   google_url);
  link_google_2.AddStringAttribute(ax::mojom::StringAttribute::kHtmlId, "g2");

  ui::AXNodeData paragraph_2;
  paragraph_2.id = 11;
  paragraph_2.role = ax::mojom::Role::kParagraph;
  paragraph_2.child_ids = {link_google_2.id};

  // Setup Wikipedia anchor
  ui::AXNodeData link_wiki;
  link_wiki.id = 6;
  link_wiki.role = ax::mojom::Role::kLink;
  link_wiki.SetName("Wiki");
  link_wiki.AddStringAttribute(ax::mojom::StringAttribute::kUrl, wikipedia_url);

  ui::AXNodeData text_next_wiki;
  text_next_wiki.id = 7;
  text_next_wiki.role = ax::mojom::Role::kStaticText;
  text_next_wiki.SetName(" is free.");

  ui::AXNodeData paragraph_3;
  paragraph_3.id = 12;
  paragraph_3.role = ax::mojom::Role::kParagraph;
  paragraph_3.child_ids = {link_wiki.id, text_next_wiki.id};

  ui::AXNodeData root;
  root.id = 1;
  root.role = ax::mojom::Role::kRootWebArea;
  root.child_ids = {paragraph_1.id, paragraph_2.id, paragraph_3.id};

  ui::AXTreeUpdate update;
  test::SetUpdateTreeID(&update, tree_id_);
  update.root_id = root.id;
  update.nodes = {std::move(root),          std::move(paragraph_1),
                  std::move(text_prev_1),   std::move(link_google_1),
                  std::move(text_next_1),   std::move(paragraph_2),
                  std::move(link_google_2), std::move(paragraph_3),
                  std::move(link_wiki),     std::move(text_next_wiki)};

  ApplyAccessibilityUpdates(tree_id_, {std::move(update)});
  model().SetActiveTreeId(tree_id_);

  model().set_should_extract_anchors_from_tree_for_readability(true);
  model().ProcessAXTreeAnchors();

  auto result = model().ax_tree_anchors();
  // Validate that there are two links in the dictionary
  ASSERT_EQ(2u, result.size());
  ASSERT_TRUE(result.count(google_url));
  ASSERT_TRUE(result.count(wikipedia_url));

  auto& google_links = result[google_url];
  ASSERT_EQ(2u, google_links.size());
  std::sort(google_links.begin(), google_links.end(),
            [](const auto& a, const auto& b) { return a.id < b.id; });

  // Validate first Google anchor
  EXPECT_EQ(3, google_links[0].id);
  EXPECT_EQ("Google Homepage", google_links[0].name);
  EXPECT_EQ("Visit ", google_links[0].text_before);
  EXPECT_EQ(" now.", google_links[0].text_after);
  EXPECT_EQ("g1", google_links[0].html_id);

  // Validate second Google anchor
  EXPECT_EQ(5, google_links[1].id);
  EXPECT_EQ("Google Footer", google_links[1].name);
  EXPECT_TRUE(google_links[1].text_before.empty());
  EXPECT_TRUE(google_links[1].text_after.empty());
  EXPECT_EQ("g2", google_links[1].html_id);

  // Validate Wiki anchor
  const auto& wiki_links = result[wikipedia_url];
  ASSERT_EQ(1u, wiki_links.size());
  EXPECT_EQ(6, wiki_links[0].id);
  EXPECT_EQ("Wiki", wiki_links[0].name);
  EXPECT_TRUE(wiki_links[0].text_before.empty());
  EXPECT_EQ(" is free.", wiki_links[0].text_after);
}

TEST_F(ReadAnythingAppModelReadabilityTest,
       GetAXTreeAnchors_IgnoresInvalidLinks) {
  std::string js_url = "javascript:alert(1)";
  ui::AXNodeData js_link_node;
  js_link_node.id = 2;
  js_link_node.role = ax::mojom::Role::kLink;
  js_link_node.SetName("Click me for XSS");
  js_link_node.AddStringAttribute(ax::mojom::StringAttribute::kUrl, js_url);

  std::string data_url = "data:text/html,<b>Hi</b>";
  ui::AXNodeData data_link_node;
  data_link_node.id = 3;
  data_link_node.role = ax::mojom::Role::kLink;
  data_link_node.SetName("Data Link");
  data_link_node.AddStringAttribute(ax::mojom::StringAttribute::kUrl, data_url);

  std::string empty_url = "";
  ui::AXNodeData empty_link_node;
  empty_link_node.id = 4;
  empty_link_node.role = ax::mojom::Role::kLink;
  empty_link_node.SetName("Empty Link");
  empty_link_node.AddStringAttribute(ax::mojom::StringAttribute::kUrl,
                                     empty_url);

  ui::AXNodeData root;
  root.id = 1;
  root.role = ax::mojom::Role::kRootWebArea;
  root.child_ids = {js_link_node.id, data_link_node.id, empty_link_node.id};

  ui::AXTreeUpdate update;
  test::SetUpdateTreeID(&update, tree_id_);
  update.root_id = root.id;
  update.nodes = {std::move(root), std::move(js_link_node),
                  std::move(data_link_node), std::move(empty_link_node)};

  ApplyAccessibilityUpdates(tree_id_, {std::move(update)});
  model().SetActiveTreeId(tree_id_);

  model().set_should_extract_anchors_from_tree_for_readability(true);
  model().ProcessAXTreeAnchors();

  const auto& result = model().ax_tree_anchors();
  ASSERT_TRUE(result.empty());
}
