// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ui_devtools/css_agent.h"

#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "components/ui_devtools/dom_agent.h"
#include "components/ui_devtools/ui_devtools_unittest_utils.h"
#include "components/ui_devtools/ui_element.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ui_devtools {

class FakeUIElement : public UIElement {
 public:
  FakeUIElement(UIElementDelegate* ui_element_delegate)
      : UIElement(UIElementType::ROOT, ui_element_delegate, nullptr) {}

  ~FakeUIElement() override {}
  // Return a vector of pairs of properties' names and values.
  std::vector<std::pair<std::string, std::string>> GetCustomProperties()
      const override {
    return {};
  }
  void GetBounds(gfx::Rect* bounds) const override { *bounds = bounds_; }
  void SetBounds(const gfx::Rect& bounds) override { bounds_ = bounds; }
  void GetVisible(bool* visible) const override { *visible = visible_; }
  void SetVisible(bool visible) override { visible_ = visible; }

  bool visible() const { return visible_; }
  gfx::Rect bounds() const { return bounds_; }

  std::pair<gfx::NativeWindow, gfx::Rect> GetNodeWindowAndBounds()
      const override {
    return {nullptr, gfx::Rect()};
  }

  std::unique_ptr<protocol::Array<std::string>> GetAttributes() const override {
    return protocol::Array<std::string>::create();
  }

 private:
  gfx::Rect bounds_;
  bool visible_;
};

class FakeDOMAgent : public DOMAgent {
 public:
  void OnUIElementAdded(UIElement* parent, UIElement* child) override {
    // nullptr root short circuits everything but adding |child|
    // to the node ID map, which is all we need here.
    DOMAgent::OnUIElementAdded(nullptr, child);
  }

  std::unique_ptr<protocol::DOM::Node> BuildTreeForUIElement(
      UIElement* root) override {
    return BuildDomNodeFromUIElement(root);
  }

  std::vector<UIElement*> CreateChildrenForRoot() override { return {}; }
};

class CSSAgentTest : public testing::Test {
 public:
  void SetUp() override {
    testing::Test::SetUp();
    frontend_channel_ = std::make_unique<FakeFrontendChannel>();
    uber_dispatcher_ =
        std::make_unique<protocol::UberDispatcher>(frontend_channel_.get());
    dom_agent_ = std::make_unique<FakeDOMAgent>();
    dom_agent_->Init(uber_dispatcher_.get());
    css_agent_ = std::make_unique<CSSAgent>(dom_agent_.get());
    css_agent_->Init(uber_dispatcher_.get());
    css_agent_->enable();
    element_ = std::make_unique<FakeUIElement>(dom_agent_.get());
    dom_agent_->OnUIElementAdded(nullptr, element_.get());
  }

 protected:
  using StyleArray = protocol::Array<protocol::CSS::CSSStyle>;

  std::pair<bool, std::unique_ptr<StyleArray>> SetStyle(
      const std::string& style_text,
      int node_id) {
    auto edits = protocol::Array<protocol::CSS::StyleDeclarationEdit>::create();
    auto edit = protocol::CSS::StyleDeclarationEdit::create()
                    .setStyleSheetId(base::IntToString(node_id))
                    .setRange(protocol::CSS::SourceRange::create()
                                  .setStartLine(0)
                                  .setStartColumn(0)
                                  .setEndLine(0)
                                  .setEndColumn(0)
                                  .build())
                    .setText(style_text)
                    .build();
    edits->addItem(std::move(edit));
    std::unique_ptr<StyleArray> output;
    auto response = css_agent_->setStyleTexts(std::move(edits), &output);
    return {response.isSuccess(), std::move(output)};
  }

  std::string GetValueForProperty(protocol::CSS::CSSStyle* style,
                                  const std::string& property_name) {
    auto* properties = style->getCssProperties();
    for (size_t i = 0; i < properties->length(); ++i) {
      auto* property = properties->get(i);
      if (property->getName() == property_name) {
        return property->getValue();
      }
    }
    return std::string();
  }
  int GetStyleSheetChangedCount(int node_id) {
    return frontend_channel_->CountProtocolNotificationMessage(
        base::StringPrintf("{\"method\":\"CSS.styleSheetChanged\",\"params\":{"
                           "\"styleSheetId\":\"%d\"}}",
                           node_id));
  }

  CSSAgent* css_agent() { return css_agent_.get(); }
  DOMAgent* dom_agent() { return dom_agent_.get(); }
  FakeUIElement* element() { return element_.get(); }

 private:
  std::unique_ptr<CSSAgent> css_agent_;
  std::unique_ptr<DOMAgent> dom_agent_;
  std::unique_ptr<FakeFrontendChannel> frontend_channel_;
  std::unique_ptr<protocol::UberDispatcher> uber_dispatcher_;
  std::unique_ptr<FakeUIElement> element_;
};

TEST_F(CSSAgentTest, UnrecognizedNodeFails) {
  EXPECT_FALSE(SetStyle("x : 25", 42).first);
}

TEST_F(CSSAgentTest, UnrecognizedKeyFails) {
  element()->SetVisible(true);
  element()->SetBounds(gfx::Rect(1, 2, 3, 4));

  auto result = SetStyle("nonsense : 3.14", element()->node_id());

  EXPECT_FALSE(result.first);
  EXPECT_FALSE(result.second);
  // Ensure element didn't change.
  EXPECT_TRUE(element()->visible());
  EXPECT_EQ(element()->bounds(), gfx::Rect(1, 2, 3, 4));
}

TEST_F(CSSAgentTest, UnrecognizedValueFails) {
  element()->SetVisible(true);
  element()->SetBounds(gfx::Rect(1, 2, 3, 4));

  auto result = SetStyle("visibility : banana", element()->node_id());
  EXPECT_FALSE(result.first);
  EXPECT_FALSE(result.second);
  // Ensure element didn't change.
  EXPECT_TRUE(element()->visible());
  EXPECT_EQ(element()->bounds(), gfx::Rect(1, 2, 3, 4));
}

TEST_F(CSSAgentTest, SettingVisibility) {
  element()->SetVisible(false);
  DCHECK(!element()->visible());

  auto result = SetStyle("visibility: 1", element()->node_id());
  EXPECT_TRUE(result.first);
  EXPECT_TRUE(element()->visible());

  EXPECT_EQ(result.second->length(), 1U);
  protocol::CSS::CSSStyle* style = result.second->get(0);
  EXPECT_EQ(style->getStyleSheetId("default"),
            base::IntToString(element()->node_id()));
  EXPECT_EQ(GetValueForProperty(style, "visibility"), "1");
}

TEST_F(CSSAgentTest, SettingX) {
  DCHECK_EQ(element()->bounds().x(), 0);

  auto result = SetStyle("x: 500", element()->node_id());
  EXPECT_TRUE(result.first);
  EXPECT_EQ(element()->bounds().x(), 500);
  EXPECT_EQ(result.second->length(), 1U);
  protocol::CSS::CSSStyle* style = result.second->get(0);
  EXPECT_EQ(style->getStyleSheetId("default"),
            base::IntToString(element()->node_id()));
  EXPECT_EQ(GetValueForProperty(style, "x"), "500");
}

TEST_F(CSSAgentTest, SettingY) {
  DCHECK_EQ(element()->bounds().y(), 0);

  auto result = SetStyle("y: 100", element()->node_id());
  EXPECT_TRUE(result.first);
  EXPECT_EQ(element()->bounds().y(), 100);
  EXPECT_EQ(result.second->length(), 1U);
  protocol::CSS::CSSStyle* style = result.second->get(0);
  EXPECT_EQ(style->getStyleSheetId("default"),
            base::IntToString(element()->node_id()));
  EXPECT_EQ(GetValueForProperty(style, "y"), "100");
}
TEST_F(CSSAgentTest, SettingWidth) {
  DCHECK_EQ(element()->bounds().width(), 0);

  auto result = SetStyle("width: 20", element()->node_id());
  EXPECT_TRUE(result.first);
  EXPECT_EQ(element()->bounds().width(), 20);
  EXPECT_EQ(result.second->length(), 1U);
  protocol::CSS::CSSStyle* style = result.second->get(0);
  EXPECT_EQ(style->getStyleSheetId("default"),
            base::IntToString(element()->node_id()));
  EXPECT_EQ(GetValueForProperty(style, "width"), "20");
}
TEST_F(CSSAgentTest, SettingHeight) {
  DCHECK_EQ(element()->bounds().height(), 0);

  auto result = SetStyle("height: 30", element()->node_id());
  EXPECT_TRUE(result.first);
  EXPECT_EQ(element()->bounds().height(), 30);
  EXPECT_EQ(result.second->length(), 1U);
  protocol::CSS::CSSStyle* style = result.second->get(0);
  EXPECT_EQ(style->getStyleSheetId("default"),
            base::IntToString(element()->node_id()));
  EXPECT_EQ(GetValueForProperty(style, "height"), "30");
}

TEST_F(CSSAgentTest, SettingAll) {
  DCHECK(element()->bounds() == gfx::Rect());
  DCHECK(element()->visible());

  // Throw in odd whitespace while we're at it.
  std::string new_text(
      "\ny: 25; width: 50;   visibility:0; height: 30;\nx: 9000;\n\n");
  auto result = SetStyle(new_text, element()->node_id());
  EXPECT_TRUE(result.first);
  EXPECT_EQ(element()->bounds(), gfx::Rect(9000, 25, 50, 30));
  EXPECT_FALSE(element()->visible());
  EXPECT_EQ(result.second->length(), 1U);
  protocol::CSS::CSSStyle* style = result.second->get(0);
  EXPECT_EQ(style->getStyleSheetId("default"),
            base::IntToString(element()->node_id()));
  EXPECT_EQ(GetValueForProperty(style, "x"), "9000");
  EXPECT_EQ(GetValueForProperty(style, "y"), "25");
  EXPECT_EQ(GetValueForProperty(style, "width"), "50");
  EXPECT_EQ(GetValueForProperty(style, "height"), "30");
  EXPECT_EQ(GetValueForProperty(style, "visibility"), "0");
}

TEST_F(CSSAgentTest, UpdateOnBoundsChange) {
  FakeUIElement another_element(dom_agent());
  EXPECT_EQ(0, GetStyleSheetChangedCount(element()->node_id()));
  EXPECT_EQ(0, GetStyleSheetChangedCount(another_element.node_id()));
  css_agent()->OnElementBoundsChanged(element());
  EXPECT_EQ(1, GetStyleSheetChangedCount(element()->node_id()));
  EXPECT_EQ(0, GetStyleSheetChangedCount(another_element.node_id()));
  css_agent()->OnElementBoundsChanged(&another_element);
  EXPECT_EQ(1, GetStyleSheetChangedCount(element()->node_id()));
  EXPECT_EQ(1, GetStyleSheetChangedCount(another_element.node_id()));

  css_agent()->OnElementBoundsChanged(&another_element);
  EXPECT_EQ(1, GetStyleSheetChangedCount(element()->node_id()));
  EXPECT_EQ(2, GetStyleSheetChangedCount(another_element.node_id()));
}
}  // namespace ui_devtools
