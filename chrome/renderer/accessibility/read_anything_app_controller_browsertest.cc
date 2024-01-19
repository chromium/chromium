// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/accessibility/read_anything_app_controller.h"

#include <cstddef>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "chrome/renderer/accessibility/ax_tree_distiller.h"
#include "chrome/test/base/chrome_render_view_test.h"
#include "content/public/renderer/render_frame.h"
#include "read_anything_app_controller.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/accessibility/ax_node.h"
#include "ui/accessibility/ax_serializable_tree.h"
#include "url/gurl.h"

class MockAXTreeDistiller : public AXTreeDistiller {
 public:
  explicit MockAXTreeDistiller(content::RenderFrame* render_frame)
      : AXTreeDistiller(base::NullCallback()) {}
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
  MOCK_METHOD(void, OnCollapseSelection, (), (override));
  MOCK_METHOD(void, OnCopy, (), (override));
  MOCK_METHOD(void,
              EnablePDFContentAccessibility,
              (const ui::AXTreeID& ax_tree_id),
              (override));
  MOCK_METHOD(void,
              OnLineSpaceChange,
              (read_anything::mojom::LineSpacing line_spacing),
              (override));
  MOCK_METHOD(void,
              OnLetterSpaceChange,
              (read_anything::mojom::LetterSpacing letter_spacing),
              (override));
  MOCK_METHOD(void, OnFontChange, (const std::string& font), (override));
  MOCK_METHOD(void, OnFontSizeChange, (double font_size), (override));
  MOCK_METHOD(void, OnSpeechRateChange, (double rate), (override));
  MOCK_METHOD(void,
              OnVoiceChange,
              (const std::string& voice, const std::string& lang),
              (override));
  MOCK_METHOD(void,
              OnColorChange,
              (read_anything::mojom::Colors color),
              (override));
  MOCK_METHOD(void,
              OnHighlightGranularityChanged,
              (read_anything::mojom::HighlightGranularity color),
              (override));

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
    ui::AXNodeData root;
    root.id = 1;

    ui::AXNodeData child1;
    child1.id = 2;

    ui::AXNodeData child2;
    child2.id = 3;

    ui::AXNodeData child3;
    child3.id = 4;

    root.child_ids = {child1.id, child2.id, child3.id};
    snapshot.root_id = root.id;
    snapshot.nodes = {root, child1, child2, child3};
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

  ui::AXTreeID SetUpPdfTrees() {
    // Call OnActiveAXTreeIDChanged() to set is_pdf_ state.
    GURL pdf_url("http://www.google.com/foo/bar.pdf");
    OnActiveAXTreeIDChanged(tree_id_, pdf_url, true);

    // PDF set up required for formatting checks.
    ui::AXTreeID pdf_iframe_tree_id = ui::AXTreeID::CreateNewAXTreeID();
    ui::AXTreeID pdf_web_contents_tree_id = ui::AXTreeID::CreateNewAXTreeID();

    // Send update for main web content with child tree (pdf web contents).
    ui::AXTreeUpdate main_web_contents_update;
    SetUpdateTreeID(&main_web_contents_update);
    ui::AXNodeData node;
    node.id = 1;
    node.AddChildTreeId(pdf_web_contents_tree_id);
    main_web_contents_update.nodes = {node};
    AccessibilityEventReceived({main_web_contents_update});

    // Send update for pdf web contents with child tree (iframe).
    ui::AXTreeUpdate pdf_web_contents_update;
    ui::AXNodeData pdf_node;
    pdf_node.id = 1;
    pdf_node.AddChildTreeId(pdf_iframe_tree_id);
    pdf_web_contents_update.nodes = {pdf_node};
    pdf_web_contents_update.root_id = pdf_node.id;
    SetUpdateTreeID(&pdf_web_contents_update, pdf_web_contents_tree_id);
    AccessibilityEventReceived({pdf_web_contents_update});

    return pdf_iframe_tree_id;
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
                          bool links_enabled,
                          SkColor foreground_color,
                          SkColor background_color,
                          int line_spacing,
                          int letter_spacing) {
    controller_->SetThemeForTesting(font_name, font_size, links_enabled,
                                    foreground_color, background_color,
                                    line_spacing, letter_spacing);
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

  void OnActiveAXTreeIDChanged(const ui::AXTreeID& tree_id,
                               const GURL& url,
                               bool force_update_state = false) {
    controller_->OnActiveAXTreeIDChanged(tree_id, ukm::kInvalidSourceId, url,
                                         force_update_state);
  }

  void OnAXTreeDistilled(const std::vector<ui::AXNodeID>& content_node_ids) {
    OnAXTreeDistilled(tree_id_, content_node_ids);
  }

  void InitAXPosition(const ui::AXNodeID id) {
    controller_->InitAXPositionWithNode(id);
  }

  ui::AXNodePosition::AXPositionInstance GetNextNodePosition() {
    return controller_->GetNextValidPositionFromCurrentPosition(
        ReadAnythingAppController::ReadAloudCurrentGranularity());
  }

  ui::AXNodePosition::AXPositionInstance GetNextNodePosition(
      ReadAnythingAppController::ReadAloudCurrentGranularity granularity) {
    return controller_->GetNextValidPositionFromCurrentPosition(granularity);
  }

  std::vector<ui::AXNodeID> GetNextText() {
    return controller_->GetNextText(160);
  }

  ReadAnythingAppController::ReadAloudCurrentGranularity GetNextNodes() {
    return controller_->GetNextNodes(160);
  }

  std::vector<ui::AXNodeID> GetPreviousText() {
    return controller_->GetPreviousText(160);
  }

  int GetNextTextStartIndex(ui::AXNodeID id) {
    return controller_->GetNextTextStartIndex(id);
  }

  int GetNextTextEndIndex(ui::AXNodeID id) {
    return controller_->GetNextTextEndIndex(id);
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

  bool LinksEnabled() { return controller_->LinksEnabled(); }

  SkColor ForegroundColor() { return controller_->ForegroundColor(); }

  SkColor BackgroundColor() { return controller_->BackgroundColor(); }

  float LineSpacing() { return controller_->LineSpacing(); }

  float LetterSpacing() { return controller_->LetterSpacing(); }

  bool isSelectable() { return controller_->IsSelectable(); }

  void OnFontSizeReset() { controller_->OnFontSizeReset(); }

  void TurnedHighlightOn() { controller_->TurnedHighlightOn(); }

  void TurnedHighlightOff() { controller_->TurnedHighlightOff(); }

  std::vector<ui::AXNodeID> GetChildren(ui::AXNodeID ax_node_id) {
    return controller_->GetChildren(ax_node_id);
  }

  std::string GetDataFontCss(ui::AXNodeID ax_node_id) {
    return controller_->GetDataFontCss(ax_node_id);
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

  bool IsGoogleDocs() { return controller_->IsGoogleDocs(); }

  bool IsLeafNode(ui::AXNodeID ax_node_id) {
    return controller_->IsLeafNode(ax_node_id);
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

  void OnCollapseSelection() { controller_->OnCollapseSelection(); }

  bool IsNodeIgnoredForReadAnything(ui::AXNodeID ax_node_id) {
    return controller_->model_.IsNodeIgnoredForReadAnything(ax_node_id);
  }

  bool HasTree(ui::AXTreeID tree_id) {
    return controller_->model_.ContainsTree(tree_id);
  }

  ui::AXTreeID ActiveTreeId() { return controller_->model_.GetActiveTreeId(); }

  size_t GetNextSentence(const std::u16string& text, size_t max_text_length) {
    return controller_->GetNextSentence(text, max_text_length);
  }

  std::string LanguageCodeForSpeech() {
    return controller_->GetLanguageCodeForSpeech();
  }
  void SetLanguageCode(std::string code) {
    controller_->SetLanguageForTesting(code);
  }

  ui::AXTreeID tree_id_;
  raw_ptr<MockAXTreeDistiller, DanglingUntriaged> distiller_ = nullptr;
  testing::StrictMock<MockReadAnythingUntrustedPageHandler> page_handler_;

 private:
  // ReadAnythingAppController constructor and destructor are private so it's
  // not accessible by std::make_unique.
  raw_ptr<ReadAnythingAppController, DanglingUntriaged> controller_ = nullptr;
};

TEST_F(ReadAnythingAppControllerTest, Theme) {
  std::string font_name = "Roboto";
  float font_size = 18.0;
  bool links_enabled = false;
  SkColor foreground = SkColorSetRGB(0x33, 0x36, 0x39);
  SkColor background = SkColorSetRGB(0xFD, 0xE2, 0x93);
  int letter_spacing =
      static_cast<int>(read_anything::mojom::LetterSpacing::kDefaultValue);
  float letter_spacing_value = 0.0;
  int line_spacing =
      static_cast<int>(read_anything::mojom::LineSpacing::kDefaultValue);
  float line_spacing_value = 1.5;
  SetThemeForTesting(font_name, font_size, links_enabled, foreground,
                     background, line_spacing, letter_spacing);
  EXPECT_EQ(font_name, FontName());
  EXPECT_EQ(font_size, FontSize());
  EXPECT_EQ(links_enabled, LinksEnabled());
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
  ui::AXNodeData node;
  node.id = 3;
  node.role = ax::mojom::Role::kNone;
  update.nodes = {node};
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
  ui::AXNodeData node;
  node.id = 3;
  node.role = ax::mojom::Role::kNone;
  update.nodes = {node};
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
  ui::AXNodeData span_node;
  span_node.id = 2;
  span_node.AddStringAttribute(ax::mojom::StringAttribute::kHtmlTag, span);

  ui::AXNodeData h1_node;
  h1_node.id = 3;
  h1_node.AddStringAttribute(ax::mojom::StringAttribute::kHtmlTag, h1);

  ui::AXNodeData ul_node;
  ul_node.id = 4;
  ul_node.AddStringAttribute(ax::mojom::StringAttribute::kHtmlTag, ul);
  update.nodes = {span_node, h1_node, ul_node};

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
  ui::AXNodeData span_node;
  span_node.id = 2;
  span_node.AddStringAttribute(ax::mojom::StringAttribute::kHtmlTag, span);

  ui::AXNodeData h1_node;
  h1_node.id = 3;
  h1_node.AddStringAttribute(ax::mojom::StringAttribute::kHtmlTag, h1);
  h1_node.role = ax::mojom::Role::kTextField;

  ui::AXNodeData ul_node;
  ul_node.id = 4;
  ul_node.AddStringAttribute(ax::mojom::StringAttribute::kHtmlTag, ul);
  ul_node.role = ax::mojom::Role::kTextFieldWithComboBox;
  update.nodes = {span_node, h1_node, ul_node};

  AccessibilityEventReceived({update});
  OnAXTreeDistilled({});
  EXPECT_EQ(span, GetHtmlTag(2));
  EXPECT_EQ(div, GetHtmlTag(3));
  EXPECT_EQ(div, GetHtmlTag(4));
}

TEST_F(ReadAnythingAppControllerTest, GetHtmlTag_SvgReturnsDivIfGoogleDocs) {
  std::string svg = "svg";
  std::string div = "div";
  ui::AXTreeUpdate update;
  ui::AXTreeID id_1 = ui::AXTreeID::CreateNewAXTreeID();
  SetUpdateTreeID(&update, id_1);
  ui::AXNodeData node;
  node.id = 2;
  node.AddStringAttribute(ax::mojom::StringAttribute::kHtmlTag, svg);

  ui::AXNodeData root;
  root.id = 1;
  root.child_ids = {node.id};
  update.nodes = {root, node};
  update.root_id = root.id;

  AccessibilityEventReceived({update});
  OnAXTreeDistilled({});
  OnActiveAXTreeIDChanged(
      id_1, GURL("https://docs.google.com/document/d/"
                 "1t6x1PQaQWjE8wb9iyYmFaoK1XAEgsl8G1Hx3rzfpoKA/"
                 "edit?ouid=103677288878638916900&usp=docs_home&ths=true"));
  EXPECT_TRUE(IsGoogleDocs());
  EXPECT_EQ(div, GetHtmlTag(2));
}

TEST_F(ReadAnythingAppControllerTest,
       GetHtmlTag_paragraphWithTagGReturnsPIfGoogleDocs) {
  std::string g = "g";
  std::string p = "p";
  ui::AXTreeUpdate update;
  ui::AXTreeID id_1 = ui::AXTreeID::CreateNewAXTreeID();
  SetUpdateTreeID(&update, id_1);
  ui::AXNodeData paragraph_node;
  paragraph_node.id = 2;
  paragraph_node.role = ax::mojom::Role::kParagraph;
  paragraph_node.AddStringAttribute(ax::mojom::StringAttribute::kHtmlTag, g);

  ui::AXNodeData svg_node;
  svg_node.id = 3;
  svg_node.AddStringAttribute(ax::mojom::StringAttribute::kHtmlTag, g);

  ui::AXNodeData root;
  root.role = ax::mojom::Role::kParagraph;
  root.id = 1;
  root.child_ids = {paragraph_node.id, svg_node.id};
  update.root_id = root.id;
  update.nodes = {root, paragraph_node, svg_node};
  AccessibilityEventReceived({update});
  OnAXTreeDistilled({});
  OnActiveAXTreeIDChanged(
      id_1, GURL("https://docs.google.com/document/d/"
                 "1t6x1PQaQWjE8wb9iyYmFaoK1XAEgsl8G1Hx3rzfpoKA/"
                 "edit?ouid=103677288878638916900&usp=docs_home&ths=true"));
  EXPECT_TRUE(IsGoogleDocs());
  EXPECT_EQ("", GetHtmlTag(1));
  EXPECT_EQ(p, GetHtmlTag(2));
  EXPECT_EQ(g, GetHtmlTag(3));
}

TEST_F(ReadAnythingAppControllerTest,
       GetHtmlTag_DivWithHeadingAndAriaLevelReturnsH) {
  std::string h3 = "h3";
  std::string div = "div";
  ui::AXTreeUpdate update;
  SetUpdateTreeID(&update);
  ui::AXNodeData node1;
  node1.id = 2;

  ui::AXNodeData node2;
  node2.id = 3;
  node2.role = ax::mojom::Role::kHeading;
  node2.html_attributes.emplace_back("aria-level", "3");

  ui::AXNodeData node3;
  node3.id = 4;
  update.nodes = {node1, node2, node3};
  AccessibilityEventReceived({update});
  OnAXTreeDistilled({});
  EXPECT_EQ(h3, GetHtmlTag(3));
}

TEST_F(ReadAnythingAppControllerTest, GetHtmlTag_PDF) {
  ui::AXTreeID pdf_iframe_tree_id = SetUpPdfTrees();

  // Send pdf iframe update with html tags to test.
  ui::AXTreeUpdate update;
  SetUpdateTreeID(&update, pdf_iframe_tree_id);
  ui::AXNodeData node1;
  node1.id = 2;
  node1.AddStringAttribute(ax::mojom::StringAttribute::kHtmlTag, "h1");
  ui::AXNodeData node2;
  node2.id = 3;
  node2.role = ax::mojom::Role::kHeading;
  node2.html_attributes.emplace_back("aria-level", "2");

  ui::AXNodeData root;
  root.id = 1;
  root.child_ids = {node1.id, node2.id};
  root.role = ax::mojom::Role::kPdfRoot;
  update.root_id = root.id;
  update.nodes = {root, node1, node2};
  AccessibilityEventReceived({update});

  OnAXTreeDistilled({});
  EXPECT_CALL(page_handler_, EnablePDFContentAccessibility).Times(1);
  EXPECT_EQ("span", GetHtmlTag(1));
  EXPECT_EQ("h1", GetHtmlTag(2));
  EXPECT_EQ("h2", GetHtmlTag(3));
}

TEST_F(ReadAnythingAppControllerTest, GetHtmlTag_IncorrectlyFormattedPDF) {
  ui::AXTreeID pdf_iframe_tree_id = SetUpPdfTrees();

  // Send pdf iframe update with html tags to test. Two headings next to each
  // other should be spans. A heading that's too long should be turned into a
  // paragraph.
  ui::AXTreeUpdate update;
  SetUpdateTreeID(&update, pdf_iframe_tree_id);
  ui::AXNodeData heading_node1;
  heading_node1.id = 2;
  heading_node1.role = ax::mojom::Role::kHeading;
  heading_node1.AddStringAttribute(ax::mojom::StringAttribute::kHtmlTag, "h1");
  ui::AXNodeData heading_node2;
  heading_node2.id = 3;
  heading_node2.role = ax::mojom::Role::kHeading;
  heading_node2.AddStringAttribute(ax::mojom::StringAttribute::kHtmlTag, "h1");

  ui::AXNodeData link_node;
  link_node.id = 4;
  link_node.role = ax::mojom::Role::kLink;

  ui::AXNodeData aria_node;
  aria_node.id = 5;
  aria_node.role = ax::mojom::Role::kHeading;
  aria_node.html_attributes.emplace_back("aria-level", "1");
  aria_node.SetNameChecked(
      "Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do eiusmod "
      "tempor incididunt ut labore et dolore magna aliqua.");
  aria_node.SetNameFrom(ax::mojom::NameFrom::kContents);

  ui::AXNodeData root;
  root.id = 1;
  root.child_ids = {heading_node1.id, heading_node2.id, link_node.id,
                    aria_node.id};
  root.role = ax::mojom::Role::kPdfRoot;
  update.root_id = root.id;
  update.nodes = {root, heading_node1, heading_node2, link_node, aria_node};

  AccessibilityEventReceived({update});

  OnAXTreeDistilled({});
  EXPECT_CALL(page_handler_, EnablePDFContentAccessibility).Times(1);
  EXPECT_EQ("span", GetHtmlTag(2));
  EXPECT_EQ("span", GetHtmlTag(3));
  EXPECT_EQ("a", GetHtmlTag(4));
  EXPECT_EQ("p", GetHtmlTag(5));
}

TEST_F(ReadAnythingAppControllerTest, GetHtmlTag_InaccessiblePDF) {
  ui::AXTreeID pdf_iframe_tree_id = SetUpPdfTrees();

  // Send pdf iframe update with html tags to test.
  ui::AXTreeUpdate update;
  SetUpdateTreeID(&update, pdf_iframe_tree_id);
  ui::AXNodeData node;
  node.id = 2;
  node.role = ax::mojom::Role::kContentInfo;
  node.SetNameChecked(string_constants::kPDFPageEnd);
  node.SetNameFrom(ax::mojom::NameFrom::kContents);

  ui::AXNodeData root;
  root.id = 1;
  root.child_ids = {node.id};
  root.role = ax::mojom::Role::kPdfRoot;
  update.root_id = 1;
  update.nodes = {root, node};
  AccessibilityEventReceived({update});

  OnAXTreeDistilled({});
  EXPECT_CALL(page_handler_, EnablePDFContentAccessibility).Times(1);
  EXPECT_EQ("br", GetHtmlTag(2));
}

TEST_F(ReadAnythingAppControllerTest, GetTextContent_NoSelection) {
  std::string text_content = "Hello";
  std::string more_text_content = " world";
  ui::AXTreeUpdate update;
  SetUpdateTreeID(&update);
  ui::AXNodeData node1;
  node1.id = 2;
  node1.role = ax::mojom::Role::kStaticText;
  node1.SetNameChecked(text_content);

  ui::AXNodeData node2;
  node2.id = 3;
  node2.role = ax::mojom::Role::kStaticText;
  node2.SetNameExplicitlyEmpty();

  ui::AXNodeData node3;
  node3.id = 4;
  node3.role = ax::mojom::Role::kStaticText;
  node3.SetNameChecked(more_text_content);
  update.nodes = {node1, node2, node3};
  AccessibilityEventReceived({update});
  OnAXTreeDistilled({});
  EXPECT_EQ("Hello world", GetTextContent(1));
  EXPECT_EQ(text_content, GetTextContent(2));
  EXPECT_EQ("", GetTextContent(3));
  EXPECT_EQ(more_text_content, GetTextContent(4));
}

TEST_F(ReadAnythingAppControllerTest, GetTextContent_WithSelection) {
  std::string text_content_1 = "Hello";
  std::string text_content_2 = " world";
  std::string text_content_3 = " friend";
  ui::AXTreeUpdate update;
  SetUpdateTreeID(&update);
  ui::AXNodeData node1;
  node1.id = 2;
  node1.role = ax::mojom::Role::kStaticText;
  node1.SetNameChecked(text_content_1);

  ui::AXNodeData node2;
  node2.id = 3;
  node2.role = ax::mojom::Role::kStaticText;
  node2.SetNameChecked(text_content_2);

  ui::AXNodeData node3;
  node3.id = 4;
  node3.role = ax::mojom::Role::kStaticText;
  node3.SetNameChecked(text_content_3);
  update.nodes = {node1, node2, node3};

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

TEST_F(ReadAnythingAppControllerTest,
       GetTextContent_UseNameAttributeTextIfGoogleDocs) {
  std::string text_content = "Hello";
  std::string more_text_content = "world";
  ui::AXTreeUpdate update;
  ui::AXTreeID id_1 = ui::AXTreeID::CreateNewAXTreeID();
  SetUpdateTreeID(&update, id_1);
  ui::AXNodeData node1;
  node1.id = 2;
  node1.AddStringAttribute(ax::mojom::StringAttribute::kName, text_content);

  ui::AXNodeData node2;
  node2.id = 3;
  node2.AddStringAttribute(ax::mojom::StringAttribute::kName,
                           more_text_content);
  ui::AXNodeData root;
  root.id = 1;
  root.child_ids = {node1.id, node2.id};
  root.role = ax::mojom::Role::kParagraph;
  update.root_id = root.id;
  update.nodes = {root, node1, node2};

  AccessibilityEventReceived({update});
  OnAXTreeDistilled({});
  OnActiveAXTreeIDChanged(
      id_1, GURL("https://docs.google.com/document/d/"
                 "1t6x1PQaQWjE8wb9iyYmFaoK1XAEgsl8G1Hx3rzfpoKA/"
                 "edit?ouid=103677288878638916900&usp=docs_home&ths=true"));
  EXPECT_TRUE(IsGoogleDocs());
  EXPECT_EQ("Hello world", GetTextContent(1));
  EXPECT_EQ(text_content, GetTextContent(2));
  EXPECT_EQ(more_text_content, GetTextContent(3));
}

TEST_F(ReadAnythingAppControllerTest,
       GetTextContent_DoNotUseNameAttributeTextIfNotGoogleDocs) {
  std::string text_content = "Hello";
  std::string more_text_content = "world";
  ui::AXTreeUpdate update;
  ui::AXTreeID id_1 = ui::AXTreeID::CreateNewAXTreeID();
  SetUpdateTreeID(&update, id_1);
  ui::AXNodeData node1;
  node1.id = 2;
  node1.AddStringAttribute(ax::mojom::StringAttribute::kName, text_content);

  ui::AXNodeData node2;
  node2.id = 3;
  node2.AddStringAttribute(ax::mojom::StringAttribute::kName,
                           more_text_content);

  ui::AXNodeData root;
  root.id = 1;
  root.child_ids = {node1.id, node2.id};
  root.role = ax::mojom::Role::kParagraph;
  update.root_id = root.id;
  update.nodes = {root, node1, node2};

  AccessibilityEventReceived({update});
  OnAXTreeDistilled({});
  OnActiveAXTreeIDChanged(id_1, GURL("https://www.google.com/"));
  EXPECT_FALSE(IsGoogleDocs());
  EXPECT_EQ("", GetTextContent(1));
  EXPECT_EQ("", GetTextContent(2));
  EXPECT_EQ("", GetTextContent(3));
}

TEST_F(ReadAnythingAppControllerTest, GetUrl) {
  std::string http_url = "http://www.google.com";
  std::string https_url = "https://www.google.com";
  std::string invalid_url = "cats";
  std::string missing_url = "";
  std::string js = "javascript:alert(origin)";
  ui::AXTreeUpdate update;
  SetUpdateTreeID(&update);

  ui::AXNodeData node1;
  node1.id = 2;
  node1.AddStringAttribute(ax::mojom::StringAttribute::kUrl, http_url);

  ui::AXNodeData node2;
  node2.id = 3;
  node2.AddStringAttribute(ax::mojom::StringAttribute::kUrl, https_url);

  ui::AXNodeData node3;
  node3.id = 4;
  node3.AddStringAttribute(ax::mojom::StringAttribute::kUrl, invalid_url);

  ui::AXNodeData node4;
  node4.id = 5;
  node4.AddStringAttribute(ax::mojom::StringAttribute::kUrl, missing_url);

  ui::AXNodeData node5;
  node5.id = 6;
  node5.AddStringAttribute(ax::mojom::StringAttribute::kUrl, js);

  ui::AXNodeData root;
  root.id = 1;
  root.child_ids = {node1.id, node2.id, node3.id, node4.id, node5.id};
  update.nodes = {root, node1, node2, node3, node4, node5};

  AccessibilityEventReceived({update});
  OnAXTreeDistilled({});
  EXPECT_EQ(http_url, GetUrl(2));
  EXPECT_EQ(https_url, GetUrl(3));
  EXPECT_EQ("", GetUrl(4));
  EXPECT_EQ("", GetUrl(5));
  EXPECT_EQ("", GetUrl(6));
}

TEST_F(ReadAnythingAppControllerTest, ShouldBold) {
  ui::AXTreeUpdate update;
  SetUpdateTreeID(&update);
  ui::AXNodeData overline_node;
  overline_node.id = 2;
  overline_node.AddTextStyle(ax::mojom::TextStyle::kOverline);

  ui::AXNodeData underline_node;
  underline_node.id = 3;
  underline_node.AddTextStyle(ax::mojom::TextStyle::kUnderline);

  ui::AXNodeData italic_node;
  italic_node.id = 4;
  italic_node.AddTextStyle(ax::mojom::TextStyle::kItalic);
  update.nodes = {overline_node, underline_node, italic_node};

  AccessibilityEventReceived({update});
  OnAXTreeDistilled({});
  EXPECT_EQ(false, ShouldBold(2));
  EXPECT_EQ(true, ShouldBold(3));
  EXPECT_EQ(true, ShouldBold(4));
}

TEST_F(ReadAnythingAppControllerTest, GetDataFontCss) {
  std::string dataFontCss = "italic 400 14.6667px 'Courier New'";
  ui::AXTreeUpdate update;
  SetUpdateTreeID(&update);
  ui::AXNodeData node;
  node.id = 2;
  node.html_attributes.emplace_back("data-font-css", dataFontCss);
  update.nodes = {node};
  AccessibilityEventReceived({update});
  OnAXTreeDistilled({});
  EXPECT_EQ(dataFontCss, GetDataFontCss(2));
}

TEST_F(ReadAnythingAppControllerTest, IsOverline) {
  ui::AXTreeUpdate update;
  SetUpdateTreeID(&update);
  ui::AXNodeData overline_node;
  overline_node.id = 2;
  overline_node.AddTextStyle(ax::mojom::TextStyle::kOverline);

  ui::AXNodeData underline_node;
  underline_node.id = 3;
  underline_node.AddTextStyle(ax::mojom::TextStyle::kUnderline);
  update.nodes = {overline_node, underline_node};

  AccessibilityEventReceived({update});
  OnAXTreeDistilled({});
  EXPECT_EQ(true, IsOverline(2));
  EXPECT_EQ(false, IsOverline(3));
}

TEST_F(ReadAnythingAppControllerTest, IsLeafNode) {
  ui::AXTreeUpdate update;
  SetUpdateTreeID(&update);
  ui::AXNodeData node1;
  node1.id = 2;

  ui::AXNodeData node2;
  node2.id = 3;

  ui::AXNodeData node3;
  node3.id = 4;

  ui::AXNodeData parent;
  parent.id = 1;
  parent.child_ids = {node1.id, node2.id, node3.id};
  update.nodes = {parent, node1, node2, node3};

  AccessibilityEventReceived({update});
  OnAXTreeDistilled({});
  EXPECT_EQ(false, IsLeafNode(1));
  EXPECT_EQ(true, IsLeafNode(2));
  EXPECT_EQ(true, IsLeafNode(3));
  EXPECT_EQ(true, IsLeafNode(4));
}

TEST_F(ReadAnythingAppControllerTest, IsGoogleDocs) {
  ui::AXTreeID id_1 = ui::AXTreeID::CreateNewAXTreeID();
  OnActiveAXTreeIDChanged(id_1, GURL("www.google.com"));
  EXPECT_FALSE(IsGoogleDocs());

  OnActiveAXTreeIDChanged(
      tree_id_, GURL("https://docs.google.com/document/d/"
                     "1t6x1PQaQWjE8wb9iyYmFaoK1XAEgsl8G1Hx3rzfpoKA/"
                     "edit?ouid=103677288878638916900&usp=docs_home&ths=true"));
  EXPECT_TRUE(IsGoogleDocs());
}

TEST_F(ReadAnythingAppControllerTest, IsNodeIgnoredForReadAnything) {
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
  ui::AXNodeData node;
  node.id = 3;
  update.nodes = {node};
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
  ui::AXNodeData node;
  node.id = 2;
  node.role = ax::mojom::Role::kStaticText;
  node.SetNameChecked("Hello world");
  update_1.nodes = {node};
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
    ui::AXNodeData static_text_node;
    static_text_node.id = i;
    static_text_node.role = ax::mojom::Role::kStaticText;
    static_text_node.SetNameChecked("Node " + base::NumberToString(i));
    update.nodes = {static_text_node};
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
  ui::AXNodeData clearNode;
  clearNode.id = 1;
  clear_update.nodes = {clearNode};
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
  ui::AXNodeData start_node;
  start_node.id = 2;
  start_node.role = ax::mojom::Role::kStaticText;
  start_node.SetNameChecked("Hello world");
  update_1.nodes = {start_node};
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
    ui::AXNodeData node;
    node.id = i;
    node.role = ax::mojom::Role::kStaticText;
    node.SetNameChecked("Node " + base::NumberToString(i));
    update.nodes = {node};
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
  ui::AXNodeData final_node;
  final_node.id = 2;
  final_node.role = ax::mojom::Role::kStaticText;
  final_node.SetNameChecked("Final update");
  update_2.nodes = {final_node};
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
    ui::AXNodeData node;
    node.id = 1;
    node.role = ax::mojom::Role::kStaticText;
    node.SetNameChecked("Tree " + base::NumberToString(i));
    update.root_id = node.id;
    update.nodes = {node};
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
  ui::AXNodeData node;
  node.id = 1;
  update.nodes = {node};
  AccessibilityEventReceived({update});
  OnAXTreeDistilled({1});

  EXPECT_CALL(*distiller_, Distill).Times(1);
  OnActiveAXTreeIDChanged(id_1, GURL("www.google.com"));
  EXPECT_TRUE(isSelectable());
  Mock::VerifyAndClearExpectations(distiller_);

  ui::AXTreeUpdate update_1;
  SetUpdateTreeID(&update_1, tree_id_);
  ui::AXNodeData root;
  root.id = 1;
  update_1.root_id = root.id;
  update_1.nodes = {root};
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
    ui::AXNodeData node;
    node.id = 1;
    update.root_id = node.id;
    update.nodes = {node};
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
    initial_update.nodes[i].SetNameChecked(base::NumberToString(id));
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
    initial_update.nodes[i].SetNameChecked(base::NumberToString(id));
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
    initial_update.nodes[i].SetNameChecked(base::NumberToString(id));
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
  ui::AXNodeData generic_container_node;
  generic_container_node.id = 1;
  generic_container_node.role = ax::mojom::Role::kGenericContainer;
  update.nodes = {generic_container_node};
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
  ui::AXNodeData node;
  node.id = 1;
  update.root_id = node.id;
  update.nodes = {node};
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
  ui::AXNodeData node1;
  node1.id = 2;
  node1.role = ax::mojom::Role::kStaticText;

  ui::AXNodeData node2;
  node2.id = 3;
  node2.role = ax::mojom::Role::kStaticText;

  ui::AXNodeData node3;
  node3.id = 4;
  node3.role = ax::mojom::Role::kStaticText;
  update.nodes = {node1, node2, node3};
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

TEST_F(ReadAnythingAppControllerTest, OnCollapseSelection) {
  ui::AXTreeUpdate update;
  SetUpdateTreeID(&update);
  ui::AXNodeData node1;
  node1.id = 2;
  node1.role = ax::mojom::Role::kStaticText;

  ui::AXNodeData node2;
  node2.id = 3;
  node2.role = ax::mojom::Role::kStaticText;

  ui::AXNodeData node3;
  node3.id = 4;
  node3.role = ax::mojom::Role::kStaticText;
  update.nodes = {node1, node2, node3};
  AccessibilityEventReceived({update});
  EXPECT_CALL(page_handler_, OnCollapseSelection()).Times(1);
  OnCollapseSelection();
  Mock::VerifyAndClearExpectations(distiller_);
}

TEST_F(ReadAnythingAppControllerTest,
       OnSelectionChange_ClickAfterClickDoesNotUpdateSelection) {
  ui::AXTreeUpdate update;
  SetUpdateTreeID(&update);
  ui::AXNodeData node1;
  node1.id = 2;
  node1.role = ax::mojom::Role::kStaticText;

  ui::AXNodeData node2;
  node2.id = 3;
  node2.role = ax::mojom::Role::kStaticText;
  update.nodes = {node1, node2};
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
  ui::AXNodeData node1;
  node1.id = 2;
  node1.role = ax::mojom::Role::kStaticText;

  ui::AXNodeData node2;
  node2.id = 3;
  node2.role = ax::mojom::Role::kStaticText;
  update.nodes = {node1, node2};
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
  EXPECT_CALL(page_handler_, OnCollapseSelection()).Times(1);
  OnSelectionChange(anchor_node_id, anchor_offset, focus_node_id, focus_offset);
  page_handler_.FlushForTesting();
  Mock::VerifyAndClearExpectations(distiller_);
}

TEST_F(ReadAnythingAppControllerTest,
       OnSelectionChange_DistillationInProgress) {
  ui::AXTreeID new_tree_id = ui::AXTreeID::CreateNewAXTreeID();
  ui::AXTreeUpdate update;
  SetUpdateTreeID(&update, new_tree_id);
  ui::AXNodeData root;
  root.id = 1;
  root.role = ax::mojom::Role::kStaticText;
  update.root_id = root.id;
  update.nodes = {root};
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
  ui::AXNodeData text_field_node1;
  text_field_node1.id = 2;
  text_field_node1.role = ax::mojom::Role::kTextField;

  ui::AXNodeData container_node;
  container_node.id = 3;
  container_node.role = ax::mojom::Role::kGenericContainer;

  ui::AXNodeData text_field_node2;
  text_field_node2.id = 4;
  text_field_node2.role = ax::mojom::Role::kTextField;
  update.nodes = {text_field_node1, container_node, text_field_node2};

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
  ui::AXNodeData text_node;
  text_node.id = 3;
  text_node.role = ax::mojom::Role::kStaticText;
  text_node.SetNameChecked("Hello");

  ui::AXNodeData ignored_node;
  ignored_node.id = 4;
  ignored_node.role = ax::mojom::Role::kNone;  // This node is ignored.
  update.nodes = {text_node, ignored_node};
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

  EXPECT_EQ(0, StartNodeId());
  EXPECT_EQ(0, EndNodeId());
  EXPECT_EQ(-1, StartOffset());
  EXPECT_EQ(-1, EndOffset());
  EXPECT_EQ(false, HasSelection());
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

TEST_F(ReadAnythingAppControllerTest, OnFontSizeReset_SetsFontSizeToDefault) {
  EXPECT_CALL(page_handler_, OnFontSizeChange(kReadAnythingDefaultFontScale))
      .Times(1);
  OnFontSizeReset();
}

TEST_F(ReadAnythingAppControllerTest, TurnedHighlightOn_SavesHighlightState) {
  EXPECT_CALL(page_handler_,
              OnHighlightGranularityChanged(
                  read_anything::mojom::HighlightGranularity::kOn))
      .Times(1);
  EXPECT_CALL(page_handler_,
              OnHighlightGranularityChanged(
                  read_anything::mojom::HighlightGranularity::kOff))
      .Times(0);
  TurnedHighlightOn();
}

TEST_F(ReadAnythingAppControllerTest, TurnedHighlightOff_SavesHighlightState) {
  EXPECT_CALL(page_handler_,
              OnHighlightGranularityChanged(
                  read_anything::mojom::HighlightGranularity::kOn))
      .Times(0);
  EXPECT_CALL(page_handler_,
              OnHighlightGranularityChanged(
                  read_anything::mojom::HighlightGranularity::kOff))
      .Times(1);
  TurnedHighlightOff();
}

TEST_F(ReadAnythingAppControllerTest, GetNextSentence_ReturnsCorrectIndex) {
  const std::u16string first_sentence = u"This is a normal sentence. ";
  const std::u16string second_sentence = u"This is a second sentence.";

  const std::u16string sentence = first_sentence + second_sentence;
  size_t index = GetNextSentence(sentence, 175);
  EXPECT_EQ(index, first_sentence.length());
  EXPECT_EQ(sentence.substr(0, index), first_sentence);
}

TEST_F(ReadAnythingAppControllerTest,
       GetNextSentence_MaxLengthCutsOffSentence_ReturnsCorrectIndex) {
  const std::u16string first_sentence = u"This is a normal sentence. ";
  const std::u16string second_sentence = u"This is a second sentence.";

  const std::u16string sentence = first_sentence + second_sentence;
  size_t index = GetNextSentence(sentence, first_sentence.length() - 3);
  EXPECT_TRUE(index < first_sentence.length());
  EXPECT_EQ(sentence.substr(0, index), u"This is a normal ");
}

TEST_F(ReadAnythingAppControllerTest,
       GetNextSentence_TextLongerThanMaxLength_ReturnsCorrectIndex) {
  const std::u16string first_sentence = u"This is a normal sentence. ";
  const std::u16string second_sentence = u"This is a second sentence.";

  const std::u16string sentence = first_sentence + second_sentence;
  size_t index = GetNextSentence(
      sentence, first_sentence.length() + second_sentence.length() - 5);
  EXPECT_EQ(index, first_sentence.length());
  EXPECT_EQ(sentence.substr(0, index), first_sentence);
}

TEST_F(ReadAnythingAppControllerTest,
       GetNextSentence_OnlyOneSentence_ReturnsCorrectIndex) {
  const std::u16string sentence = u"Hello, this is a normal sentence.";

  size_t index = GetNextSentence(sentence, 175);
  EXPECT_EQ(index, sentence.length());
  EXPECT_EQ(sentence.substr(0, index), sentence);
}

TEST_F(
    ReadAnythingAppControllerTest,
    GetNextSentence_MaxLengthCutsOffSentence_OnlyOneSentence_ReturnsCorrectIndex) {
  const std::u16string sentence = u"Hello, this is a normal sentence.";

  size_t index = GetNextSentence(sentence, 12);
  EXPECT_TRUE(index < sentence.length());
  EXPECT_EQ(sentence.substr(0, index), u"Hello, ");
}

TEST_F(ReadAnythingAppControllerTest,
       GetLanguageCodeForSpeech_ReturnsCorrectLanguageCode) {
  SetLanguageCode("es");
  ASSERT_EQ(LanguageCodeForSpeech(), "es");
}

TEST_F(ReadAnythingAppControllerTest, AccessibilityEventReceived_PDFHandling) {
  // Call OnActiveAXTreeIDChanged() to set is_pdf_ state.
  GURL pdf_url("http://www.google.com/foo/bar.pdf");
  OnActiveAXTreeIDChanged(tree_id_, pdf_url, true);

  // Send update for main web contentes.
  ui::AXTreeID pdf_web_contents_tree_id = ui::AXTreeID::CreateNewAXTreeID();
  ui::AXTreeUpdate update;
  SetUpdateTreeID(&update);
  ui::AXNodeData node;
  node.id = 1;
  node.AddChildTreeId(pdf_web_contents_tree_id);
  update.nodes = {node};
  AccessibilityEventReceived({update});

  // Send update for pdf web contents.
  ui::AXTreeUpdate pdf_web_contents_update;
  ui::AXNodeData pdfNode;
  pdfNode.id = 1;
  pdf_web_contents_update.root_id = pdfNode.id;
  pdf_web_contents_update.nodes = {pdfNode};
  SetUpdateTreeID(&pdf_web_contents_update, pdf_web_contents_tree_id);
  AccessibilityEventReceived({pdf_web_contents_update});

  EXPECT_CALL(page_handler_,
              EnablePDFContentAccessibility(pdf_web_contents_tree_id))
      .Times(1);
  Mock::VerifyAndClearExpectations(distiller_);
}

TEST_F(ReadAnythingAppControllerTest, GetNextValidPosition) {
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
  OnAXTreeDistilled({static_text1.id, static_text2.id, static_text3.id});
  InitAXPosition(update.nodes[0].id);
  ui::AXNodePosition::AXPositionInstance new_position = GetNextNodePosition();
  EXPECT_EQ(new_position->anchor_id(), static_text2.id);
  EXPECT_EQ(new_position->GetText(), sentence2);

  // Getting the next node position shouldn't update the current AXPosition.
  new_position = GetNextNodePosition();
  EXPECT_EQ(new_position->anchor_id(), static_text2.id);
  EXPECT_EQ(new_position->GetText(), sentence2);
}

TEST_F(ReadAnythingAppControllerTest, GetNextValidPosition_SkipsNonTextNode) {
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
  OnAXTreeDistilled({static_text1.id, empty_node.id, static_text2.id});
  InitAXPosition(update.nodes[0].id);
  ui::AXNodePosition::AXPositionInstance new_position = GetNextNodePosition();
  EXPECT_EQ(new_position->anchor_id(), static_text2.id);
  EXPECT_EQ(new_position->GetText(), sentence2);
}

TEST_F(ReadAnythingAppControllerTest,
       GetNextValidPosition_SkipsNonDistilledNode) {
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
  OnAXTreeDistilled({static_text1.id, static_text3.id});
  InitAXPosition(update.nodes[0].id);
  ui::AXNodePosition::AXPositionInstance new_position = GetNextNodePosition();
  EXPECT_EQ(new_position->anchor_id(), static_text3.id);
  EXPECT_EQ(new_position->GetText(), sentence3);
}

TEST_F(ReadAnythingAppControllerTest,
       GetNextValidPosition_SkipsNodeWithHTMLTag) {
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
  OnAXTreeDistilled({static_text1.id, static_text2.id, static_text3.id});
  InitAXPosition(update.nodes[0].id);
  ui::AXNodePosition::AXPositionInstance new_position = GetNextNodePosition();
  EXPECT_EQ(new_position->anchor_id(), static_text3.id);
  EXPECT_EQ(new_position->GetText(), sentence3);
}

TEST_F(ReadAnythingAppControllerTest,
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
  OnAXTreeDistilled({static_text.id, empty_node1.id, empty_node2.id});
  InitAXPosition(update.nodes[0].id);
  ui::AXNodePosition::AXPositionInstance new_position = GetNextNodePosition();
  EXPECT_TRUE(new_position->IsNullPosition());
}

TEST_F(ReadAnythingAppControllerTest, GetNextText_ReturnsExpectedNodes) {
  // TODO(crbug.com/1474951): Investigate if we can improve in scenarios when
  // there's not a space between sentences.
  std::u16string sentence1 = u"This is a sentence. ";
  std::u16string sentence2 = u"This is another sentence. ";
  std::u16string sentence3 = u"And this is yet another sentence. ";
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
  OnAXTreeDistilled({static_text1.id, static_text2.id, static_text3.id});
  InitAXPosition(update.nodes[0].id);

  std::vector<ui::AXNodeID> next_node_ids = GetNextText();
  EXPECT_EQ((int)next_node_ids.size(), 1);
  // The returned id should be the next node id, 2
  EXPECT_EQ(next_node_ids[0], static_text1.id);
  // The returned int should be the beginning of the node's text.
  EXPECT_EQ(GetNextTextStartIndex(next_node_ids[0]), 0);
  // The returned int should be equivalent to the text in the node.
  EXPECT_EQ(GetNextTextEndIndex(next_node_ids[0]), (int)sentence1.length());

  // Move to the next node
  next_node_ids = GetNextText();
  EXPECT_EQ((int)next_node_ids.size(), 1);
  EXPECT_EQ(next_node_ids[0], static_text2.id);
  EXPECT_EQ(GetNextTextStartIndex(next_node_ids[0]), 0);
  EXPECT_EQ(GetNextTextEndIndex(next_node_ids[0]), (int)sentence2.length());

  // Move to the last node
  next_node_ids = GetNextText();
  EXPECT_EQ((int)next_node_ids.size(), 1);
  EXPECT_EQ(next_node_ids[0], static_text3.id);
  EXPECT_EQ(GetNextTextStartIndex(next_node_ids[0]), 0);
  EXPECT_EQ(GetNextTextEndIndex(next_node_ids[0]), (int)sentence3.length());

  // Attempt to move to another node.
  next_node_ids = GetNextText();
  EXPECT_EQ((int)next_node_ids.size(), 0);
}

TEST_F(ReadAnythingAppControllerTest, GetNextText_AfterAXTreeRefresh) {
  std::u16string sentence1 = u"This is a sentence. ";
  std::u16string sentence2 = u"This is another sentence. ";
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
  OnAXTreeDistilled({static_text1.id, static_text2.id, static_text3.id});
  InitAXPosition(update.nodes[0].id);

  std::vector<ui::AXNodeID> next_node_ids = GetNextText();
  EXPECT_EQ((int)next_node_ids.size(), 1);
  EXPECT_EQ(next_node_ids[0], static_text1.id);
  EXPECT_EQ(GetNextTextStartIndex(next_node_ids[0]), 0);
  EXPECT_EQ(GetNextTextEndIndex(next_node_ids[0]), (int)sentence1.length());

  // Simulate updating the page text.
  std::u16string new_sentence_1 =
      u"And so I read a book or maybe two or three. ";
  std::u16string new_sentence_2 =
      u"I will add a few new paitings to my gallery. ";
  std::u16string new_sentence_3 =
      u"I will play guitar and knit and cook and basically wonder when will my "
      u"life begin.";
  ui::AXTreeID id_1 = ui::AXTreeID::CreateNewAXTreeID();
  ui::AXTreeUpdate update2;
  SetUpdateTreeID(&update2, id_1);
  ui::AXNodeData root;
  root.id = 1;

  ui::AXNodeData new_static_text1;
  new_static_text1.id = 10;
  new_static_text1.role = ax::mojom::Role::kStaticText;
  new_static_text1.SetNameChecked(new_sentence_1);

  ui::AXNodeData new_static_text2;
  new_static_text2.id = 12;
  new_static_text2.role = ax::mojom::Role::kStaticText;
  new_static_text2.SetNameChecked(new_sentence_2);

  ui::AXNodeData new_static_text3;
  new_static_text3.id = 16;
  new_static_text3.role = ax::mojom::Role::kStaticText;
  new_static_text3.SetNameChecked(new_sentence_3);

  root.child_ids = {new_static_text1.id, new_static_text2.id,
                    new_static_text3.id};
  update2.root_id = root.id;
  update2.nodes = {root, new_static_text1, new_static_text2, new_static_text3};
  OnActiveAXTreeIDChanged(id_1);
  OnAXTreeDistilled({});
  AccessibilityEventReceived({update2});
  OnAXTreeDistilled(
      id_1, {new_static_text1.id, new_static_text2.id, new_static_text3.id});
  InitAXPosition(update2.nodes[1].id);

  // The nodes from the new tree are used.
  next_node_ids = GetNextText();
  EXPECT_EQ((int)next_node_ids.size(), 1);
  EXPECT_EQ(next_node_ids[0], update2.nodes[1].id);
  EXPECT_EQ(GetNextTextStartIndex(next_node_ids[0]), 0);
  EXPECT_EQ(GetNextTextEndIndex(next_node_ids[0]),
            (int)new_sentence_1.length());

  next_node_ids = GetNextText();
  EXPECT_EQ((int)next_node_ids.size(), 1);
  EXPECT_EQ(next_node_ids[0], update2.nodes[2].id);
  EXPECT_EQ(GetNextTextStartIndex(next_node_ids[0]), 0);
  EXPECT_EQ(GetNextTextEndIndex(next_node_ids[0]),
            (int)new_sentence_2.length());

  next_node_ids = GetNextText();
  EXPECT_EQ((int)next_node_ids.size(), 1);
  EXPECT_EQ(next_node_ids[0], update2.nodes[3].id);
  EXPECT_EQ(GetNextTextStartIndex(next_node_ids[0]), 0);
  EXPECT_EQ(GetNextTextEndIndex(next_node_ids[0]),
            (int)new_sentence_3.length());

  // Nodes are empty at the end of the new tree.
  next_node_ids = GetNextText();
  EXPECT_EQ((int)next_node_ids.size(), 0);
}

TEST_F(ReadAnythingAppControllerTest,
       GetNextText_SentenceSplitAcrossMultipleNodes) {
  std::u16string sentence1 = u"The wind is howling like this ";
  std::u16string sentence2 = u"swirling storm ";
  std::u16string sentence3 = u"inside.";
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
  OnAXTreeDistilled({static_text1.id, static_text2.id, static_text3.id});
  InitAXPosition(update.nodes[0].id);

  std::vector<ui::AXNodeID> next_node_ids = GetNextText();

  // The first segment was returned correctly.
  EXPECT_EQ(next_node_ids[0], static_text1.id);
  EXPECT_EQ(GetNextTextStartIndex(next_node_ids[0]), 0);
  EXPECT_EQ(GetNextTextEndIndex(next_node_ids[0]), (int)sentence1.length());

  // The second segment was returned correctly.
  EXPECT_EQ(next_node_ids[1], static_text2.id);
  EXPECT_EQ(GetNextTextStartIndex(next_node_ids[1]), 0);
  EXPECT_EQ(GetNextTextEndIndex(next_node_ids[1]), (int)sentence2.length());

  // The third segment was returned correctly.
  EXPECT_EQ(next_node_ids[2], static_text3.id);
  EXPECT_EQ(GetNextTextStartIndex(next_node_ids[2]), 0);
  EXPECT_EQ(GetNextTextEndIndex(next_node_ids[2]), (int)sentence3.length());

  // Nodes are empty at the end of the new tree.
  next_node_ids = GetNextText();
  EXPECT_EQ((int)next_node_ids.size(), 0);
}

TEST_F(ReadAnythingAppControllerTest, GetNextText_SentenceSplitAcrossTwoNodes) {
  std::u16string sentence1 = u"And I am almost ";
  std::u16string sentence2 = u"there. ";
  std::u16string sentence3 = u"I am almost there.";
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
  OnAXTreeDistilled({static_text1.id, static_text2.id, static_text3.id});
  InitAXPosition(update.nodes[0].id);

  std::vector<ui::AXNodeID> next_node_ids = GetNextText();
  EXPECT_EQ((int)next_node_ids.size(), 2);

  // The first segment was returned correctly.
  EXPECT_EQ(next_node_ids[0], static_text1.id);
  EXPECT_EQ(GetNextTextStartIndex(next_node_ids[0]), 0);
  EXPECT_EQ(GetNextTextEndIndex(next_node_ids[0]), (int)sentence1.length());

  // The second segment was returned correctly.
  EXPECT_EQ(next_node_ids[1], static_text2.id);
  EXPECT_EQ(GetNextTextStartIndex(next_node_ids[1]), 0);
  EXPECT_EQ(GetNextTextEndIndex(next_node_ids[1]), (int)sentence2.length());

  // The third segment was returned correctly after getting the next text.
  next_node_ids = GetNextText();
  EXPECT_EQ((int)next_node_ids.size(), 1);
  EXPECT_EQ(next_node_ids[0], static_text3.id);
  EXPECT_EQ(GetNextTextStartIndex(next_node_ids[0]), 0);
  EXPECT_EQ(GetNextTextEndIndex(next_node_ids[0]), (int)sentence3.length());

  // Nodes are empty at the end of the new tree.
  next_node_ids = GetNextText();
  EXPECT_EQ((int)next_node_ids.size(), 0);
}

TEST_F(ReadAnythingAppControllerTest,
       GetNextText_SentenceSplitAcrossParagraphs) {
  std::u16string header_text = u"Header Text";
  std::u16string paragraph_text1 = u"Paragraph one.";
  std::u16string paragraph_text2 = u"Paragraph two.";
  ui::AXTreeUpdate update;
  SetUpdateTreeID(&update);

  ui::AXNodeData static_text1;
  static_text1.id = 2;
  static_text1.role = ax::mojom::Role::kStaticText;
  static_text1.SetNameChecked(header_text);

  ui::AXNodeData static_text2;
  static_text2.id = 3;
  static_text2.role = ax::mojom::Role::kStaticText;
  static_text2.SetNameChecked(paragraph_text1);

  ui::AXNodeData static_text3;
  static_text3.id = 4;
  static_text3.role = ax::mojom::Role::kStaticText;
  static_text3.SetNameChecked(paragraph_text2);

  ui::AXNodeData header_node;
  header_node.id = 5;
  header_node.role = ax::mojom::Role::kHeader;
  header_node.AddBoolAttribute(ax::mojom::BoolAttribute::kIsLineBreakingObject,
                               true);
  header_node.child_ids = {static_text1.id};

  ui::AXNodeData paragraph_node1;
  paragraph_node1.id = 6;
  paragraph_node1.role = ax::mojom::Role::kParagraph;
  paragraph_node1.AddBoolAttribute(
      ax::mojom::BoolAttribute::kIsLineBreakingObject, true);
  paragraph_node1.child_ids = {static_text2.id};

  ui::AXNodeData paragraph_node2;
  paragraph_node2.id = 7;
  paragraph_node2.role = ax::mojom::Role::kParagraph;
  paragraph_node2.AddBoolAttribute(
      ax::mojom::BoolAttribute::kIsLineBreakingObject, true);
  paragraph_node2.child_ids = {static_text3.id};

  ui::AXNodeData root;
  root.id = 10;
  root.role = ax::mojom::Role::kParagraph;
  root.child_ids = {header_node.id, paragraph_node1.id, paragraph_node2.id};
  update.root_id = root.id;

  update.nodes = {root,         header_node,     static_text1, paragraph_node1,
                  static_text2, paragraph_node2, static_text3};
  AccessibilityEventReceived({update});
  OnAXTreeDistilled({root.id, header_node.id, static_text1.id,
                     paragraph_node1.id, static_text2.id, paragraph_node2.id,
                     static_text3.id});
  InitAXPosition(static_text1.id);

  // The header is returned alone.
  std::vector<ui::AXNodeID> next_node_ids = GetNextText();
  EXPECT_EQ((int)next_node_ids.size(), 1);
  EXPECT_EQ(next_node_ids[0], static_text1.id);
  EXPECT_EQ(GetNextTextStartIndex(next_node_ids[0]), 0);
  EXPECT_EQ(GetNextTextEndIndex(next_node_ids[0]), (int)header_text.length());

  // Paragraph 1 is returned alone.
  next_node_ids = GetNextText();
  EXPECT_EQ((int)next_node_ids.size(), 1);
  EXPECT_EQ(next_node_ids[0], static_text2.id);
  EXPECT_EQ(GetNextTextStartIndex(next_node_ids[0]), 0);
  EXPECT_EQ(GetNextTextEndIndex(next_node_ids[0]),
            (int)paragraph_text1.length());

  // Paragraph 2 is returned alone.
  next_node_ids = GetNextText();
  EXPECT_EQ((int)next_node_ids.size(), 1);
  EXPECT_EQ(next_node_ids[0], static_text3.id);
  EXPECT_EQ(GetNextTextStartIndex(next_node_ids[0]), 0);
  EXPECT_EQ(GetNextTextEndIndex(next_node_ids[0]),
            (int)paragraph_text2.length());

  // Nodes are empty at the end of the new tree.
  next_node_ids = GetNextText();
  EXPECT_EQ((int)next_node_ids.size(), 0);
}

TEST_F(ReadAnythingAppControllerTest,
       GetNextText_SentenceSplitAcrossParagraphsWithoutParagraphRoles) {
  std::u16string header_text = u"Header Text\n";
  std::u16string paragraph_text1 = u"Paragraph one.\n";
  std::u16string paragraph_text2 = u"Paragraph two.";
  ui::AXTreeUpdate update;
  SetUpdateTreeID(&update);

  ui::AXNodeData header_node;
  header_node.id = 2;
  header_node.role = ax::mojom::Role::kStaticText;
  header_node.SetNameChecked(header_text);

  ui::AXNodeData paragraph_node1;
  paragraph_node1.id = 3;
  paragraph_node1.role = ax::mojom::Role::kStaticText;
  paragraph_node1.SetNameChecked(paragraph_text1);

  ui::AXNodeData paragraph_node2;
  paragraph_node2.id = 4;
  paragraph_node2.role = ax::mojom::Role::kStaticText;
  paragraph_node2.SetNameChecked(paragraph_text2);

  update.nodes = {header_node, paragraph_node1, paragraph_node2};
  AccessibilityEventReceived({update});
  OnAXTreeDistilled({header_node.id, paragraph_node1.id, paragraph_node2.id});
  InitAXPosition(header_node.id);

  // The header is returned alone.
  std::vector<ui::AXNodeID> next_node_ids = GetNextText();
  EXPECT_EQ((int)next_node_ids.size(), 1);
  EXPECT_EQ(next_node_ids[0], header_node.id);
  EXPECT_EQ(GetNextTextStartIndex(next_node_ids[0]), 0);
  EXPECT_EQ(GetNextTextEndIndex(next_node_ids[0]), (int)header_text.length());

  // Paragraph 1 is returned alone.
  next_node_ids = GetNextText();
  EXPECT_EQ((int)next_node_ids.size(), 1);
  EXPECT_EQ(next_node_ids[0], paragraph_node1.id);
  EXPECT_EQ(GetNextTextStartIndex(next_node_ids[0]), 0);
  EXPECT_EQ(GetNextTextEndIndex(next_node_ids[0]),
            (int)paragraph_text1.length());

  // Paragraph 2 is returned alone.
  next_node_ids = GetNextText();
  EXPECT_EQ((int)next_node_ids.size(), 1);
  EXPECT_EQ(next_node_ids[0], paragraph_node2.id);
  EXPECT_EQ(GetNextTextStartIndex(next_node_ids[0]), 0);
  EXPECT_EQ(GetNextTextEndIndex(next_node_ids[0]),
            (int)paragraph_text2.length());

  // Nodes are empty at the end of the new tree.
  next_node_ids = GetNextText();
  EXPECT_EQ((int)next_node_ids.size(), 0);
}

TEST_F(ReadAnythingAppControllerTest, GetNextText_MultipleSentencesInSameNode) {
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
  OnAXTreeDistilled({static_text1.id, static_text2.id, static_text3.id});
  InitAXPosition(update.nodes[0].id);

  std::vector<ui::AXNodeID> next_node_ids = GetNextText();
  EXPECT_EQ((int)next_node_ids.size(), 1);

  // The first segment was returned correctly.
  EXPECT_EQ(next_node_ids[0], static_text1.id);
  EXPECT_EQ(GetNextTextStartIndex(next_node_ids[0]), 0);
  EXPECT_EQ(GetNextTextEndIndex(next_node_ids[0]), (int)sentence1.find(u"The"));

  // The second segment was returned correctly, across 3 nodes.
  next_node_ids = GetNextText();
  EXPECT_EQ((int)next_node_ids.size(), 3);

  EXPECT_EQ(next_node_ids[0], static_text1.id);
  EXPECT_EQ(GetNextTextStartIndex(next_node_ids[0]),
            (int)sentence1.find(u"The"));
  EXPECT_EQ(GetNextTextEndIndex(next_node_ids[0]), (int)sentence1.length());

  EXPECT_EQ(next_node_ids[1], static_text2.id);
  EXPECT_EQ(GetNextTextStartIndex(next_node_ids[1]), 0);
  EXPECT_EQ(GetNextTextEndIndex(next_node_ids[1]), (int)sentence2.length());

  EXPECT_EQ(next_node_ids[2], static_text3.id);
  EXPECT_EQ(GetNextTextStartIndex(next_node_ids[2]), 0);
  EXPECT_EQ(GetNextTextEndIndex(next_node_ids[2]), (int)sentence3.find(u"And"));

  // The next sentence "And suddenly life seems so clear" was returned correctly
  next_node_ids = GetNextText();
  EXPECT_EQ((int)next_node_ids.size(), 1);

  EXPECT_EQ(next_node_ids[0], static_text3.id);
  EXPECT_EQ(GetNextTextStartIndex(next_node_ids[0]),
            (int)sentence3.find(u"And"));
  EXPECT_EQ(GetNextTextEndIndex(next_node_ids[0]),
            (int)sentence3.find(u"And from"));

  // The next sentence "And from up here" was returned correctly
  next_node_ids = GetNextText();
  EXPECT_EQ((int)next_node_ids.size(), 1);

  EXPECT_EQ(next_node_ids[0], static_text3.id);
  EXPECT_EQ(GetNextTextStartIndex(next_node_ids[0]),
            (int)sentence3.find(u"And from"));
  EXPECT_EQ(GetNextTextEndIndex(next_node_ids[0]), (int)sentence3.find(u"You"));

  // The next sentence "You coast past it all" was returned correctly
  next_node_ids = GetNextText();
  EXPECT_EQ((int)next_node_ids.size(), 1);

  EXPECT_EQ(next_node_ids[0], static_text3.id);
  EXPECT_EQ(GetNextTextStartIndex(next_node_ids[0]),
            (int)sentence3.find(u"You"));
  EXPECT_EQ(GetNextTextEndIndex(next_node_ids[0]), (int)sentence3.find(u"The"));

  // The next sentence "The obstacles just disappear" was returned correctly
  next_node_ids = GetNextText();
  EXPECT_EQ((int)next_node_ids.size(), 1);

  EXPECT_EQ(next_node_ids[0], static_text3.id);
  EXPECT_EQ(GetNextTextStartIndex(next_node_ids[0]),
            (int)sentence3.find(u"The"));
  EXPECT_EQ(GetNextTextEndIndex(next_node_ids[0]), (int)sentence3.length());

  // Nodes are empty at the end of the new tree.
  next_node_ids = GetNextText();
  EXPECT_EQ((int)next_node_ids.size(), 0);
}

TEST_F(
    ReadAnythingAppControllerTest,
    GetNextValidPosition_AfterGetNextNodesButBeforeGetNextText_UsesCurrentGranularity) {
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
  OnAXTreeDistilled({static_text1.id, static_text2.id, static_text3.id});
  InitAXPosition(update.nodes[0].id);

  ReadAnythingAppController::ReadAloudCurrentGranularity current_granularity =
      GetNextNodes();
  // Expect that current_granularity contains static_text1
  // Expect that the indices aren't returned correctly
  // Expect that GetNextValidPosition fails without inserted the granularity.
  // The first segment was returned correctly.
  EXPECT_EQ((int)current_granularity.node_ids.size(), 1);
  EXPECT_TRUE(base::Contains(current_granularity.node_ids, static_text1.id));
  EXPECT_EQ(GetNextTextStartIndex(static_text1.id), -1);
  EXPECT_EQ(GetNextTextEndIndex(static_text1.id), -1);

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

TEST_F(ReadAnythingAppControllerTest, GetNextText_EmptyTree) {
  // If InitAXPosition hasn't been called, GetNextText should return nothing.
  std::vector<ui::AXNodeID> next_node_ids = GetNextText();
  EXPECT_EQ((int)next_node_ids.size(), 0);

  // GetNextTextStartIndex and GetNextTextEndIndex should return -1  on an
  // invalid id.
  EXPECT_EQ(GetNextTextStartIndex(0), -1);
  EXPECT_EQ(GetNextTextEndIndex(0), -1);
}

TEST_F(ReadAnythingAppControllerTest, GetPreviousText_AfterAXTreeRefresh) {
  std::u16string sentence1 = u"This is a sentence. ";
  std::u16string sentence2 = u"This is another sentence. ";
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
  OnAXTreeDistilled({static_text1.id, static_text2.id, static_text3.id});
  InitAXPosition(update.nodes[0].id);

  std::vector<ui::AXNodeID> next_node_ids = GetNextText();
  EXPECT_EQ((int)next_node_ids.size(), 1);
  EXPECT_EQ(next_node_ids[0], static_text1.id);
  EXPECT_EQ(GetNextTextStartIndex(next_node_ids[0]), 0);
  EXPECT_EQ(GetNextTextEndIndex(next_node_ids[0]), (int)sentence1.length());

  // Simulate updating the page text.
  std::u16string new_sentence1 = u"Welcome to the show to the histo-remix. ";
  std::u16string new_sentence2 =
      u"Switching up the flow, as we add the prefix. ";
  std::u16string new_sentence3 =
      u"Everybody knows that we used to be six wives. ";
  ui::AXTreeID id_1 = ui::AXTreeID::CreateNewAXTreeID();
  ui::AXTreeUpdate update2;
  SetUpdateTreeID(&update2, id_1);
  ui::AXNodeData root;
  root.id = 1;

  ui::AXNodeData new_static_text1;
  new_static_text1.id = 10;
  new_static_text1.role = ax::mojom::Role::kStaticText;
  new_static_text1.SetNameChecked(new_sentence1);

  ui::AXNodeData new_static_text2;
  new_static_text2.id = 12;
  new_static_text2.role = ax::mojom::Role::kStaticText;
  new_static_text2.SetNameChecked(new_sentence2);

  ui::AXNodeData new_static_text3;
  new_static_text3.id = 16;
  new_static_text3.role = ax::mojom::Role::kStaticText;
  new_static_text3.SetNameChecked(new_sentence3);

  root.child_ids = {new_static_text1.id, new_static_text2.id,
                    new_static_text3.id};
  update2.root_id = root.id;
  update2.nodes = {root, new_static_text1, new_static_text2, new_static_text3};
  OnActiveAXTreeIDChanged(id_1);
  OnAXTreeDistilled({});
  AccessibilityEventReceived({update2});
  OnAXTreeDistilled(
      id_1, {new_static_text1.id, new_static_text2.id, new_static_text3.id});
  InitAXPosition(update2.nodes[1].id);

  // The nodes from the new tree are used.
  // Move to the end of the content.
  GetNextText();
  GetNextText();
  GetNextText();
  next_node_ids = GetNextText();
  EXPECT_EQ((int)next_node_ids.size(), 0);

  std::vector<ui::AXNodeID> previous_node_ids = GetPreviousText();
  EXPECT_EQ((int)previous_node_ids.size(), 1);
  EXPECT_EQ(previous_node_ids[0], new_static_text2.id);
  EXPECT_EQ(GetNextTextStartIndex(previous_node_ids[0]), 0);
  EXPECT_EQ(GetNextTextEndIndex(previous_node_ids[0]),
            (int)new_sentence2.length());

  previous_node_ids = GetPreviousText();
  EXPECT_EQ((int)previous_node_ids.size(), 1);
  EXPECT_EQ(previous_node_ids[0], new_static_text1.id);
  EXPECT_EQ(GetNextTextStartIndex(previous_node_ids[0]), 0);
  EXPECT_EQ(GetNextTextEndIndex(previous_node_ids[0]),
            (int)new_sentence1.length());

  // We're at the beginning of the content again, so the first sentence
  // should be retrieved next.
  previous_node_ids = GetPreviousText();
  EXPECT_EQ((int)previous_node_ids.size(), 1);
  EXPECT_EQ(previous_node_ids[0], new_static_text1.id);
  EXPECT_EQ(GetNextTextStartIndex(previous_node_ids[0]), 0);
  EXPECT_EQ(GetNextTextEndIndex(previous_node_ids[0]),
            (int)new_sentence1.length());

  // After navigating previous text, navigating forwards should continue
  // to work as expected.
  next_node_ids = GetNextText();
  EXPECT_EQ((int)next_node_ids.size(), 1);
  EXPECT_EQ(next_node_ids[0], new_static_text2.id);
  EXPECT_EQ(GetNextTextStartIndex(next_node_ids[0]), 0);
  EXPECT_EQ(GetNextTextEndIndex(next_node_ids[0]), (int)new_sentence2.length());

  next_node_ids = GetNextText();
  EXPECT_EQ((int)next_node_ids.size(), 1);
  EXPECT_EQ(next_node_ids[0], new_static_text3.id);
  EXPECT_EQ(GetNextTextStartIndex(next_node_ids[0]), 0);
  EXPECT_EQ(GetNextTextEndIndex(next_node_ids[0]), (int)new_sentence3.length());

  // Attempt to move to another node.
  next_node_ids = GetNextText();
  EXPECT_EQ((int)next_node_ids.size(), 0);
}

TEST_F(ReadAnythingAppControllerTest, GetPreviousText_ReturnsExpectedNodes) {
  std::u16string sentence1 = u"See the line where the sky meets the sea? ";
  std::u16string sentence2 = u"It calls me. ";
  std::u16string sentence3 = u"And no one knows how far it goes.";
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
  OnAXTreeDistilled({static_text1.id, static_text2.id, static_text3.id});
  InitAXPosition(update.nodes[0].id);

  // Move to the end of the content.
  GetNextText();
  GetNextText();
  GetNextText();
  std::vector<ui::AXNodeID> next_node_ids = GetNextText();
  EXPECT_EQ((int)next_node_ids.size(), 0);

  std::vector<ui::AXNodeID> previous_node_ids = GetPreviousText();
  EXPECT_EQ((int)previous_node_ids.size(), 1);
  EXPECT_EQ(previous_node_ids[0], static_text2.id);
  EXPECT_EQ(GetNextTextStartIndex(previous_node_ids[0]), 0);
  EXPECT_EQ(GetNextTextEndIndex(previous_node_ids[0]), (int)sentence2.length());

  previous_node_ids = GetPreviousText();
  EXPECT_EQ((int)previous_node_ids.size(), 1);
  EXPECT_EQ(previous_node_ids[0], static_text1.id);
  EXPECT_EQ(GetNextTextStartIndex(previous_node_ids[0]), 0);
  EXPECT_EQ(GetNextTextEndIndex(previous_node_ids[0]), (int)sentence1.length());

  // We're at the beginning of the content again, so the first sentence
  // should be retrieved next.
  previous_node_ids = GetPreviousText();
  EXPECT_EQ((int)previous_node_ids.size(), 1);
  EXPECT_EQ(previous_node_ids[0], static_text1.id);
  EXPECT_EQ(GetNextTextStartIndex(previous_node_ids[0]), 0);
  EXPECT_EQ(GetNextTextEndIndex(previous_node_ids[0]), (int)sentence1.length());

  // After navigating previous text, navigating forwards should continue
  // to work as expected.
  next_node_ids = GetNextText();
  EXPECT_EQ((int)next_node_ids.size(), 1);
  EXPECT_EQ(next_node_ids[0], static_text2.id);
  EXPECT_EQ(GetNextTextStartIndex(next_node_ids[0]), 0);
  EXPECT_EQ(GetNextTextEndIndex(next_node_ids[0]), (int)sentence2.length());

  next_node_ids = GetNextText();
  EXPECT_EQ((int)next_node_ids.size(), 1);
  EXPECT_EQ(next_node_ids[0], static_text3.id);
  EXPECT_EQ(GetNextTextStartIndex(next_node_ids[0]), 0);
  EXPECT_EQ(GetNextTextEndIndex(next_node_ids[0]), (int)sentence3.length());

  // Attempt to move to another node.
  next_node_ids = GetNextText();
  EXPECT_EQ((int)next_node_ids.size(), 0);
}

TEST_F(ReadAnythingAppControllerTest, GetPreviousText_EmptyTree) {
  // If InitAXPosition hasn't been called, GetPreviousText should return
  // nothing.
  std::vector<ui::AXNodeID> previous_node_ids = GetPreviousText();
  EXPECT_EQ((int)previous_node_ids.size(), 0);

  // GetNextTextStartIndex and GetNextTextEndIndex should return -1  on an
  // invalid id.
  EXPECT_EQ(GetNextTextStartIndex(0), -1);
  EXPECT_EQ(GetNextTextEndIndex(0), -1);
}

TEST_F(ReadAnythingAppControllerTest, GetPreviousText_BeforeNextTextCalled) {
  std::u16string sentence1 = u"This is a sentence. ";
  std::u16string sentence2 = u"This is another sentence. ";
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
  OnAXTreeDistilled({static_text1.id, static_text2.id, static_text3.id});
  InitAXPosition(update.nodes[0].id);

  // If GetNextText hasn't been called, GetPreviousText should return
  // nothing.
  std::vector<ui::AXNodeID> previous_node_ids = GetPreviousText();
  EXPECT_EQ((int)previous_node_ids.size(), 0);

  // GetNextTextStartIndex and GetNextTextEndIndex should return -1  on an
  // invalid id.
  EXPECT_EQ(GetNextTextStartIndex(0), -1);
  EXPECT_EQ(GetNextTextEndIndex(0), -1);
}

TEST_F(ReadAnythingAppControllerTest,
       GetPreviousText_SentenceSplitAcrossMultipleNodes) {
  std::u16string sentence1 = u"The wind is howling like this ";
  std::u16string sentence2 = u"swirling storm ";
  std::u16string sentence3 = u"inside.";
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
  OnAXTreeDistilled({static_text1.id, static_text2.id, static_text3.id});
  InitAXPosition(update.nodes[0].id);

  GetNextText();
  std::vector<ui::AXNodeID> previous_node_ids = GetPreviousText();

  // The first segment was returned correctly.
  EXPECT_EQ(previous_node_ids[0], static_text1.id);
  EXPECT_EQ(GetNextTextStartIndex(previous_node_ids[0]), 0);
  EXPECT_EQ(GetNextTextEndIndex(previous_node_ids[0]), (int)sentence1.length());

  // The second segment was returned correctly.
  EXPECT_EQ(previous_node_ids[1], static_text2.id);
  EXPECT_EQ(GetNextTextStartIndex(previous_node_ids[1]), 0);
  EXPECT_EQ(GetNextTextEndIndex(previous_node_ids[1]), (int)sentence2.length());

  // The third segment was returned correctly.
  EXPECT_EQ(previous_node_ids[2], static_text3.id);
  EXPECT_EQ(GetNextTextStartIndex(previous_node_ids[2]), 0);
  EXPECT_EQ(GetNextTextEndIndex(previous_node_ids[2]), (int)sentence3.length());

  // Nodes are empty at the end of the new tree.
  std::vector<ui::AXNodeID> next_node_ids = GetNextText();
  EXPECT_EQ((int)next_node_ids.size(), 0);
}
