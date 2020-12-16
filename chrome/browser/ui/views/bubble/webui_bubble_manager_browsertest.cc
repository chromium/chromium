// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/bubble/webui_bubble_manager.h"

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/views/bubble/webui_bubble_dialog_view.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"

namespace {

class TestWebUIBubbleManager : public WebUIBubbleManagerBase {
 public:
  explicit TestWebUIBubbleManager(Browser* browser)
      : WebUIBubbleManagerBase(IDS_ACCNAME_TAB_SEARCH,
                               BrowserView::GetBrowserViewForBrowser(browser),
                               browser->profile(),
                               GURL("chrome://about")) {}
  TestWebUIBubbleManager(const TestWebUIBubbleManager&) = delete;
  const TestWebUIBubbleManager& operator=(const TestWebUIBubbleManager&) =
      delete;
  ~TestWebUIBubbleManager() override = default;

 private:
  std::unique_ptr<WebUIBubbleView> CreateWebView() override {
    return std::make_unique<WebUIBubbleView>(browser_context());
  }
};

}  // namespace

class WebUIBubbleManagerBrowserTest : public InProcessBrowserTest {
 public:
  WebUIBubbleManagerBrowserTest() = default;
  WebUIBubbleManagerBrowserTest(const WebUIBubbleManagerBrowserTest&) = delete;
  const WebUIBubbleManagerBrowserTest& operator=(
      const WebUIBubbleManagerBrowserTest&) = delete;
  ~WebUIBubbleManagerBrowserTest() override = default;

  // content::BrowserTestBase:
  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    bubble_manager_ = std::make_unique<TestWebUIBubbleManager>(browser());
  }
  void TearDownOnMainThread() override {
    auto* widget = bubble_manager_->GetBubbleWidget();
    if (widget)
      widget->CloseNow();
    bubble_manager()->ResetWebViewForTesting();
    InProcessBrowserTest::TearDownOnMainThread();
  }

  TestWebUIBubbleManager* bubble_manager() { return bubble_manager_.get(); }

 private:
  std::unique_ptr<TestWebUIBubbleManager> bubble_manager_;
};

IN_PROC_BROWSER_TEST_F(WebUIBubbleManagerBrowserTest, CreateAndCloseBubble) {
  EXPECT_EQ(nullptr, bubble_manager()->GetBubbleWidget());
  bubble_manager()->ShowBubble();
  EXPECT_NE(nullptr, bubble_manager()->GetBubbleWidget());
  EXPECT_FALSE(bubble_manager()->GetBubbleWidget()->IsClosed());

  bubble_manager()->CloseBubble();
  EXPECT_TRUE(bubble_manager()->GetBubbleWidget()->IsClosed());
}

IN_PROC_BROWSER_TEST_F(WebUIBubbleManagerBrowserTest,
                       ShowUISetsBubbleWidgetVisible) {
  EXPECT_EQ(nullptr, bubble_manager()->GetBubbleWidget());
  bubble_manager()->ShowBubble();
  EXPECT_NE(nullptr, bubble_manager()->GetBubbleWidget());
  EXPECT_FALSE(bubble_manager()->GetBubbleWidget()->IsClosed());
  EXPECT_FALSE(bubble_manager()->GetBubbleWidget()->IsVisible());

  bubble_manager()->bubble_view_for_testing()->ShowUI();
  EXPECT_TRUE(bubble_manager()->GetBubbleWidget()->IsVisible());

  bubble_manager()->CloseBubble();
  EXPECT_TRUE(bubble_manager()->GetBubbleWidget()->IsClosed());
}
