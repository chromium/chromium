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

  void SetThemeForTesting(const std::string& font_name,
                          float font_size,
                          SkColor foreground_color,
                          SkColor background_color) {
    controller_->SetThemeForTesting(font_name, font_size, foreground_color,
                                    background_color);
  }
  void OnAXTreeDistilled(const ui::AXTreeUpdate& snapshot,
                         const std::vector<ui::AXNodeID>& content_node_ids) {
    controller_->OnAXTreeDistilled(snapshot, content_node_ids);
  }

  std::vector<ui::AXNodeID> ContentNodeIds() {
    return controller_->ContentNodeIds();
  }

  std::string FontName() { return controller_->FontName(); }

  float FontSize() { return controller_->FontSize(); }

  SkColor ForegroundColor() { return controller_->ForegroundColor(); }

  SkColor BackgroundColor() { return controller_->BackgroundColor(); }

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
  SetThemeForTesting(font_name, font_size, foreground, background);
  EXPECT_EQ(font_name, FontName());
  EXPECT_EQ(font_size, FontSize());
  EXPECT_EQ(foreground, ForegroundColor());
  EXPECT_EQ(background, BackgroundColor());
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

TEST_F(ReadAnythingAppControllerTest, GetTextContent) {
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
