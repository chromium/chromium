// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ui_devtools/views/page_agent_views.h"

#include "base/command_line.h"
#include "components/ui_devtools/agent_util.h"
#include "components/ui_devtools/ui_devtools_unittest_utils.h"
#include "components/ui_devtools/ui_element.h"
#include "components/ui_devtools/views/dom_agent_views.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/views_switches.h"

namespace ui_devtools {

class PageAgentViewsTest : public views::ViewsTestBase {
 public:
  void SetUp() override {
    frontend_channel_ = std::make_unique<FakeFrontendChannel>();
    uber_dispatcher_ =
        std::make_unique<protocol::UberDispatcher>(frontend_channel_.get());
    dom_agent_ = DOMAgentViews::Create();
    dom_agent_->Init(uber_dispatcher_.get());
    page_agent_ = std::make_unique<PageAgentViews>(dom_agent_.get());
    page_agent_->Init(uber_dispatcher_.get());
    page_agent_->enable();
    views::ViewsTestBase::SetUp();
  }

  void TearDown() override {
    page_agent_.reset();
    dom_agent_.reset();
    uber_dispatcher_.reset();
    frontend_channel_.reset();
    views::ViewsTestBase::TearDown();
  }

  bool HasSource(protocol::Array<protocol::Page::FrameResource>* resources,
                 std::string source) {
    for (const auto& resource : *resources) {
      if (resource->getUrl() == kChromiumCodeSearchSrcURL + source)
        return true;
    }
    return false;
  }

  bool VerifyResources(
      protocol::Array<protocol::Page::FrameResource>* resources) {
    std::stack<UIElement*> elements;
    elements.emplace(dom_agent()->element_root());

    UIElement* cur_element;
    while (!elements.empty()) {
      cur_element = elements.top();
      elements.pop();
      for (const auto& source : cur_element->GetSources()) {
        if (!HasSource(resources, source.path_ + "?l=" +
                                      base::NumberToString(source.line_)))
          return false;
      }

      for (auto* child : cur_element->children()) {
        elements.emplace(child);
      }
    }

    return true;
  }

  std::pair<bool, std::string> GetResourceContent(std::string url_input) {
    std::string result;
    bool out_bool;

    auto response =
        page_agent()->getResourceContent("1", url_input, &result, &out_bool);
    return {response.isSuccess(), result};
  }

 protected:
  PageAgentViews* page_agent() { return page_agent_.get(); }
  DOMAgentViews* dom_agent() { return dom_agent_.get(); }
  bool devtools_dismiss_override() {
    return page_agent()->devtools_dismiss_override();
  }

 private:
  std::unique_ptr<PageAgentViews> page_agent_;
  std::unique_ptr<DOMAgentViews> dom_agent_;
  std::unique_ptr<FakeFrontendChannel> frontend_channel_;
  std::unique_ptr<protocol::UberDispatcher> uber_dispatcher_;
};

TEST_F(PageAgentViewsTest, ToggleBubbleLock) {
  DCHECK(!devtools_dismiss_override());

  // Enable bubble locking.
  page_agent()->reload(true);

  // Check that |devtools_dismiss_override_| is set to true.
  EXPECT_TRUE(devtools_dismiss_override());

  // Call again to disable bubble locking.
  page_agent()->reload(true);

  // Check that |devtools_dismiss_override_| set back to false.
  EXPECT_FALSE(devtools_dismiss_override());
}

TEST_F(PageAgentViewsTest, ToggleViewDebugRects) {
  std::unique_ptr<protocol::DOM::Node> root;
  dom_agent()->getDocument(&root);

  DCHECK(!base::CommandLine::ForCurrentProcess()->HasSwitch(
      views::switches::kDrawViewBoundsRects));

  // Enable debug rectangles.
  page_agent()->reload(false);

  // Check that debug rectangles switch is present.
  EXPECT_TRUE(base::CommandLine::ForCurrentProcess()->HasSwitch(
      views::switches::kDrawViewBoundsRects));

  // Disable debug rectangles.
  page_agent()->reload(false);

  // Check that the debug rectangles switch is removed.
  EXPECT_FALSE(base::CommandLine::ForCurrentProcess()->HasSwitch(
      views::switches::kDrawViewBoundsRects));
}

TEST_F(PageAgentViewsTest, ResetOnDisable) {
  std::unique_ptr<protocol::DOM::Node> root;
  dom_agent()->getDocument(&root);

  // Enable both debug rectangles and bubble locking.
  page_agent()->reload(false);
  page_agent()->reload(true);
  DCHECK(devtools_dismiss_override());
  DCHECK(base::CommandLine::ForCurrentProcess()->HasSwitch(
      views::switches::kDrawViewBoundsRects));

  // Disable PageAgentViews, as is done on UI DevTools closing.
  page_agent()->disable();

  // Check that both bubble locking and debug rectangles are off.
  EXPECT_FALSE(devtools_dismiss_override());
  EXPECT_FALSE(base::CommandLine::ForCurrentProcess()->HasSwitch(
      views::switches::kDrawViewBoundsRects));
}

TEST_F(PageAgentViewsTest, GetResourceTree) {
  std::unique_ptr<protocol::Page::FrameResourceTree> resource_tree;
  EXPECT_TRUE(page_agent()->getResourceTree(&resource_tree).isSuccess());

  protocol::Page::Frame* frame_object = resource_tree->getFrame();
  EXPECT_EQ(frame_object->getId(), "1");
  EXPECT_EQ(frame_object->getUrl(), kChromiumCodeSearchURL);

  EXPECT_TRUE(VerifyResources(resource_tree->getResources()));
}

TEST_F(PageAgentViewsTest, GetResourceContent) {
  auto result = GetResourceContent(
      "chromium/src/components/test/data/ui_devtools/test_file.cc?l=0");

  EXPECT_TRUE(result.first);
  EXPECT_NE(result.second, "");
}

TEST_F(PageAgentViewsTest, GetResourceContentFailsOnBadURL) {
  // Test if URL doesn't have src/.
  auto result = GetResourceContent(
      "incorrect/components/test/data/ui_devtools/test_file.cc?l=0");

  EXPECT_FALSE(result.first);
  EXPECT_EQ(result.second, "");

  // Test if URL doesn't have line number.
  result = GetResourceContent(
      "chromium/src/components/test/data/ui_devtools/test_file.cc");

  EXPECT_FALSE(result.first);
  EXPECT_EQ(result.second, "");

  // Test if URL isn't a real file.
  result = GetResourceContent("chromium/src/not/a/real/file.cc?l=0");

  EXPECT_FALSE(result.first);
  EXPECT_EQ(result.second, "");
}

}  // namespace ui_devtools