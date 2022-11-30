// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/bubble/webui_bubble_manager.h"

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/views/bubble/webui_bubble_dialog_view.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"
#include "ui/webui/mojo_bubble_web_ui_controller.h"

class TestWebUIController : public ui::MojoBubbleWebUIController {
  WEB_UI_CONTROLLER_TYPE_DECL();
};
WEB_UI_CONTROLLER_TYPE_IMPL(TestWebUIController)

template <>
class BubbleContentsWrapperT<TestWebUIController>
    : public BubbleContentsWrapper {
 public:
  BubbleContentsWrapperT(const GURL& webui_url,
                         content::BrowserContext* browser_context,
                         int task_manager_string_id,
                         bool webui_resizes_host = true,
                         bool esc_closes_ui = true)
      : BubbleContentsWrapper(webui_url,
                              browser_context,
                              task_manager_string_id,
                              webui_resizes_host,
                              esc_closes_ui) {}
  void ReloadWebContents() override {}
};

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
    bubble_manager_ =
        std::make_unique<WebUIBubbleManagerT<TestWebUIController>>(
            BrowserView::GetBrowserViewForBrowser(browser()),
            browser()->profile(), GURL("chrome://test"), 1);
  }
  void TearDownOnMainThread() override {
    bubble_manager_.reset();
    InProcessBrowserTest::TearDownOnMainThread();
  }

  WebUIBubbleManager* bubble_manager() { return bubble_manager_.get(); }

  void DestroyBubbleManager() { bubble_manager_.reset(); }

 private:
  std::unique_ptr<WebUIBubbleManager> bubble_manager_;
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

// Ensures that the WebUI bubble is destroyed synchronously with the manager.
// This guards against a potential UAF crash (see crbug.com/1345546).
IN_PROC_BROWSER_TEST_F(WebUIBubbleManagerBrowserTest,
                       ManagerDestructionClosesBubble) {
  EXPECT_EQ(nullptr, bubble_manager()->GetBubbleWidget());
  bubble_manager()->ShowBubble();
  EXPECT_NE(nullptr, bubble_manager()->GetBubbleWidget());

  base::WeakPtr<WebUIBubbleDialogView> bubble_view =
      bubble_manager()->bubble_view_for_testing();
  EXPECT_TRUE(bubble_view);
  bubble_view->ShowUI();
  EXPECT_TRUE(bubble_manager()->GetBubbleWidget()->IsVisible());

  // Destroy the bubble manager without explicitly destroying the bubble. Ensure
  // the bubble is closed synchronously.
  DestroyBubbleManager();
  EXPECT_FALSE(bubble_view);
}
