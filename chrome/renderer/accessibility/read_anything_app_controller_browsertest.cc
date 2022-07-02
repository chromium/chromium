// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/accessibility/read_anything_app_controller.h"

#include <string>
#include <vector>

#include "chrome/test/base/chrome_render_view_test.h"
#include "content/public/renderer/render_frame.h"

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
  }

  void OnFontNameChange(const std::string& new_font_name) {
    controller_->OnFontNameChange(new_font_name);
  }

  void OnAXTreeDistilled(const ui::AXTreeUpdate& snapshot,
                         const std::vector<ui::AXNodeID>& content_node_ids) {
    controller_->OnAXTreeDistilled(snapshot, content_node_ids);
  }

  std::vector<ui::AXNodeID> ContentNodeIds() {
    return controller_->ContentNodeIds();
  }

  std::string FontName() { return controller_->FontName(); }

  std::vector<ui::AXNodeID> GetChildren(ui::AXNodeID ax_node_id) {
    return controller_->GetChildren(ax_node_id);
  }

  uint32_t GetHeadingLevel(ui::AXNodeID ax_node_id) {
    return controller_->GetHeadingLevel(ax_node_id);
  }

  std::string GetTextContent(ui::AXNodeID ax_node_id) {
    return controller_->GetTextContent(ax_node_id);
  }

  std::string GetUrl(ui::AXNodeID ax_node_id) {
    return controller_->GetUrl(ax_node_id);
  }

  bool IsHeading(ui::AXNodeID ax_node_id) {
    return controller_->IsHeading(ax_node_id);
  }

  bool IsLink(ui::AXNodeID ax_node_id) {
    return controller_->IsLink(ax_node_id);
  }

  bool IsParagraph(ui::AXNodeID ax_node_id) {
    return controller_->IsParagraph(ax_node_id);
  }

  bool IsStaticText(ui::AXNodeID ax_node_id) {
    return controller_->IsStaticText(ax_node_id);
  }

  ui::AXTreeUpdate basic_snapshot_;

 private:
  // ReadAnythingAppController constructor and destructor are private so it's
  // not accessible by std::make_unique.
  ReadAnythingAppController* controller_ = nullptr;
};

TEST_F(ReadAnythingAppControllerTest, FontName) {
  std::string font_name = "Roboto";
  OnFontNameChange(font_name);
  EXPECT_EQ(font_name, FontName());
}

TEST_F(ReadAnythingAppControllerTest, ContentNodeIds) {
  std::vector<ui::AXNodeID> content_node_ids = {2, 4};
  OnAXTreeDistilled(basic_snapshot_, content_node_ids);
  EXPECT_EQ(content_node_ids.size(), ContentNodeIds().size());
  for (size_t i = 0; i < content_node_ids.size(); i++) {
    EXPECT_EQ(content_node_ids[i], ContentNodeIds()[i]);
  }
}

TEST_F(ReadAnythingAppControllerTest, GetChildren) {
  basic_snapshot_.nodes[2].role = ax::mojom::Role::kNone;
  OnAXTreeDistilled(basic_snapshot_, {});
  EXPECT_EQ(2u, GetChildren(1).size());
  EXPECT_EQ(0u, GetChildren(2).size());
  EXPECT_EQ(0u, GetChildren(3).size());
  EXPECT_EQ(0u, GetChildren(4).size());

  EXPECT_EQ(2, GetChildren(1)[0]);
  EXPECT_EQ(4, GetChildren(1)[1]);
}

TEST_F(ReadAnythingAppControllerTest, GetHeadingLevel) {
  uint32_t heading_level = 3;
  basic_snapshot_.nodes[1].role = ax::mojom::Role::kHeading;
  basic_snapshot_.nodes[1].AddIntAttribute(
      ax::mojom::IntAttribute::kHierarchicalLevel, heading_level);
  OnAXTreeDistilled(basic_snapshot_, {});
  EXPECT_EQ(heading_level, GetHeadingLevel(2));
}

TEST_F(ReadAnythingAppControllerTest, GetTextContent) {
  std::string text_content = "Hello";
  std::string missing_text_content = "";
  std::string more_text_content = " world";
  basic_snapshot_.nodes[1].SetName(text_content);
  basic_snapshot_.nodes[1].SetNameFrom(ax::mojom::NameFrom::kContents);
  basic_snapshot_.nodes[2].SetName(missing_text_content);
  basic_snapshot_.nodes[2].SetNameFrom(ax::mojom::NameFrom::kContents);
  basic_snapshot_.nodes[3].SetName(more_text_content);
  basic_snapshot_.nodes[3].SetNameFrom(ax::mojom::NameFrom::kContents);
  OnAXTreeDistilled(basic_snapshot_, {});
  EXPECT_EQ("Hello world", GetTextContent(1));
  EXPECT_EQ(text_content, GetTextContent(2));
  EXPECT_EQ(missing_text_content, GetTextContent(3));
  EXPECT_EQ(more_text_content, GetTextContent(4));
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

TEST_F(ReadAnythingAppControllerTest, IsHeading) {
  basic_snapshot_.nodes[1].role = ax::mojom::Role::kHeading;
  basic_snapshot_.nodes[2].role = ax::mojom::Role::kDocSubtitle;
  basic_snapshot_.nodes[3].role = ax::mojom::Role::kLink;
  OnAXTreeDistilled(basic_snapshot_, {});
  EXPECT_TRUE(IsHeading(2));
  EXPECT_TRUE(IsHeading(3));
  EXPECT_FALSE(IsHeading(4));
}

TEST_F(ReadAnythingAppControllerTest, IsLink) {
  basic_snapshot_.nodes.resize(7);
  for (int i = 4; i < 7; i++) {
    basic_snapshot_.nodes[0].child_ids.push_back(i + 1);
    basic_snapshot_.nodes[i].id = i + 1;
  }

  basic_snapshot_.nodes[1].role = ax::mojom::Role::kDocBackLink;
  basic_snapshot_.nodes[2].role = ax::mojom::Role::kDocBiblioRef;
  basic_snapshot_.nodes[3].role = ax::mojom::Role::kDocGlossRef;
  basic_snapshot_.nodes[4].role = ax::mojom::Role::kDocNoteRef;
  basic_snapshot_.nodes[5].role = ax::mojom::Role::kLink;
  basic_snapshot_.nodes[6].role = ax::mojom::Role::kParagraph;
  OnAXTreeDistilled(basic_snapshot_, {});
  EXPECT_TRUE(IsLink(2));
  EXPECT_TRUE(IsLink(3));
  EXPECT_TRUE(IsLink(4));
  EXPECT_TRUE(IsLink(5));
  EXPECT_TRUE(IsLink(6));
  EXPECT_FALSE(IsLink(7));
}

TEST_F(ReadAnythingAppControllerTest, IsParagraph) {
  basic_snapshot_.nodes[1].role = ax::mojom::Role::kParagraph;
  basic_snapshot_.nodes[2].role = ax::mojom::Role::kListBox;
  OnAXTreeDistilled(basic_snapshot_, {});
  EXPECT_TRUE(IsParagraph(2));
  EXPECT_FALSE(IsParagraph(3));
}

TEST_F(ReadAnythingAppControllerTest, IsStaticText) {
  basic_snapshot_.nodes[1].role = ax::mojom::Role::kStaticText;
  basic_snapshot_.nodes[2].role = ax::mojom::Role::kInlineTextBox;
  basic_snapshot_.nodes[3].role = ax::mojom::Role::kLabelText;
  OnAXTreeDistilled(basic_snapshot_, {});
  EXPECT_TRUE(IsStaticText(2));
  EXPECT_FALSE(IsStaticText(3));
  EXPECT_FALSE(IsStaticText(4));
}
