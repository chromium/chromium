// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/accessibility/read_anything_app_controller.h"

#include <string>
#include <vector>

#include "chrome/test/base/chrome_render_view_test.h"
#include "content/public/renderer/render_frame.h"
#include "ui/accessibility/ax_node.h"

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

    // Create simple AXTreeUpdate with a root node and 3 children.
    basic_snapshot_.root_id = 1;
    basic_snapshot_.nodes.resize(4);
    basic_snapshot_.nodes[0].id = 1;
    basic_snapshot_.nodes[0].child_ids = {2, 3, 4};
    basic_snapshot_.nodes[1].id = 2;
    basic_snapshot_.nodes[2].id = 3;
    basic_snapshot_.nodes[3].id = 4;

    // Create simple AXTreeData with selection.
    basic_tree_data_with_selection_.sel_anchor_object_id = 2;
    basic_tree_data_with_selection_.sel_focus_object_id = 3;
    basic_tree_data_with_selection_.sel_anchor_offset = 0;
    basic_tree_data_with_selection_.sel_focus_offset = 0;
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
  void OnAXTreeDistilled(const ui::AXTreeUpdate& snapshot,
                         const std::vector<ui::AXNodeID>& content_node_ids) {
    controller_->OnAXTreeDistilled(snapshot, content_node_ids);
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

  ui::AXTreeUpdate basic_snapshot_;
  ui::AXTreeData basic_tree_data_with_selection_;

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
  OnAXTreeDistilled(basic_snapshot_, {1});
  EXPECT_EQ(1, RootId());
  OnAXTreeDistilled(basic_snapshot_, {2});
  EXPECT_EQ(1, RootId());
  OnAXTreeDistilled(basic_snapshot_, {3});
  EXPECT_EQ(1, RootId());
  OnAXTreeDistilled(basic_snapshot_, {4});
  EXPECT_EQ(1, RootId());
}

TEST_F(ReadAnythingAppControllerTest, GetChildren_NoSelectionOrContentNodes) {
  basic_snapshot_.nodes[2].role = ax::mojom::Role::kNone;
  OnAXTreeDistilled(basic_snapshot_, {});
  EXPECT_EQ(0u, GetChildren(1).size());
  EXPECT_EQ(0u, GetChildren(2).size());
  EXPECT_EQ(0u, GetChildren(3).size());
  EXPECT_EQ(0u, GetChildren(4).size());
}

TEST_F(ReadAnythingAppControllerTest, GetChildren_WithContentNodes) {
  basic_snapshot_.nodes[2].role = ax::mojom::Role::kNone;
  OnAXTreeDistilled(basic_snapshot_, {1, 2, 3, 4});
  EXPECT_EQ(2u, GetChildren(1).size());
  EXPECT_EQ(0u, GetChildren(2).size());
  EXPECT_EQ(0u, GetChildren(3).size());
  EXPECT_EQ(0u, GetChildren(4).size());

  EXPECT_EQ(2, GetChildren(1)[0]);
  EXPECT_EQ(4, GetChildren(1)[1]);
}

TEST_F(ReadAnythingAppControllerTest, GetChildren_WithSelection) {
  // Create selection from node 3-4.
  basic_tree_data_with_selection_.sel_anchor_object_id = 3;
  basic_tree_data_with_selection_.sel_focus_object_id = 4;
  basic_snapshot_.has_tree_data = true;
  basic_snapshot_.tree_data = basic_tree_data_with_selection_;
  OnAXTreeDistilled(basic_snapshot_, {});
  EXPECT_EQ(2u, GetChildren(1).size());
  EXPECT_EQ(0u, GetChildren(2).size());
  EXPECT_EQ(0u, GetChildren(3).size());
  EXPECT_EQ(0u, GetChildren(4).size());

  EXPECT_EQ(3, GetChildren(1)[0]);
  EXPECT_EQ(4, GetChildren(1)[1]);
}

TEST_F(ReadAnythingAppControllerTest, GetChildren_WithBackwardSelection) {
  // Create backward selection from node 4-3.
  basic_tree_data_with_selection_.sel_is_backward = true;
  basic_tree_data_with_selection_.sel_anchor_object_id = 4;
  basic_tree_data_with_selection_.sel_focus_object_id = 3;
  basic_snapshot_.has_tree_data = true;
  basic_snapshot_.tree_data = basic_tree_data_with_selection_;
  OnAXTreeDistilled(basic_snapshot_, {});
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
  basic_snapshot_.nodes[1].AddStringAttribute(
      ax::mojom::StringAttribute::kHtmlTag, span);
  basic_snapshot_.nodes[2].AddStringAttribute(
      ax::mojom::StringAttribute::kHtmlTag, h1);
  basic_snapshot_.nodes[3].AddStringAttribute(
      ax::mojom::StringAttribute::kHtmlTag, ul);
  OnAXTreeDistilled(basic_snapshot_, {});
  EXPECT_EQ(span, GetHtmlTag(2));
  EXPECT_EQ(h1, GetHtmlTag(3));
  EXPECT_EQ(ul, GetHtmlTag(4));
}

TEST_F(ReadAnythingAppControllerTest, GetTextContent_NoSelection) {
  std::string text_content = "Hello";
  std::string missing_text_content = "";
  std::string more_text_content = " world";
  basic_snapshot_.nodes[1].role = ax::mojom::Role::kStaticText;
  basic_snapshot_.nodes[1].SetName(text_content);
  basic_snapshot_.nodes[1].SetNameFrom(ax::mojom::NameFrom::kContents);
  basic_snapshot_.nodes[2].role = ax::mojom::Role::kStaticText;
  basic_snapshot_.nodes[2].SetName(missing_text_content);
  basic_snapshot_.nodes[2].SetNameFrom(ax::mojom::NameFrom::kContents);
  basic_snapshot_.nodes[3].role = ax::mojom::Role::kStaticText;
  basic_snapshot_.nodes[3].SetName(more_text_content);
  basic_snapshot_.nodes[3].SetNameFrom(ax::mojom::NameFrom::kContents);
  OnAXTreeDistilled(basic_snapshot_, {});
  EXPECT_EQ("Hello world", GetTextContent(1));
  EXPECT_EQ(text_content, GetTextContent(2));
  EXPECT_EQ(missing_text_content, GetTextContent(3));
  EXPECT_EQ(more_text_content, GetTextContent(4));
}

TEST_F(ReadAnythingAppControllerTest, GetTextContent_With2NodeSelection) {
  std::string text_content_1 = "Hello";
  std::string text_content_2 = " world";
  std::string text_content_3 = " friend";
  basic_snapshot_.nodes[1].role = ax::mojom::Role::kStaticText;
  basic_snapshot_.nodes[1].SetName(text_content_1);
  basic_snapshot_.nodes[1].SetNameFrom(ax::mojom::NameFrom::kContents);
  basic_snapshot_.nodes[2].role = ax::mojom::Role::kStaticText;
  basic_snapshot_.nodes[2].SetName(text_content_2);
  basic_snapshot_.nodes[2].SetNameFrom(ax::mojom::NameFrom::kContents);
  basic_snapshot_.nodes[3].role = ax::mojom::Role::kStaticText;
  basic_snapshot_.nodes[3].SetName(text_content_3);
  basic_snapshot_.nodes[3].SetNameFrom(ax::mojom::NameFrom::kContents);
  // Create selection from node 2-3.
  basic_tree_data_with_selection_.sel_anchor_object_id = 2;
  basic_tree_data_with_selection_.sel_focus_object_id = 3;
  basic_tree_data_with_selection_.sel_anchor_offset = 1;
  basic_tree_data_with_selection_.sel_focus_offset = 3;
  basic_snapshot_.has_tree_data = true;
  basic_snapshot_.tree_data = basic_tree_data_with_selection_;
  OnAXTreeDistilled(basic_snapshot_, {});
  EXPECT_EQ("Hello world friend", GetTextContent(1));
  EXPECT_EQ("ello", GetTextContent(2));
  EXPECT_EQ(" wo", GetTextContent(3));
  EXPECT_EQ(" friend", GetTextContent(4));
}

TEST_F(ReadAnythingAppControllerTest, GetTextContent_With3NodeSelection) {
  std::string text_content_1 = "Hello";
  std::string text_content_2 = " world";
  std::string text_content_3 = " friend";
  basic_snapshot_.nodes[1].role = ax::mojom::Role::kStaticText;
  basic_snapshot_.nodes[1].SetName(text_content_1);
  basic_snapshot_.nodes[1].SetNameFrom(ax::mojom::NameFrom::kContents);
  basic_snapshot_.nodes[2].role = ax::mojom::Role::kStaticText;
  basic_snapshot_.nodes[2].SetName(text_content_2);
  basic_snapshot_.nodes[2].SetNameFrom(ax::mojom::NameFrom::kContents);
  basic_snapshot_.nodes[3].role = ax::mojom::Role::kStaticText;
  basic_snapshot_.nodes[3].SetName(text_content_3);
  basic_snapshot_.nodes[3].SetNameFrom(ax::mojom::NameFrom::kContents);
  // Create selection from node 2-4.
  basic_tree_data_with_selection_.sel_anchor_object_id = 2;
  basic_tree_data_with_selection_.sel_focus_object_id = 4;
  basic_tree_data_with_selection_.sel_anchor_offset = 1;
  basic_tree_data_with_selection_.sel_focus_offset = 3;
  basic_snapshot_.has_tree_data = true;
  basic_snapshot_.tree_data = basic_tree_data_with_selection_;
  OnAXTreeDistilled(basic_snapshot_, {});
  EXPECT_EQ("Hello world friend", GetTextContent(1));
  EXPECT_EQ("ello", GetTextContent(2));
  EXPECT_EQ(" world", GetTextContent(3));
  EXPECT_EQ(" fr", GetTextContent(4));
}

TEST_F(ReadAnythingAppControllerTest, GetTextContent_WithBackwardSelection) {
  std::string text_content_1 = "Hello";
  std::string text_content_2 = " world";
  std::string text_content_3 = " friend";
  basic_snapshot_.nodes[1].role = ax::mojom::Role::kStaticText;
  basic_snapshot_.nodes[1].SetName(text_content_1);
  basic_snapshot_.nodes[1].SetNameFrom(ax::mojom::NameFrom::kContents);
  basic_snapshot_.nodes[2].role = ax::mojom::Role::kStaticText;
  basic_snapshot_.nodes[2].SetName(text_content_2);
  basic_snapshot_.nodes[2].SetNameFrom(ax::mojom::NameFrom::kContents);
  basic_snapshot_.nodes[3].role = ax::mojom::Role::kStaticText;
  basic_snapshot_.nodes[3].SetName(text_content_3);
  basic_snapshot_.nodes[3].SetNameFrom(ax::mojom::NameFrom::kContents);
  // Create backward selection from node 4-3.
  basic_tree_data_with_selection_.sel_is_backward = true;
  basic_tree_data_with_selection_.sel_anchor_object_id = 4;
  basic_tree_data_with_selection_.sel_focus_object_id = 3;
  basic_tree_data_with_selection_.sel_anchor_offset = 5;
  basic_tree_data_with_selection_.sel_focus_offset = 2;
  basic_snapshot_.has_tree_data = true;
  basic_snapshot_.tree_data = basic_tree_data_with_selection_;
  OnAXTreeDistilled(basic_snapshot_, {});
  EXPECT_EQ("Hello world friend", GetTextContent(1));
  EXPECT_EQ("Hello", GetTextContent(2));
  EXPECT_EQ("orld", GetTextContent(3));
  EXPECT_EQ(" frie", GetTextContent(4));
}

TEST_F(ReadAnythingAppControllerTest, GetUrl) {
  std::string url = "http://www.google.com";
  std::string invalid_url = "cats";
  std::string missing_url = "";
  basic_snapshot_.nodes[1].AddStringAttribute(ax::mojom::StringAttribute::kUrl,
                                              url);
  basic_snapshot_.nodes[2].AddStringAttribute(ax::mojom::StringAttribute::kUrl,
                                              invalid_url);
  basic_snapshot_.nodes[3].AddStringAttribute(ax::mojom::StringAttribute::kUrl,
                                              missing_url);
  OnAXTreeDistilled(basic_snapshot_, {});
  EXPECT_EQ(url, GetUrl(2));
  EXPECT_EQ(invalid_url, GetUrl(3));
  EXPECT_EQ(missing_url, GetUrl(4));
}

TEST_F(ReadAnythingAppControllerTest, DisplayNodeIdsContains_Selection) {
  basic_snapshot_.has_tree_data = true;
  basic_snapshot_.tree_data = basic_tree_data_with_selection_;
  OnAXTreeDistilled(basic_snapshot_, {});
  EXPECT_TRUE(DisplayNodeIdsContains(1));
  EXPECT_TRUE(DisplayNodeIdsContains(2));
  EXPECT_TRUE(DisplayNodeIdsContains(3));
  EXPECT_FALSE(DisplayNodeIdsContains(4));
}

TEST_F(ReadAnythingAppControllerTest,
       DisplayNodeIdsContains_BackwardSelection) {
  basic_tree_data_with_selection_.sel_is_backward = true;
  basic_tree_data_with_selection_.sel_anchor_object_id = 3;
  basic_tree_data_with_selection_.sel_focus_object_id = 2;
  basic_snapshot_.has_tree_data = true;
  basic_snapshot_.tree_data = basic_tree_data_with_selection_;
  OnAXTreeDistilled(basic_snapshot_, {});
  EXPECT_TRUE(DisplayNodeIdsContains(1));
  EXPECT_TRUE(DisplayNodeIdsContains(2));
  EXPECT_TRUE(DisplayNodeIdsContains(3));
  EXPECT_FALSE(DisplayNodeIdsContains(4));
}

TEST_F(ReadAnythingAppControllerTest, DisplayNodeIdsContains_ContentNodes) {
  basic_snapshot_.nodes.resize(6);
  basic_snapshot_.nodes[3].child_ids = {5, 6};
  basic_snapshot_.nodes[4].id = 5;
  basic_snapshot_.nodes[5].id = 6;

  OnAXTreeDistilled(basic_snapshot_, {3, 4});
  EXPECT_TRUE(DisplayNodeIdsContains(1));
  EXPECT_FALSE(DisplayNodeIdsContains(2));
  EXPECT_TRUE(DisplayNodeIdsContains(3));
  EXPECT_TRUE(DisplayNodeIdsContains(4));
  EXPECT_TRUE(DisplayNodeIdsContains(5));
  EXPECT_TRUE(DisplayNodeIdsContains(6));
}

TEST_F(ReadAnythingAppControllerTest,
       DisplayNodeIdsContains_NoSelectionOrContentNodes) {
  OnAXTreeDistilled(basic_snapshot_, {});
  EXPECT_FALSE(DisplayNodeIdsContains(1));
  EXPECT_FALSE(DisplayNodeIdsContains(2));
  EXPECT_FALSE(DisplayNodeIdsContains(3));
  EXPECT_FALSE(DisplayNodeIdsContains(4));
}
