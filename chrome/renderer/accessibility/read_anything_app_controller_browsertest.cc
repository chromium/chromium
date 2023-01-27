// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/accessibility/read_anything_app_controller.h"

#include <string>
#include <vector>

#include "chrome/renderer/accessibility/ax_tree_distiller.h"
#include "chrome/test/base/chrome_render_view_test.h"
#include "content/public/renderer/render_frame.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/accessibility/ax_node.h"

class MockAXTreeDistiller : public AXTreeDistiller {
 public:
  explicit MockAXTreeDistiller(content::RenderFrame* render_frame)
      : AXTreeDistiller(render_frame, base::NullCallback()) {}
  MOCK_METHOD(void,
              Distill,
              (const ui::AXTree& tree, const ui::AXTreeUpdate& snapshot),
              (override));
};

class MockReadAnythingPageHandler : public read_anything::mojom::PageHandler {
 public:
  MockReadAnythingPageHandler() = default;

  MOCK_METHOD(void,
              OnLinkClicked,
              (const ui::AXTreeID& target_tree_id, ui::AXNodeID target_node_id),
              (override));
  MOCK_METHOD(void,
              OnSelectionChange,
              (const ui::AXTreeID& target_tree_id,
               ui::AXNodeID anchor_node_id,
               int anchor_offset,
               ui::AXNodeID focus_node_id,
               int focus_offset),
              (override));

  mojo::PendingRemote<read_anything::mojom::PageHandler>
  BindNewPipeAndPassRemote() {
    return receiver_.BindNewPipeAndPassRemote();
  }
  void FlushForTesting() { receiver_.FlushForTesting(); }

 private:
  mojo::Receiver<read_anything::mojom::PageHandler> receiver_{this};
};

class ReadAnythingAppControllerTest : public ChromeRenderViewTest {
 public:
  ReadAnythingAppControllerTest() = default;
  ~ReadAnythingAppControllerTest() override = default;
  ReadAnythingAppControllerTest(const ReadAnythingAppControllerTest&) = delete;
  ReadAnythingAppControllerTest& operator=(
      const ReadAnythingAppControllerTest&) = delete;

  void SetUp() override {
    ChromeRenderViewTest::SetUp();
    content::RenderFrame* render_frame =
        content::RenderFrame::FromWebFrame(GetMainFrame());
    controller_ = ReadAnythingAppController::Install(render_frame);
    controller_->SetPageHandlerForTesting(
        page_handler_.BindNewPipeAndPassRemote());
    std::unique_ptr<AXTreeDistiller> distiller =
        std::make_unique<MockAXTreeDistiller>(render_frame);
    distiller_ = static_cast<MockAXTreeDistiller*>(
        controller_->SetDistillerForTesting(std::move(distiller)));

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

    // Send the snapshot to the controller and set its tree ID to be the active
    // tree ID. When the accessibility event is received and unserialized, the
    // controller will call distiller_->Distill().
    EXPECT_CALL(*distiller_, Distill).Times(1);
    AccessibilityEventReceived({snapshot});
    OnActiveAXTreeIDChanged(tree_id_);
    OnAXTreeDistilled({});
  }

  void SetUpdateTreeID(ui::AXTreeUpdate* update) {
    SetUpdateTreeID(update, tree_id_);
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
    controller_->SetThemeForTesting(font_name, font_size, foreground_color,
                                    background_color, line_spacing,
                                    letter_spacing);
  }

  void AccessibilityEventReceived(
      const std::vector<ui::AXTreeUpdate>& updates) {
    AccessibilityEventReceived(updates[0].tree_data.tree_id, updates);
  }

  void AccessibilityEventReceived(
      const ui::AXTreeID& tree_id,
      const std::vector<ui::AXTreeUpdate>& updates) {
    controller_->AccessibilityEventReceived(tree_id, updates, {});
  }

  void OnActiveAXTreeIDChanged(const ui::AXTreeID& tree_id) {
    controller_->OnActiveAXTreeIDChanged(tree_id);
  }

  void OnAXTreeDistilled(const std::vector<ui::AXNodeID>& content_node_ids) {
    OnAXTreeDistilled(tree_id_, content_node_ids);
  }

  void OnAXTreeDistilled(const ui::AXTreeID& tree_id,
                         const std::vector<ui::AXNodeID>& content_node_ids) {
    controller_->OnAXTreeDistilled(tree_id, content_node_ids);
  }

  void OnAXTreeDestroyed(const ui::AXTreeID& tree_id) {
    controller_->OnAXTreeDestroyed(tree_id);
  }

  ui::AXNodeID RootId() { return controller_->RootId(); }

  bool DisplayNodeIdsContains(ui::AXNodeID ax_node_id) {
    return base::Contains(controller_->display_node_ids_, ax_node_id);
  }

  std::string FontName() { return controller_->FontName(); }

  float FontSize() { return controller_->FontSize(); }

  SkColor ForegroundColor() { return controller_->ForegroundColor(); }

  SkColor BackgroundColor() { return controller_->BackgroundColor(); }

  float LineSpacing() { return controller_->LineSpacing(); }

  float LetterSpacing() { return controller_->LetterSpacing(); }

  std::vector<ui::AXNodeID> GetChildren(ui::AXNodeID ax_node_id) {
    return controller_->GetChildren(ax_node_id);
  }

  std::string GetHtmlTag(ui::AXNodeID ax_node_id) {
    return controller_->GetHtmlTag(ax_node_id);
  }

  std::string GetTextContent(ui::AXNodeID ax_node_id) {
    return controller_->GetTextContent(ax_node_id);
  }

  std::string GetUrl(ui::AXNodeID ax_node_id) {
    return controller_->GetUrl(ax_node_id);
  }

  bool ShouldBold(ui::AXNodeID ax_node_id) {
    return controller_->ShouldBold(ax_node_id);
  }

  bool IsOverline(ui::AXNodeID ax_node_id) {
    return controller_->IsOverline(ax_node_id);
  }

  void OnLinkClicked(ui::AXNodeID ax_node_id) {
    controller_->OnLinkClicked(ax_node_id);
  }

  void OnSelectionChange(ui::AXNodeID anchor_node_id,
                         int anchor_offset,
                         ui::AXNodeID focus_node_id,
                         int focus_offset) {
    controller_->OnSelectionChange(anchor_node_id, anchor_offset, focus_node_id,
                                   focus_offset);
  }

  bool IsNodeIgnoredForReadAnything(ui::AXNodeID ax_node_id) {
    return controller_->IsNodeIgnoredForReadAnything(ax_node_id);
  }

  size_t GetNumTrees() { return controller_->trees_.size(); }

  size_t GetNumPendingUpdates() { return controller_->pending_updates_.size(); }

  ui::AXTreeID tree_id_;
  MockAXTreeDistiller* distiller_ = nullptr;
  testing::StrictMock<MockReadAnythingPageHandler> page_handler_;

 private:
  // ReadAnythingAppController constructor and destructor are private so it's
  // not accessible by std::make_unique.
  ReadAnythingAppController* controller_ = nullptr;
};

TEST_F(ReadAnythingAppControllerTest, Theme) {
  std::string font_name = "Roboto";
  float font_size = 18.0;
  SkColor foreground = SkColorSetRGB(0x33, 0x36, 0x39);
  SkColor background = SkColorSetRGB(0xFD, 0xE2, 0x93);
  int letter_spacing = 0;  // enum value, kTight
  float letter_spacing_value = -0.05;
  int line_spacing = 1;  // enum value, kDefault
  float line_spacing_value = 1.15;
  SetThemeForTesting(font_name, font_size, foreground, background, line_spacing,
                     letter_spacing);
  EXPECT_EQ(font_name, FontName());
  EXPECT_EQ(font_size, FontSize());
  EXPECT_EQ(foreground, ForegroundColor());
  EXPECT_EQ(background, BackgroundColor());
  EXPECT_EQ(line_spacing_value, LineSpacing());
  EXPECT_EQ(letter_spacing_value, LetterSpacing());
}

TEST_F(ReadAnythingAppControllerTest, RootIdIsSnapshotRootId) {
  OnAXTreeDistilled({1});
  EXPECT_EQ(1, RootId());
  OnAXTreeDistilled({2});
  EXPECT_EQ(1, RootId());
  OnAXTreeDistilled({3});
  EXPECT_EQ(1, RootId());
  OnAXTreeDistilled({4});
  EXPECT_EQ(1, RootId());
}

TEST_F(ReadAnythingAppControllerTest, GetChildren_NoSelectionOrContentNodes) {
  ui::AXTreeUpdate update;
  SetUpdateTreeID(&update);
  update.nodes.resize(1);
  update.nodes[0].id = 3;
  update.nodes[0].role = ax::mojom::Role::kNone;
  AccessibilityEventReceived({update});
  OnAXTreeDistilled({});
  EXPECT_EQ(0u, GetChildren(1).size());
  EXPECT_EQ(0u, GetChildren(2).size());
  EXPECT_EQ(0u, GetChildren(3).size());
  EXPECT_EQ(0u, GetChildren(4).size());
}

TEST_F(ReadAnythingAppControllerTest, GetChildren_WithContentNodes) {
  ui::AXTreeUpdate update;
  SetUpdateTreeID(&update);
  update.nodes.resize(1);
  update.nodes[0].id = 3;
  update.nodes[0].role = ax::mojom::Role::kNone;
  AccessibilityEventReceived({update});
  OnAXTreeDistilled({1, 2, 3, 4});
  EXPECT_EQ(2u, GetChildren(1).size());
  EXPECT_EQ(0u, GetChildren(2).size());
  EXPECT_EQ(0u, GetChildren(3).size());
  EXPECT_EQ(0u, GetChildren(4).size());

  EXPECT_EQ(2, GetChildren(1)[0]);
  EXPECT_EQ(4, GetChildren(1)[1]);
}

TEST_F(ReadAnythingAppControllerTest, GetChildren_WithSelection) {
  // Create selection from node 3-4.
  ui::AXTreeUpdate update;
  SetUpdateTreeID(&update);
  update.tree_data.sel_anchor_object_id = 3;
  update.tree_data.sel_focus_object_id = 4;
  update.tree_data.sel_anchor_offset = 0;
  update.tree_data.sel_focus_offset = 0;
  update.tree_data.sel_is_backward = false;
  AccessibilityEventReceived({update});
  OnAXTreeDistilled({});
  EXPECT_EQ(2u, GetChildren(1).size());
  EXPECT_EQ(0u, GetChildren(2).size());
  EXPECT_EQ(0u, GetChildren(3).size());
  EXPECT_EQ(0u, GetChildren(4).size());

  EXPECT_EQ(3, GetChildren(1)[0]);
  EXPECT_EQ(4, GetChildren(1)[1]);
}

TEST_F(ReadAnythingAppControllerTest, GetChildren_WithBackwardSelection) {
  // Create backward selection from node 4-3.
  ui::AXTreeUpdate update;
  SetUpdateTreeID(&update);
  update.tree_data.sel_anchor_object_id = 4;
  update.tree_data.sel_focus_object_id = 3;
  update.tree_data.sel_anchor_offset = 0;
  update.tree_data.sel_focus_offset = 0;
  update.tree_data.sel_is_backward = true;
  AccessibilityEventReceived({update});
  OnAXTreeDistilled({});
  EXPECT_EQ(2u, GetChildren(1).size());
  EXPECT_EQ(0u, GetChildren(2).size());
  EXPECT_EQ(0u, GetChildren(3).size());
  EXPECT_EQ(0u, GetChildren(4).size());

  EXPECT_EQ(3, GetChildren(1)[0]);
  EXPECT_EQ(4, GetChildren(1)[1]);
}

TEST_F(ReadAnythingAppControllerTest, GetHtmlTag) {
  std::string span = "span";
  std::string h1 = "h1";
  std::string ul = "ul";
  ui::AXTreeUpdate update;
  SetUpdateTreeID(&update);
  update.nodes.resize(3);
  update.nodes[0].id = 2;
  update.nodes[1].id = 3;
  update.nodes[2].id = 4;
  update.nodes[0].AddStringAttribute(ax::mojom::StringAttribute::kHtmlTag,
                                     span);
  update.nodes[1].AddStringAttribute(ax::mojom::StringAttribute::kHtmlTag, h1);
  update.nodes[2].AddStringAttribute(ax::mojom::StringAttribute::kHtmlTag, ul);
  AccessibilityEventReceived({update});
  OnAXTreeDistilled({});
  EXPECT_EQ(span, GetHtmlTag(2));
  EXPECT_EQ(h1, GetHtmlTag(3));
  EXPECT_EQ(ul, GetHtmlTag(4));
}

TEST_F(ReadAnythingAppControllerTest, GetTextContent_NoSelection) {
  std::string text_content = "Hello";
  std::string missing_text_content = "";
  std::string more_text_content = " world";
  ui::AXTreeUpdate update;
  SetUpdateTreeID(&update);
  update.nodes.resize(3);
  update.nodes[0].id = 2;
  update.nodes[1].id = 3;
  update.nodes[2].id = 4;
  update.nodes[0].role = ax::mojom::Role::kStaticText;
  update.nodes[0].SetName(text_content);
  update.nodes[0].SetNameFrom(ax::mojom::NameFrom::kContents);
  update.nodes[1].role = ax::mojom::Role::kStaticText;
  update.nodes[1].SetName(missing_text_content);
  update.nodes[1].SetNameFrom(ax::mojom::NameFrom::kContents);
  update.nodes[2].role = ax::mojom::Role::kStaticText;
  update.nodes[2].SetName(more_text_content);
  update.nodes[2].SetNameFrom(ax::mojom::NameFrom::kContents);
  AccessibilityEventReceived({update});
  OnAXTreeDistilled({});
  EXPECT_EQ("Hello world", GetTextContent(1));
  EXPECT_EQ(text_content, GetTextContent(2));
  EXPECT_EQ(missing_text_content, GetTextContent(3));
  EXPECT_EQ(more_text_content, GetTextContent(4));
}

TEST_F(ReadAnythingAppControllerTest, GetTextContent_With2NodeSelection) {
  std::string text_content_1 = "Hello";
  std::string text_content_2 = " world";
  std::string text_content_3 = " friend";
  ui::AXTreeUpdate update;
  SetUpdateTreeID(&update);
  update.nodes.resize(3);
  update.nodes[0].id = 2;
  update.nodes[1].id = 3;
  update.nodes[2].id = 4;
  update.nodes[0].role = ax::mojom::Role::kStaticText;
  update.nodes[0].SetName(text_content_1);
  update.nodes[0].SetNameFrom(ax::mojom::NameFrom::kContents);
  update.nodes[1].role = ax::mojom::Role::kStaticText;
  update.nodes[1].SetName(text_content_2);
  update.nodes[1].SetNameFrom(ax::mojom::NameFrom::kContents);
  update.nodes[2].role = ax::mojom::Role::kStaticText;
  update.nodes[2].SetName(text_content_3);
  update.nodes[2].SetNameFrom(ax::mojom::NameFrom::kContents);
  // Create selection from node 2-3.
  update.tree_data.sel_anchor_object_id = 2;
  update.tree_data.sel_focus_object_id = 3;
  update.tree_data.sel_anchor_offset = 1;
  update.tree_data.sel_focus_offset = 3;
  update.tree_data.sel_is_backward = false;
  AccessibilityEventReceived({update});
  OnAXTreeDistilled({});
  EXPECT_EQ("Hello world friend", GetTextContent(1));
  EXPECT_EQ("ello", GetTextContent(2));
  EXPECT_EQ(" wo", GetTextContent(3));
  EXPECT_EQ(" friend", GetTextContent(4));
}

TEST_F(ReadAnythingAppControllerTest, GetTextContent_With3NodeSelection) {
  std::string text_content_1 = "Hello";
  std::string text_content_2 = " world";
  std::string text_content_3 = " friend";
  ui::AXTreeUpdate update;
  SetUpdateTreeID(&update);
  update.nodes.resize(3);
  update.nodes[0].id = 2;
  update.nodes[1].id = 3;
  update.nodes[2].id = 4;
  update.nodes[0].role = ax::mojom::Role::kStaticText;
  update.nodes[0].SetName(text_content_1);
  update.nodes[0].SetNameFrom(ax::mojom::NameFrom::kContents);
  update.nodes[1].role = ax::mojom::Role::kStaticText;
  update.nodes[1].SetName(text_content_2);
  update.nodes[1].SetNameFrom(ax::mojom::NameFrom::kContents);
  update.nodes[2].role = ax::mojom::Role::kStaticText;
  update.nodes[2].SetName(text_content_3);
  update.nodes[2].SetNameFrom(ax::mojom::NameFrom::kContents);
  // Create selection from node 2-4.
  update.tree_data.sel_anchor_object_id = 2;
  update.tree_data.sel_focus_object_id = 4;
  update.tree_data.sel_anchor_offset = 1;
  update.tree_data.sel_focus_offset = 3;
  update.tree_data.sel_is_backward = false;
  AccessibilityEventReceived({update});
  OnAXTreeDistilled({});
  EXPECT_EQ("Hello world friend", GetTextContent(1));
  EXPECT_EQ("ello", GetTextContent(2));
  EXPECT_EQ(" world", GetTextContent(3));
  EXPECT_EQ(" fr", GetTextContent(4));
}

TEST_F(ReadAnythingAppControllerTest, GetTextContent_WithBackwardSelection) {
  std::string text_content_1 = "Hello";
  std::string text_content_2 = " world";
  std::string text_content_3 = " friend";
  ui::AXTreeUpdate update;
  SetUpdateTreeID(&update);
  update.nodes.resize(3);
  update.nodes[0].id = 2;
  update.nodes[1].id = 3;
  update.nodes[2].id = 4;
  update.nodes[0].role = ax::mojom::Role::kStaticText;
  update.nodes[0].SetName(text_content_1);
  update.nodes[0].SetNameFrom(ax::mojom::NameFrom::kContents);
  update.nodes[1].role = ax::mojom::Role::kStaticText;
  update.nodes[1].SetName(text_content_2);
  update.nodes[1].SetNameFrom(ax::mojom::NameFrom::kContents);
  update.nodes[2].role = ax::mojom::Role::kStaticText;
  update.nodes[2].SetName(text_content_3);
  update.nodes[2].SetNameFrom(ax::mojom::NameFrom::kContents);
  // Create backward selection from node 4-3.
  update.tree_data.sel_anchor_object_id = 4;
  update.tree_data.sel_focus_object_id = 3;
  update.tree_data.sel_anchor_offset = 5;
  update.tree_data.sel_focus_offset = 2;
  update.tree_data.sel_is_backward = true;
  AccessibilityEventReceived({update});
  OnAXTreeDistilled({});
  EXPECT_EQ("Hello world friend", GetTextContent(1));
  EXPECT_EQ("Hello", GetTextContent(2));
  EXPECT_EQ("orld", GetTextContent(3));
  EXPECT_EQ(" frie", GetTextContent(4));
}

TEST_F(ReadAnythingAppControllerTest, GetUrl) {
  std::string url = "http://www.google.com";
  std::string invalid_url = "cats";
  std::string missing_url = "";
  ui::AXTreeUpdate update;
  SetUpdateTreeID(&update);
  update.nodes.resize(3);
  update.nodes[0].id = 2;
  update.nodes[1].id = 3;
  update.nodes[2].id = 4;
  update.nodes[0].AddStringAttribute(ax::mojom::StringAttribute::kUrl, url);
  update.nodes[1].AddStringAttribute(ax::mojom::StringAttribute::kUrl,
                                     invalid_url);
  update.nodes[2].AddStringAttribute(ax::mojom::StringAttribute::kUrl,
                                     missing_url);
  AccessibilityEventReceived({update});
  OnAXTreeDistilled({});
  EXPECT_EQ(url, GetUrl(2));
  EXPECT_EQ(invalid_url, GetUrl(3));
  EXPECT_EQ(missing_url, GetUrl(4));
}

TEST_F(ReadAnythingAppControllerTest, ShouldBold) {
  ui::AXTreeUpdate update;
  SetUpdateTreeID(&update);
  update.nodes.resize(3);
  update.nodes[0].id = 2;
  update.nodes[1].id = 3;
  update.nodes[2].id = 4;
  update.nodes[0].AddTextStyle(ax::mojom::TextStyle::kOverline);
  update.nodes[1].AddTextStyle(ax::mojom::TextStyle::kUnderline);
  update.nodes[2].AddTextStyle(ax::mojom::TextStyle::kItalic);
  AccessibilityEventReceived({update});
  OnAXTreeDistilled({});
  EXPECT_EQ(false, ShouldBold(2));
  EXPECT_EQ(true, ShouldBold(3));
  EXPECT_EQ(true, ShouldBold(4));
}

TEST_F(ReadAnythingAppControllerTest, IsOverline) {
  ui::AXTreeUpdate update;
  SetUpdateTreeID(&update);
  update.nodes.resize(2);
  update.nodes[0].id = 2;
  update.nodes[1].id = 3;
  update.nodes[0].AddTextStyle(ax::mojom::TextStyle::kOverline);
  update.nodes[1].AddTextStyle(ax::mojom::TextStyle::kUnderline);
  AccessibilityEventReceived({update});
  OnAXTreeDistilled({});
  EXPECT_EQ(true, IsOverline(2));
  EXPECT_EQ(false, IsOverline(3));
}

TEST_F(ReadAnythingAppControllerTest, IsNodeIgnoredForReadAnything) {
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
  OnAXTreeDistilled({});
  EXPECT_EQ(false, IsNodeIgnoredForReadAnything(2));
  EXPECT_EQ(true, IsNodeIgnoredForReadAnything(3));
  EXPECT_EQ(true, IsNodeIgnoredForReadAnything(4));
}

TEST_F(ReadAnythingAppControllerTest, DisplayNodeIdsContains_Selection) {
  ui::AXTreeUpdate update;
  SetUpdateTreeID(&update);
  update.tree_data.sel_anchor_object_id = 2;
  update.tree_data.sel_focus_object_id = 3;
  update.tree_data.sel_anchor_offset = 0;
  update.tree_data.sel_focus_offset = 0;
  update.tree_data.sel_is_backward = false;
  AccessibilityEventReceived({update});
  OnAXTreeDistilled({});
  EXPECT_TRUE(DisplayNodeIdsContains(1));
  EXPECT_TRUE(DisplayNodeIdsContains(2));
  EXPECT_TRUE(DisplayNodeIdsContains(3));
  EXPECT_FALSE(DisplayNodeIdsContains(4));
}

TEST_F(ReadAnythingAppControllerTest,
       DisplayNodeIdsContains_BackwardSelection) {
  ui::AXTreeUpdate update;
  SetUpdateTreeID(&update);
  update.tree_data.sel_anchor_object_id = 3;
  update.tree_data.sel_focus_object_id = 2;
  update.tree_data.sel_anchor_offset = 0;
  update.tree_data.sel_focus_offset = 0;
  update.tree_data.sel_is_backward = true;
  AccessibilityEventReceived({update});
  OnAXTreeDistilled({});
  EXPECT_TRUE(DisplayNodeIdsContains(1));
  EXPECT_TRUE(DisplayNodeIdsContains(2));
  EXPECT_TRUE(DisplayNodeIdsContains(3));
  EXPECT_FALSE(DisplayNodeIdsContains(4));
}

TEST_F(ReadAnythingAppControllerTest, DisplayNodeIdsContains_ContentNodes) {
  ui::AXTreeUpdate update;
  SetUpdateTreeID(&update);
  update.nodes.resize(3);
  update.nodes[0].id = 4;
  update.nodes[0].child_ids = {5, 6};
  update.nodes[1].id = 5;
  update.nodes[2].id = 6;
  // This update changes the structure of the tree. When the controller receives
  // it in AccessibilityEventReceived, it will re-distill the tree.
  EXPECT_CALL(*distiller_, Distill).Times(1);
  AccessibilityEventReceived({update});
  OnAXTreeDistilled({3, 4});
  EXPECT_TRUE(DisplayNodeIdsContains(1));
  EXPECT_FALSE(DisplayNodeIdsContains(2));
  EXPECT_TRUE(DisplayNodeIdsContains(3));
  EXPECT_TRUE(DisplayNodeIdsContains(4));
  EXPECT_TRUE(DisplayNodeIdsContains(5));
  EXPECT_TRUE(DisplayNodeIdsContains(6));
}

TEST_F(ReadAnythingAppControllerTest,
       DisplayNodeIdsContains_NoSelectionOrContentNodes) {
  OnAXTreeDistilled({});
  EXPECT_FALSE(DisplayNodeIdsContains(1));
  EXPECT_FALSE(DisplayNodeIdsContains(2));
  EXPECT_FALSE(DisplayNodeIdsContains(3));
  EXPECT_FALSE(DisplayNodeIdsContains(4));
}

TEST_F(ReadAnythingAppControllerTest, DoesNotCrashIfContentNodeNotFoundInTree) {
  OnAXTreeDistilled({6});
}

TEST_F(ReadAnythingAppControllerTest, AccessibilityEventReceived) {
  // Tree starts off with no text content.
  EXPECT_EQ("", GetTextContent(1));
  EXPECT_EQ("", GetTextContent(2));
  EXPECT_EQ("", GetTextContent(3));
  EXPECT_EQ("", GetTextContent(4));

  // Send a new update which settings the text content of node 2.
  ui::AXTreeUpdate update_1;
  SetUpdateTreeID(&update_1);
  update_1.nodes.resize(1);
  update_1.nodes[0].id = 2;
  update_1.nodes[0].role = ax::mojom::Role::kStaticText;
  update_1.nodes[0].SetName("Hello world");
  update_1.nodes[0].SetNameFrom(ax::mojom::NameFrom::kContents);
  AccessibilityEventReceived({update_1});
  EXPECT_EQ("Hello world", GetTextContent(1));
  EXPECT_EQ("Hello world", GetTextContent(2));
  EXPECT_EQ("", GetTextContent(3));
  EXPECT_EQ("", GetTextContent(4));

  // Send three updates which should be merged.
  std::vector<ui::AXTreeUpdate> batch_updates;
  for (int i = 2; i < 5; i++) {
    ui::AXTreeUpdate update;
    SetUpdateTreeID(&update);
    update.nodes.resize(1);
    update.nodes[0].id = i;
    update.nodes[0].role = ax::mojom::Role::kStaticText;
    update.nodes[0].SetName("Node " + base::NumberToString(i));
    update.nodes[0].SetNameFrom(ax::mojom::NameFrom::kContents);
    batch_updates.push_back(update);
  }
  AccessibilityEventReceived(batch_updates);
  EXPECT_EQ("Node 2Node 3Node 4", GetTextContent(1));
  EXPECT_EQ("Node 2", GetTextContent(2));
  EXPECT_EQ("Node 3", GetTextContent(3));
  EXPECT_EQ("Node 4", GetTextContent(4));

  // Clear node 1.
  ui::AXTreeUpdate clear_update;
  SetUpdateTreeID(&clear_update);
  clear_update.root_id = 1;
  clear_update.node_id_to_clear = 1;
  clear_update.nodes.resize(1);
  clear_update.nodes[0].id = 1;
  AccessibilityEventReceived({clear_update});
  EXPECT_EQ("", GetTextContent(1));
}

TEST_F(ReadAnythingAppControllerTest, OnActiveAXTreeIDChanged) {
  // Create three AXTreeUpdates with three different tree IDs.
  std::vector<ui::AXTreeID> tree_ids = {ui::AXTreeID::CreateNewAXTreeID(),
                                        ui::AXTreeID::CreateNewAXTreeID(),
                                        tree_id_};
  std::vector<ui::AXTreeUpdate> updates;
  for (int i = 0; i < 3; i++) {
    ui::AXTreeUpdate update;
    SetUpdateTreeID(&update, tree_ids[i]);
    update.root_id = 1;
    update.nodes.resize(1);
    update.nodes[0].id = 1;
    update.nodes[0].role = ax::mojom::Role::kStaticText;
    update.nodes[0].SetName("Tree " + base::NumberToString(i));
    update.nodes[0].SetNameFrom(ax::mojom::NameFrom::kContents);
    updates.push_back(update);
  }
  // Add the three updates separately since they have different tree IDs.
  for (int i = 0; i < 3; i++) {
    AccessibilityEventReceived({updates[i]});
  }

  OnAXTreeDistilled({1});

  // Check that changing the active tree ID changes the active tree which is
  // used when using a v8 getter.
  for (int i = 0; i < 3; i++) {
    EXPECT_CALL(*distiller_, Distill).Times(1);
    OnActiveAXTreeIDChanged(tree_ids[i]);
    EXPECT_EQ("Tree " + base::NumberToString(i), GetTextContent(1));
  }

  // Changing the active tree ID to the same ID does nothing.
  EXPECT_CALL(*distiller_, Distill).Times(0);
  OnActiveAXTreeIDChanged(tree_ids[2]);
}

TEST_F(ReadAnythingAppControllerTest, DoesNotCrashIfActiveAXTreeIDUnknown) {
  EXPECT_CALL(*distiller_, Distill).Times(0);
  ui::AXTreeID tree_id = ui::AXTreeIDUnknown();
  OnActiveAXTreeIDChanged(tree_id);
  OnAXTreeDestroyed(tree_id);
  OnAXTreeDistilled({1});
}

TEST_F(ReadAnythingAppControllerTest, DoesNotCrashIfActiveAXTreeIDNotInTrees) {
  ui::AXTreeID tree_id = ui::AXTreeID::CreateNewAXTreeID();
  OnActiveAXTreeIDChanged(tree_id);
  OnAXTreeDestroyed(tree_id);
}

TEST_F(ReadAnythingAppControllerTest, AddAndRemoveTrees) {
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

  // Add the two trees.
  AccessibilityEventReceived({updates[0]});
  ASSERT_EQ(2u, GetNumTrees());
  AccessibilityEventReceived({updates[1]});
  ASSERT_EQ(3u, GetNumTrees());

  // Remove all of the trees.
  OnAXTreeDestroyed(tree_id_);
  ASSERT_EQ(2u, GetNumTrees());
  OnAXTreeDestroyed(tree_ids[0]);
  ASSERT_EQ(1u, GetNumTrees());
  OnAXTreeDestroyed(tree_ids[1]);
  ASSERT_EQ(0u, GetNumTrees());
}

TEST_F(ReadAnythingAppControllerTest, OnAXTreeDestroyed_ChildTree) {
  // Create three AXTreeUpdates. The first has tree ID tree_id_ and the others
  // have two new tree IDs.
  std::vector<ui::AXTreeID> tree_ids = {tree_id_,
                                        ui::AXTreeID::CreateNewAXTreeID(),
                                        ui::AXTreeID::CreateNewAXTreeID()};
  std::vector<ui::AXTreeUpdate> updates;
  for (int i = 0; i < 3; i++) {
    ui::AXTreeUpdate update;
    SetUpdateTreeID(&update, tree_ids[i]);
    update.root_id = 1;
    update.nodes.resize(1);
    update.nodes[0].id = 1;
    updates.push_back(update);
  }

  // Tree_ids[0] is the parent of tree_ids[1]. Tree_ids[1] is the parent of
  // tree_ids[2].
  updates[1].tree_data.parent_tree_id = tree_ids[0];
  updates[2].tree_data.parent_tree_id = tree_ids[1];
  updates[0].nodes[0].AddChildTreeId(tree_ids[1]);
  updates[1].nodes[0].AddChildTreeId(tree_ids[2]);

  // Start with 1 tree (the tree created in SetUp).
  ASSERT_EQ(1u, GetNumTrees());
  AccessibilityEventReceived({updates[0]});
  ASSERT_EQ(1u, GetNumTrees());

  // Add two trees.
  AccessibilityEventReceived({updates[1]});
  ASSERT_EQ(2u, GetNumTrees());
  AccessibilityEventReceived({updates[2]});
  ASSERT_EQ(3u, GetNumTrees());

  // Remove the grandparent tree (tree_ids[0]. When it is removed, its
  // child (tree_ids[1]) and its child's child (tree_ids[2]) are both removed.
  OnAXTreeDestroyed(tree_ids[0]);
  ASSERT_EQ(0u, GetNumTrees());
}

TEST_F(ReadAnythingAppControllerTest,
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
  // Since this update is just cosmetic (it changes the nodes' name but doesn't
  // change the structure of the tree by adding or removing nodes), the
  // controller does not distill.
  EXPECT_CALL(*distiller_, Distill).Times(0);
  AccessibilityEventReceived({initial_update});
  EXPECT_EQ("234", GetTextContent(1));

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
  EXPECT_CALL(*distiller_, Distill).Times(1);
  AccessibilityEventReceived({updates[0]});
  EXPECT_EQ("2345", GetTextContent(1));
  EXPECT_EQ(0u, GetNumPendingUpdates());

  // Send update 1. Since distillation is in progress, this will not be
  // unserialized yet.
  EXPECT_CALL(*distiller_, Distill).Times(0);
  AccessibilityEventReceived({updates[1]});
  EXPECT_EQ("2345", GetTextContent(1));
  EXPECT_EQ(1u, GetNumPendingUpdates());

  // Send update 2. This is still not unserialized yet.
  EXPECT_CALL(*distiller_, Distill).Times(0);
  AccessibilityEventReceived({updates[2]});
  EXPECT_EQ("2345", GetTextContent(1));
  EXPECT_EQ(2u, GetNumPendingUpdates());

  // Complete distillation which unserializes the pending updates and distills
  // them.
  EXPECT_CALL(*distiller_, Distill).Times(2);
  OnAXTreeDistilled({1});
  EXPECT_EQ("234567", GetTextContent(1));
  EXPECT_EQ(0u, GetNumPendingUpdates());
}

TEST_F(ReadAnythingAppControllerTest,
       DistillationInProgress_TreeUpdateReceivedOnInactiveTree) {
  EXPECT_EQ(0u, GetNumPendingUpdates());

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
  EXPECT_EQ(0u, GetNumPendingUpdates());
}

TEST_F(ReadAnythingAppControllerTest,
       DistillationInProgress_ActiveTreeIDChanges) {
  EXPECT_EQ(0u, GetNumPendingUpdates());

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

  EXPECT_CALL(*distiller_, Distill).Times(1);
  AccessibilityEventReceived({updates[0]});
  EXPECT_EQ(0u, GetNumPendingUpdates());
  EXPECT_CALL(*distiller_, Distill).Times(0);
  AccessibilityEventReceived({updates[1]});
  EXPECT_EQ(1u, GetNumPendingUpdates());
  EXPECT_CALL(*distiller_, Distill).Times(0);
  AccessibilityEventReceived({updates[2]});
  EXPECT_EQ(2u, GetNumPendingUpdates());
  EXPECT_EQ("5", GetTextContent(1));

  // Switching the active AXTreeID flushes the pending updates.
  ui::AXTreeID tree_id_2 = ui::AXTreeID::CreateNewAXTreeID();
  EXPECT_CALL(*distiller_, Distill).Times(0);
  OnActiveAXTreeIDChanged(tree_id_2);
  EXPECT_EQ(0u, GetNumPendingUpdates());

  EXPECT_CALL(*distiller_, Distill).Times(1);
  OnActiveAXTreeIDChanged(tree_id_);
  EXPECT_EQ("567", GetTextContent(1));
}

TEST_F(ReadAnythingAppControllerTest,
       OnAXTreeDistilledCalledWithInactiveTreeId) {
  OnActiveAXTreeIDChanged(ui::AXTreeID::CreateNewAXTreeID());
  // Should not crash.
  OnAXTreeDistilled({});
}

TEST_F(ReadAnythingAppControllerTest,
       OnAXTreeDistilledCalledWithDestroyedTreeId) {
  OnAXTreeDestroyed(tree_id_);
  // Should not crash.
  OnAXTreeDistilled({});
}

TEST_F(ReadAnythingAppControllerTest,
       OnAXTreeDistilledCalledWithUnknownActiveTreeId) {
  OnActiveAXTreeIDChanged(ui::AXTreeIDUnknown());
  // Should not crash.
  OnAXTreeDistilled({});
}

TEST_F(ReadAnythingAppControllerTest,
       OnAXTreeDistilledCalledWithUnknownTreeId) {
  // Should not crash.
  OnAXTreeDistilled(ui::AXTreeIDUnknown(), {});
}

TEST_F(ReadAnythingAppControllerTest,
       ChangeActiveTreeWithPendingUpdates_UnknownID) {
  EXPECT_EQ(0u, GetNumPendingUpdates());

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
  EXPECT_CALL(*distiller_, Distill).Times(1);
  AccessibilityEventReceived({updates[0]});
  EXPECT_EQ(0u, GetNumPendingUpdates());
  AccessibilityEventReceived(tree_id_, {updates[1], updates[2]});
  EXPECT_EQ(2u, GetNumPendingUpdates());

  // Switch to a new active tree. Should not crash.
  EXPECT_CALL(*distiller_, Distill).Times(0);
  OnActiveAXTreeIDChanged(ui::AXTreeIDUnknown());
}

TEST_F(ReadAnythingAppControllerTest, OnLinkClicked) {
  ui::AXNodeID ax_node_id = 2;
  EXPECT_CALL(page_handler_, OnLinkClicked(tree_id_, ax_node_id)).Times(1);
  OnLinkClicked(ax_node_id);
  page_handler_.FlushForTesting();
}

TEST_F(ReadAnythingAppControllerTest, OnLinkClicked_DistillationInProgress) {
  ui::AXTreeID new_tree_id = ui::AXTreeID::CreateNewAXTreeID();
  ui::AXTreeUpdate update;
  SetUpdateTreeID(&update, new_tree_id);
  update.root_id = 1;
  update.nodes.resize(1);
  update.nodes[0].id = 1;
  AccessibilityEventReceived({update});
  EXPECT_CALL(*distiller_, Distill).Times(1);
  OnActiveAXTreeIDChanged(new_tree_id);

  // If distillation is in progress, OnLinkClicked should not be called.
  EXPECT_CALL(page_handler_, OnLinkClicked).Times(0);
  OnLinkClicked(2);
  page_handler_.FlushForTesting();
}

TEST_F(ReadAnythingAppControllerTest, OnSelectionChange) {
  ui::AXNodeID anchor_node_id = 2;
  int anchor_offset = 0;
  ui::AXNodeID focus_node_id = 3;
  int focus_offset = 1;
  EXPECT_CALL(page_handler_,
              OnSelectionChange(tree_id_, anchor_node_id, anchor_offset,
                                focus_node_id, focus_offset))
      .Times(1);
  OnSelectionChange(anchor_node_id, anchor_offset, focus_node_id, focus_offset);
  page_handler_.FlushForTesting();
}

TEST_F(ReadAnythingAppControllerTest,
       OnSelectionChange_DistillationInProgress) {
  ui::AXTreeID new_tree_id = ui::AXTreeID::CreateNewAXTreeID();
  ui::AXTreeUpdate update;
  SetUpdateTreeID(&update, new_tree_id);
  update.root_id = 1;
  update.nodes.resize(1);
  update.nodes[0].id = 1;
  AccessibilityEventReceived({update});
  EXPECT_CALL(*distiller_, Distill).Times(1);
  OnActiveAXTreeIDChanged(new_tree_id);

  // If distillation is in progress, OnSelectionChange should not be called.
  EXPECT_CALL(page_handler_, OnSelectionChange).Times(0);
  OnSelectionChange(2, 0, 3, 1);
  page_handler_.FlushForTesting();
}
