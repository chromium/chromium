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
#include "ui/accessibility/ax_serializable_tree.h"
#include "url/gurl.h"

class MockAXTreeDistiller : public AXTreeDistiller {
 public:
  explicit MockAXTreeDistiller(content::RenderFrame* render_frame)
      : AXTreeDistiller(render_frame, base::NullCallback()) {}
  MOCK_METHOD(void,
              Distill,
              (const ui::AXTree& tree,
               const ui::AXTreeUpdate& snapshot,
               const ukm::SourceId ukm_source_id),
              (override));
};

class MockReadAnythingUntrustedPageHandler
    : public read_anything::mojom::UntrustedPageHandler {
 public:
  MockReadAnythingUntrustedPageHandler() = default;

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
  MOCK_METHOD(void, OnCopy, (), (override));

  mojo::PendingRemote<read_anything::mojom::UntrustedPageHandler>
  BindNewPipeAndPassRemote() {
    return receiver_.BindNewPipeAndPassRemote();
  }
  void FlushForTesting() { receiver_.FlushForTesting(); }

 private:
  mojo::Receiver<read_anything::mojom::UntrustedPageHandler> receiver_{this};
};

using testing::Mock;

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

    // Set the page handler for testing.
    controller_->page_handler_.reset();
    controller_->page_handler_.Bind(page_handler_.BindNewPipeAndPassRemote());

    // Set distiller for testing.
    std::unique_ptr<AXTreeDistiller> distiller =
        std::make_unique<MockAXTreeDistiller>(render_frame);
    controller_->distiller_ = std::move(distiller);
    distiller_ =
        static_cast<MockAXTreeDistiller*>(controller_->distiller_.get());

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
    Mock::VerifyAndClearExpectations(distiller_);
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
      const std::vector<ui::AXTreeUpdate>& updates,
      const std::vector<ui::AXEvent>& events = std::vector<ui::AXEvent>()) {
    AccessibilityEventReceived(updates[0].tree_data.tree_id, updates, events);
  }

  void AccessibilityEventReceived(
      const ui::AXTreeID& tree_id,
      const std::vector<ui::AXTreeUpdate>& updates,
      const std::vector<ui::AXEvent>& events = std::vector<ui::AXEvent>()) {
    controller_->AccessibilityEventReceived(tree_id, updates, events);
  }

  // Since a11y events happen asynchronously, they can come between the time
  // distillation finishes and pending updates are unserialized in
  // OnAXTreeDistilled. Thus we need to be able to set distillation progress
  // independent of OnAXTreeDistilled.
  void SetDistillationInProgress(bool in_progress) {
    controller_->model_.SetDistillationInProgress(in_progress);
  }

  void OnActiveAXTreeIDChanged(const ui::AXTreeID& tree_id) {
    OnActiveAXTreeIDChanged(tree_id, GURL::EmptyGURL());
  }

  void OnActiveAXTreeIDChanged(const ui::AXTreeID& tree_id, const GURL& url) {
    controller_->OnActiveAXTreeIDChanged(tree_id, ukm::kInvalidSourceId, url);
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

  ui::AXNodeID StartNodeId() { return controller_->StartNodeId(); }

  int StartOffset() { return controller_->StartOffset(); }

  ui::AXNodeID EndNodeId() { return controller_->EndNodeId(); }

  int EndOffset() { return controller_->EndOffset(); }

  bool HasSelection() { return controller_->model_.has_selection(); }

  bool DisplayNodeIdsContains(ui::AXNodeID ax_node_id) {
    return base::Contains(controller_->model_.display_node_ids(), ax_node_id);
  }

  bool SelectionNodeIdsContains(ui::AXNodeID ax_node_id) {
    return base::Contains(controller_->model_.selection_node_ids(), ax_node_id);
  }

  std::string FontName() { return controller_->FontName(); }

  float FontSize() { return controller_->FontSize(); }

  SkColor ForegroundColor() { return controller_->ForegroundColor(); }

  SkColor BackgroundColor() { return controller_->BackgroundColor(); }

  float LineSpacing() { return controller_->LineSpacing(); }

  float LetterSpacing() { return controller_->LetterSpacing(); }

  bool isSelectable() { return controller_->IsSelectable(); }

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
    return controller_->model_.IsNodeIgnoredForReadAnything(ax_node_id);
  }

  bool HasTree(ui::AXTreeID tree_id) {
    return controller_->model_.ContainsTree(tree_id);
  }

  ui::AXTreeID ActiveTreeId() { return controller_->model_.active_tree_id(); }

  ui::AXTreeID tree_id_;
  MockAXTreeDistiller* distiller_ = nullptr;
  testing::StrictMock<MockReadAnythingUntrustedPageHandler> page_handler_;

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

TEST_F(ReadAnythingAppControllerTest,
       GetChildren_WithSelection_ContainsNearbyNodes) {
  // Create selection from node 3-4.
  ui::AXTreeUpdate update;
  SetUpdateTreeID(&update);
  update.has_tree_data = true;
  update.event_from = ax::mojom::EventFrom::kUser;
  update.tree_data.sel_anchor_object_id = 3;
  update.tree_data.sel_focus_object_id = 4;
  update.tree_data.sel_anchor_offset = 0;
  update.tree_data.sel_focus_offset = 0;
  update.tree_data.sel_is_backward = false;
  AccessibilityEventReceived({update});
  EXPECT_EQ(3u, GetChildren(1).size());
  EXPECT_EQ(0u, GetChildren(2).size());
  EXPECT_EQ(0u, GetChildren(3).size());
  EXPECT_EQ(0u, GetChildren(4).size());

  EXPECT_EQ(2, GetChildren(1)[0]);
  EXPECT_EQ(3, GetChildren(1)[1]);
  EXPECT_EQ(4, GetChildren(1)[2]);
}

TEST_F(ReadAnythingAppControllerTest,
       GetChildren_WithBackwardSelection_ContainsNearbyNodes) {
  // Create backward selection from node 4-3.
  ui::AXTreeUpdate update;
  SetUpdateTreeID(&update);
  update.has_tree_data = true;
  update.event_from = ax::mojom::EventFrom::kUser;
  update.tree_data.sel_anchor_object_id = 4;
  update.tree_data.sel_focus_object_id = 3;
  update.tree_data.sel_anchor_offset = 0;
  update.tree_data.sel_focus_offset = 0;
  update.tree_data.sel_is_backward = true;
  AccessibilityEventReceived({update});
  EXPECT_EQ(3u, GetChildren(1).size());
  EXPECT_EQ(0u, GetChildren(2).size());
  EXPECT_EQ(0u, GetChildren(3).size());
  EXPECT_EQ(0u, GetChildren(4).size());

  EXPECT_EQ(2, GetChildren(1)[0]);
  EXPECT_EQ(3, GetChildren(1)[1]);
  EXPECT_EQ(4, GetChildren(1)[2]);
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

TEST_F(ReadAnythingAppControllerTest, GetHtmlTag_TextFieldReturnsDiv) {
  std::string span = "span";
  std::string h1 = "h1";
  std::string ul = "ul";
  std::string div = "div";
  ui::AXTreeUpdate update;
  SetUpdateTreeID(&update);
  update.nodes.resize(3);
  update.nodes[0].id = 2;
  update.nodes[1].id = 3;
  update.nodes[2].id = 4;
  update.nodes[0].AddStringAttribute(ax::mojom::StringAttribute::kHtmlTag,
                                     span);
  update.nodes[1].AddStringAttribute(ax::mojom::StringAttribute::kHtmlTag, h1);
  update.nodes[1].role = ax::mojom::Role::kTextField;
  update.nodes[2].AddStringAttribute(ax::mojom::StringAttribute::kHtmlTag, ul);
  update.nodes[2].role = ax::mojom::Role::kTextFieldWithComboBox;
  AccessibilityEventReceived({update});
  OnAXTreeDistilled({});
  EXPECT_EQ(span, GetHtmlTag(2));
  EXPECT_EQ(div, GetHtmlTag(3));
  EXPECT_EQ(div, GetHtmlTag(4));
}

TEST_F(ReadAnythingAppControllerTest,
       GetHtmlTag_DivWithHeadingAndAriaLevelReturnsH) {
  std::string h3 = "h3";
  std::string div = "div";
  ui::AXTreeUpdate update;
  SetUpdateTreeID(&update);
  update.nodes.resize(3);
  update.nodes[0].id = 2;
  update.nodes[1].id = 3;
  update.nodes[2].id = 4;
  update.nodes[1].role = ax::mojom::Role::kHeading;
  update.nodes[1].html_attributes.emplace_back("aria-level", "3");
  AccessibilityEventReceived({update});
  OnAXTreeDistilled({});
  EXPECT_EQ(h3, GetHtmlTag(3));
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

TEST_F(ReadAnythingAppControllerTest, GetTextContent_WithSelection) {
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
  EXPECT_EQ("Hello", GetTextContent(2));
  EXPECT_EQ(" world", GetTextContent(3));
  EXPECT_EQ(" friend", GetTextContent(4));
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

TEST_F(ReadAnythingAppControllerTest,
       SelectionNodeIdsContains_SelectionAndNearbyNodes) {
  ui::AXTreeUpdate update;
  SetUpdateTreeID(&update);
  update.has_tree_data = true;
  update.event_from = ax::mojom::EventFrom::kUser;
  update.tree_data.sel_anchor_object_id = 2;
  update.tree_data.sel_focus_object_id = 3;
  update.tree_data.sel_anchor_offset = 0;
  update.tree_data.sel_focus_offset = 0;
  update.tree_data.sel_is_backward = false;

  AccessibilityEventReceived({update});
  EXPECT_TRUE(SelectionNodeIdsContains(1));
  EXPECT_TRUE(SelectionNodeIdsContains(2));
  EXPECT_TRUE(SelectionNodeIdsContains(3));
  EXPECT_TRUE(SelectionNodeIdsContains(4));
}

TEST_F(ReadAnythingAppControllerTest,
       SelectionNodeIdsContains_BackwardSelectionAndNearbyNodes) {
  ui::AXTreeUpdate update;
  SetUpdateTreeID(&update);
  update.has_tree_data = true;
  update.event_from = ax::mojom::EventFrom::kUser;
  update.tree_data.sel_anchor_object_id = 3;
  update.tree_data.sel_focus_object_id = 2;
  update.tree_data.sel_anchor_offset = 0;
  update.tree_data.sel_focus_offset = 0;
  update.tree_data.sel_is_backward = true;
  AccessibilityEventReceived({update});
  EXPECT_TRUE(SelectionNodeIdsContains(1));
  EXPECT_TRUE(SelectionNodeIdsContains(2));
  EXPECT_TRUE(SelectionNodeIdsContains(3));
  EXPECT_TRUE(SelectionNodeIdsContains(4));
}

TEST_F(ReadAnythingAppControllerTest, DisplayNodeIdsContains_ContentNodes) {
  ui::AXTreeUpdate update;
  SetUpdateTreeID(&update);
  update.nodes.resize(1);
  update.nodes[0].id = 3;
  // This update says the page loaded. When the controller receives it in
  // AccessibilityEventReceived, it will re-distill the tree. This is an
  // example of a non-generated event.
  EXPECT_CALL(*distiller_, Distill).Times(1);
  ui::AXEvent load_complete(0, ax::mojom::Event::kLoadComplete);
  AccessibilityEventReceived({update}, {load_complete});
  OnAXTreeDistilled({3});
  EXPECT_TRUE(DisplayNodeIdsContains(1));
  EXPECT_FALSE(DisplayNodeIdsContains(2));
  EXPECT_TRUE(DisplayNodeIdsContains(3));
  Mock::VerifyAndClearExpectations(distiller_);
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

TEST_F(ReadAnythingAppControllerTest,
       AccessibilityEventReceivedWhileDistilling) {
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

  // Send three updates while distilling.
  SetDistillationInProgress(true);
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
  // The updates shouldn't be applied yet.
  EXPECT_EQ("Hello world", GetTextContent(1));
  EXPECT_EQ("Hello world", GetTextContent(2));

  // Send another update after distillation finishes but before
  // OnAXTreeDistilled would unserialize the pending updates. Since a11y events
  // happen asynchronously, they can come between the time distillation finishes
  // and pending updates are unserialized.
  SetDistillationInProgress(false);
  ui::AXTreeUpdate update_2;
  SetUpdateTreeID(&update_2);
  update_2.nodes.resize(1);
  update_2.nodes[0].id = 2;
  update_2.nodes[0].role = ax::mojom::Role::kStaticText;
  update_2.nodes[0].SetName("Final update");
  update_2.nodes[0].SetNameFrom(ax::mojom::NameFrom::kContents);
  AccessibilityEventReceived({update_2});

  EXPECT_EQ("Final updateNode 3Node 4", GetTextContent(1));
  EXPECT_EQ("Final update", GetTextContent(2));
  EXPECT_EQ("Node 3", GetTextContent(3));
  EXPECT_EQ("Node 4", GetTextContent(4));
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
  // Check that changing the active tree ID changes the active tree which is
  // used when using a v8 getter.
  for (int i = 0; i < 3; i++) {
    AccessibilityEventReceived({updates[i]});
    OnAXTreeDistilled({1});
    EXPECT_CALL(*distiller_, Distill).Times(1);
    OnActiveAXTreeIDChanged(tree_ids[i]);
    EXPECT_EQ("Tree " + base::NumberToString(i), GetTextContent(1));
    Mock::VerifyAndClearExpectations(distiller_);
  }

  // Changing the active tree ID to the same ID does nothing.
  EXPECT_CALL(*distiller_, Distill).Times(0);
  OnActiveAXTreeIDChanged(tree_ids[2]);
  Mock::VerifyAndClearExpectations(distiller_);
}

TEST_F(ReadAnythingAppControllerTest,
       OnActiveAXTreeIDChanged_DocsLabeledNotSelectable) {
  ui::AXTreeUpdate update;
  ui::AXTreeID id_1 = ui::AXTreeID::CreateNewAXTreeID();
  SetUpdateTreeID(&update, id_1);
  update.root_id = 1;
  update.nodes.resize(1);
  update.nodes[0].id = 1;
  AccessibilityEventReceived({update});
  OnAXTreeDistilled({1});

  EXPECT_CALL(*distiller_, Distill).Times(1);
  OnActiveAXTreeIDChanged(id_1, GURL("www.google.com"));
  EXPECT_TRUE(isSelectable());
  Mock::VerifyAndClearExpectations(distiller_);

  ui::AXTreeUpdate update_1;
  SetUpdateTreeID(&update_1, tree_id_);
  update_1.root_id = 1;
  update_1.nodes.resize(1);
  update_1.nodes[0].id = 1;
  AccessibilityEventReceived({update_1});
  OnAXTreeDistilled({1});

  EXPECT_CALL(*distiller_, Distill).Times(1);
  OnActiveAXTreeIDChanged(
      tree_id_, GURL("https://docs.google.com/document/d/"
                     "1t6x1PQaQWjE8wb9iyYmFaoK1XAEgsl8G1Hx3rzfpoKA/"
                     "edit?ouid=103677288878638916900&usp=docs_home&ths=true"));
  EXPECT_FALSE(isSelectable());
  Mock::VerifyAndClearExpectations(distiller_);
}

TEST_F(ReadAnythingAppControllerTest, DoesNotCrashIfActiveAXTreeIDUnknown) {
  EXPECT_CALL(*distiller_, Distill).Times(0);
  ui::AXTreeID tree_id = ui::AXTreeIDUnknown();
  OnActiveAXTreeIDChanged(tree_id);
  OnAXTreeDestroyed(tree_id);
  OnAXTreeDistilled({1});
  Mock::VerifyAndClearExpectations(distiller_);
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
  ASSERT_TRUE(HasTree(tree_id_));

  // Add the two trees.
  AccessibilityEventReceived({updates[0]});
  ASSERT_TRUE(HasTree(tree_id_));
  ASSERT_TRUE(HasTree(tree_ids[0]));
  AccessibilityEventReceived({updates[1]});
  ASSERT_TRUE(HasTree(tree_id_));
  ASSERT_TRUE(HasTree(tree_ids[0]));
  ASSERT_TRUE(HasTree(tree_ids[1]));

  // Remove all of the trees.
  OnAXTreeDestroyed(tree_id_);
  ASSERT_FALSE(HasTree(tree_id_));
  ASSERT_TRUE(HasTree(tree_ids[0]));
  ASSERT_TRUE(HasTree(tree_ids[1]));
  OnAXTreeDestroyed(tree_ids[0]);
  ASSERT_FALSE(HasTree(tree_ids[0]));
  ASSERT_TRUE(HasTree(tree_ids[1]));
  OnAXTreeDestroyed(tree_ids[1]);
  ASSERT_FALSE(HasTree(tree_ids[1]));
}

TEST_F(ReadAnythingAppControllerTest, OnAXTreeDestroyed_EraseTreeCalled) {
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
  Mock::VerifyAndClearExpectations(distiller_);

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

  // Send update 0.
  EXPECT_CALL(*distiller_, Distill).Times(0);
  AccessibilityEventReceived({updates[0]});
  EXPECT_EQ("2345", GetTextContent(1));
  Mock::VerifyAndClearExpectations(distiller_);

  // Send update 1.
  EXPECT_CALL(*distiller_, Distill).Times(0);
  AccessibilityEventReceived({updates[1]});
  EXPECT_EQ("23456", GetTextContent(1));
  Mock::VerifyAndClearExpectations(distiller_);

  // Destroy the tree.
  ASSERT_TRUE(HasTree(tree_id_));
  OnAXTreeDestroyed(tree_id_);
  ASSERT_FALSE(HasTree(tree_id_));
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
  // No events we care about come about, so there's no distillation.
  EXPECT_CALL(*distiller_, Distill).Times(0);
  AccessibilityEventReceived({initial_update});
  EXPECT_EQ("234", GetTextContent(1));
  Mock::VerifyAndClearExpectations(distiller_);

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

  // Send update 0. Data gets unserialized.
  EXPECT_CALL(*distiller_, Distill).Times(0);
  AccessibilityEventReceived({updates[0]});
  EXPECT_EQ("2345", GetTextContent(1));
  Mock::VerifyAndClearExpectations(distiller_);

  // Send update 1. This triggers distillation via a non-generated event. The
  // data is also unserialized.
  EXPECT_CALL(*distiller_, Distill).Times(1);
  ui::AXEvent load_complete_1(1, ax::mojom::Event::kLoadComplete);
  AccessibilityEventReceived({updates[1]}, {load_complete_1});
  EXPECT_EQ("23456", GetTextContent(1));
  Mock::VerifyAndClearExpectations(distiller_);

  // Send update 2. Distillation is still in progress; we get a non-generated
  // event. This does not result in distillation (yet). The data is not
  // unserialized.
  EXPECT_CALL(*distiller_, Distill).Times(0);
  ui::AXEvent load_complete_2(2, ax::mojom::Event::kLoadComplete);
  AccessibilityEventReceived({updates[2]}, {load_complete_2});
  EXPECT_EQ("23456", GetTextContent(1));
  Mock::VerifyAndClearExpectations(distiller_);

  // Complete distillation. The queued up tree update gets unserialized; we also
  // request distillation (deferred from above) with state
  // `requires_distillation_` from the model.
  EXPECT_CALL(*distiller_, Distill).Times(1);
  OnAXTreeDistilled({1});
  EXPECT_EQ("234567", GetTextContent(1));
  Mock::VerifyAndClearExpectations(distiller_);
}

TEST_F(ReadAnythingAppControllerTest,
       AccessibilityReceivedAfterDistillingOnSameTree_DoesNotCrash) {
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
  Mock::VerifyAndClearExpectations(distiller_);

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

  // Send update 0, which starts distillation because of the load complete.
  EXPECT_CALL(*distiller_, Distill).Times(1);
  ui::AXEvent load_complete(1, ax::mojom::Event::kLoadComplete);
  AccessibilityEventReceived({updates[0]}, {load_complete});
  Mock::VerifyAndClearExpectations(distiller_);

  // Send update 1. Since there's no event (generated or not) which triggers
  // distllation, we have no calls.
  EXPECT_CALL(*distiller_, Distill).Times(0);
  AccessibilityEventReceived({updates[1]});
  Mock::VerifyAndClearExpectations(distiller_);

  // Ensure that there are no crashes after an accessibility event is received
  // immediately after distilling.
  EXPECT_CALL(*distiller_, Distill).Times(0);
  OnAXTreeDistilled({1});
  SetDistillationInProgress(true);
  AccessibilityEventReceived({updates[2]});
  Mock::VerifyAndClearExpectations(distiller_);
}

TEST_F(ReadAnythingAppControllerTest,
       DistillationInProgress_ActiveTreeIDChanges) {
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

  EXPECT_CALL(*distiller_, Distill).Times(0);
  AccessibilityEventReceived({updates[0]});
  Mock::VerifyAndClearExpectations(distiller_);

  EXPECT_CALL(*distiller_, Distill).Times(1);
  ui::AXEvent load_complete(1, ax::mojom::Event::kLoadComplete);
  AccessibilityEventReceived({updates[1]}, {load_complete});
  Mock::VerifyAndClearExpectations(distiller_);

  EXPECT_CALL(*distiller_, Distill).Times(0);
  AccessibilityEventReceived({updates[2]});
  EXPECT_EQ("56", GetTextContent(1));
  Mock::VerifyAndClearExpectations(distiller_);

  // Calling OnActiveAXTreeID updates the active AXTreeID.
  ui::AXTreeID tree_id_2 = ui::AXTreeID::CreateNewAXTreeID();
  EXPECT_CALL(*distiller_, Distill).Times(0);
  ASSERT_EQ(tree_id_, ActiveTreeId());
  OnActiveAXTreeIDChanged(tree_id_2);
  ASSERT_EQ(tree_id_2, ActiveTreeId());
  Mock::VerifyAndClearExpectations(distiller_);
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
  EXPECT_CALL(*distiller_, Distill).Times(0);
  AccessibilityEventReceived({updates[0]});
  AccessibilityEventReceived(tree_id_, {updates[1], updates[2]});
  Mock::VerifyAndClearExpectations(distiller_);

  // Switch to a new active tree. Should not crash.
  EXPECT_CALL(*distiller_, Distill).Times(0);
  OnActiveAXTreeIDChanged(ui::AXTreeIDUnknown());
  Mock::VerifyAndClearExpectations(distiller_);
}

TEST_F(ReadAnythingAppControllerTest, OnLinkClicked) {
  ui::AXNodeID ax_node_id = 2;
  EXPECT_CALL(page_handler_, OnLinkClicked(tree_id_, ax_node_id)).Times(1);
  OnLinkClicked(ax_node_id);
  page_handler_.FlushForTesting();
  Mock::VerifyAndClearExpectations(distiller_);
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
  Mock::VerifyAndClearExpectations(distiller_);

  // If distillation is in progress, OnLinkClicked should not be called.
  EXPECT_CALL(page_handler_, OnLinkClicked).Times(0);
  OnLinkClicked(2);
  page_handler_.FlushForTesting();
  Mock::VerifyAndClearExpectations(distiller_);
}

TEST_F(ReadAnythingAppControllerTest, OnSelectionChange) {
  ui::AXTreeUpdate update;
  SetUpdateTreeID(&update);
  update.nodes.resize(3);
  update.nodes[0].id = 2;
  update.nodes[1].id = 3;
  update.nodes[2].id = 4;
  update.nodes[0].role = ax::mojom::Role::kStaticText;
  update.nodes[1].role = ax::mojom::Role::kStaticText;
  update.nodes[2].role = ax::mojom::Role::kStaticText;
  AccessibilityEventReceived({update});
  ui::AXNodeID anchor_node_id = 2;
  int anchor_offset = 0;
  ui::AXNodeID focus_node_id = 3;
  int focus_offset = 1;
  EXPECT_CALL(page_handler_,
              OnSelectionChange(tree_id_, anchor_node_id, anchor_offset,
                                focus_node_id, focus_offset))
      .Times(1);
  OnSelectionChange(anchor_node_id, anchor_offset, focus_node_id, focus_offset);
  Mock::VerifyAndClearExpectations(distiller_);
}

TEST_F(ReadAnythingAppControllerTest,
       OnSelectionChange_ClickAfterClickDoesNotUpdateSelection) {
  ui::AXTreeUpdate update;
  SetUpdateTreeID(&update);
  update.nodes.resize(2);
  update.nodes[0].id = 2;
  update.nodes[1].id = 3;
  update.nodes[0].role = ax::mojom::Role::kStaticText;
  update.nodes[1].role = ax::mojom::Role::kStaticText;
  AccessibilityEventReceived({update});

  ui::AXTreeUpdate selection;
  SetUpdateTreeID(&selection);
  selection.has_tree_data = true;
  selection.event_from = ax::mojom::EventFrom::kUser;
  selection.tree_data.sel_anchor_object_id = 2;
  selection.tree_data.sel_focus_object_id = 2;
  selection.tree_data.sel_anchor_offset = 0;
  selection.tree_data.sel_focus_offset = 0;
  AccessibilityEventReceived({selection});

  EXPECT_CALL(page_handler_, OnSelectionChange).Times(0);
  OnSelectionChange(3, 5, 3, 5);
  page_handler_.FlushForTesting();
}

TEST_F(ReadAnythingAppControllerTest,
       OnSelectionChange_ClickAfterSelectionClearsSelection) {
  ui::AXTreeUpdate update;
  SetUpdateTreeID(&update);
  update.nodes.resize(2);
  update.nodes[0].id = 2;
  update.nodes[1].id = 3;
  update.nodes[0].role = ax::mojom::Role::kStaticText;
  update.nodes[1].role = ax::mojom::Role::kStaticText;
  AccessibilityEventReceived({update});

  ui::AXTreeUpdate selection;
  SetUpdateTreeID(&selection);
  selection.has_tree_data = true;
  selection.event_from = ax::mojom::EventFrom::kUser;
  selection.tree_data.sel_anchor_object_id = 2;
  selection.tree_data.sel_focus_object_id = 3;
  selection.tree_data.sel_anchor_offset = 0;
  selection.tree_data.sel_focus_offset = 1;
  AccessibilityEventReceived({selection});

  ui::AXNodeID anchor_node_id = 3;
  int anchor_offset = 5;
  ui::AXNodeID focus_node_id = 3;
  int focus_offset = 5;
  EXPECT_CALL(page_handler_,
              OnSelectionChange(tree_id_, anchor_node_id, anchor_offset,
                                focus_node_id, focus_offset))
      .Times(1);
  OnSelectionChange(anchor_node_id, anchor_offset, focus_node_id, focus_offset);
  page_handler_.FlushForTesting();
  Mock::VerifyAndClearExpectations(distiller_);
}

TEST_F(ReadAnythingAppControllerTest,
       OnSelectionChange_DistillationInProgress) {
  ui::AXTreeID new_tree_id = ui::AXTreeID::CreateNewAXTreeID();
  ui::AXTreeUpdate update;
  SetUpdateTreeID(&update, new_tree_id);
  update.root_id = 1;
  update.nodes.resize(1);
  update.nodes[0].role = ax::mojom::Role::kStaticText;
  update.nodes[0].id = 1;
  AccessibilityEventReceived({update});
  EXPECT_CALL(*distiller_, Distill).Times(1);
  OnActiveAXTreeIDChanged(new_tree_id);
  Mock::VerifyAndClearExpectations(distiller_);

  // If distillation is in progress, OnSelectionChange should not be called.
  EXPECT_CALL(page_handler_, OnSelectionChange).Times(0);
  OnSelectionChange(2, 0, 3, 1);
  page_handler_.FlushForTesting();
  Mock::VerifyAndClearExpectations(distiller_);
}

TEST_F(ReadAnythingAppControllerTest,
       OnSelectionChange_NonTextFieldDoesNotUpdateSelection) {
  ui::AXTreeUpdate update;
  SetUpdateTreeID(&update);
  update.nodes.resize(3);
  update.nodes[0].id = 2;
  update.nodes[1].id = 3;
  update.nodes[2].id = 4;
  update.nodes[0].role = ax::mojom::Role::kTextField;
  update.nodes[1].role = ax::mojom::Role::kGenericContainer;
  update.nodes[2].role = ax::mojom::Role::kTextField;
  AccessibilityEventReceived({update});
  ui::AXNodeID anchor_node_id = 2;
  int anchor_offset = 0;
  ui::AXNodeID focus_node_id = 3;
  int focus_offset = 1;
  EXPECT_CALL(page_handler_,
              OnSelectionChange(tree_id_, anchor_node_id, anchor_offset,
                                focus_node_id, focus_offset))
      .Times(0);
  OnSelectionChange(anchor_node_id, anchor_offset, focus_node_id, focus_offset);
  page_handler_.FlushForTesting();
  Mock::VerifyAndClearExpectations(distiller_);
}

TEST_F(ReadAnythingAppControllerTest, Selection_Forward) {
  // Create selection from node 3-4.
  ui::AXTreeUpdate update;
  SetUpdateTreeID(&update);
  update.has_tree_data = true;
  update.event_from = ax::mojom::EventFrom::kUser;
  update.tree_data.sel_anchor_object_id = 3;
  update.tree_data.sel_focus_object_id = 4;
  update.tree_data.sel_anchor_offset = 0;
  update.tree_data.sel_focus_offset = 1;
  update.tree_data.sel_is_backward = false;
  AccessibilityEventReceived({update});
  EXPECT_EQ(3, StartNodeId());
  EXPECT_EQ(4, EndNodeId());
  EXPECT_EQ(0, StartOffset());
  EXPECT_EQ(1, EndOffset());
}

TEST_F(ReadAnythingAppControllerTest, Selection_Backward) {
  // Create backward selection from node 4-3.
  ui::AXTreeUpdate update;
  SetUpdateTreeID(&update);
  update.has_tree_data = true;
  update.event_from = ax::mojom::EventFrom::kUser;
  update.tree_data.sel_anchor_object_id = 4;
  update.tree_data.sel_focus_object_id = 3;
  update.tree_data.sel_anchor_offset = 1;
  update.tree_data.sel_focus_offset = 0;
  update.tree_data.sel_is_backward = true;
  AccessibilityEventReceived({update});
  EXPECT_EQ(3, StartNodeId());
  EXPECT_EQ(4, EndNodeId());
  EXPECT_EQ(0, StartOffset());
  EXPECT_EQ(1, EndOffset());
}

TEST_F(ReadAnythingAppControllerTest, Selection_IgnoredNode) {
  // Make 4 ignored and give 3 some text content.
  ui::AXTreeUpdate update;
  SetUpdateTreeID(&update);
  update.root_id = 1;
  update.nodes.resize(2);
  update.nodes[0].id = 3;
  update.nodes[1].id = 4;
  update.nodes[0].role = ax::mojom::Role::kStaticText;
  update.nodes[0].SetName("Hello");
  update.nodes[0].SetNameFrom(ax::mojom::NameFrom::kContents);
  update.nodes[1].role = ax::mojom::Role::kNone;  // This node is ignored.
  AccessibilityEventReceived({update});
  OnAXTreeDistilled({});

  // Create selection from node 2-4, where 4 is ignored.
  ui::AXTreeUpdate update_2;
  SetUpdateTreeID(&update_2);
  update_2.tree_data.sel_anchor_object_id = 2;
  update_2.tree_data.sel_focus_object_id = 4;
  update_2.tree_data.sel_anchor_offset = 0;
  update_2.tree_data.sel_focus_offset = 0;
  update_2.tree_data.sel_is_backward = false;
  AccessibilityEventReceived({update_2});
  OnAXTreeDistilled({});

  // We want to check that no crash occurs in this case. These node ids and
  // offset are incorrect, they should be 2, 3, 0, and 5 (5 is the length of the
  // world "Hello"). But because we don't have an AXTreeManager in the test,
  // AXSelection::ToUnignoredSelection() exits early without calculating the
  // actual ignored selection.
  EXPECT_EQ(2, StartNodeId());
  EXPECT_EQ(4, EndNodeId());
  EXPECT_EQ(0, StartOffset());
  EXPECT_EQ(0, EndOffset());
}

TEST_F(ReadAnythingAppControllerTest, Selection_IsCollapsed) {
  ui::AXTreeUpdate update;
  SetUpdateTreeID(&update);
  update.has_tree_data = true;
  update.event_from = ax::mojom::EventFrom::kUser;
  update.tree_data.sel_anchor_object_id = 2;
  update.tree_data.sel_focus_object_id = 2;
  update.tree_data.sel_anchor_offset = 3;
  update.tree_data.sel_focus_offset = 3;
  AccessibilityEventReceived({update});
  EXPECT_EQ(ui::kInvalidAXNodeID, StartNodeId());
  EXPECT_EQ(ui::kInvalidAXNodeID, EndNodeId());
  EXPECT_EQ(-1, StartOffset());
  EXPECT_EQ(-1, EndOffset());
  EXPECT_EQ(false, HasSelection());
}
