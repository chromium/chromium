// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/browser_context.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test_utils.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/request_handler_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_tree.h"
#include "ui/views/controls/webview/webview.h"
#include "url/gurl.h"

constexpr base::FilePath::CharType kDocRoot[] =
    FILE_PATH_LITERAL("chrome/test/data/accessibility");

namespace {

int CountOffscreenButtons(const ui::AXTree* tree, const ui::AXNode* node) {
  int count = 0;
  if (node->data().role == ax::mojom::Role::kButton) {
    bool offscreen = false;
    tree->GetTreeBounds(node, &offscreen, /* clip = */ true);
    if (offscreen)
      count++;
  }

  for (const auto* child : node->children())
    count += CountOffscreenButtons(tree, child);

  return count;
}

int CountOffscreenButtons(const ui::AXTreeUpdate& tree_update) {
  ui::AXTree tree(tree_update);
  DCHECK(tree.root());
  return CountOffscreenButtons(&tree, tree.root());
}

}  // namespace

class WebViewBrowserTest : public InProcessBrowserTest {
 public:
  WebViewBrowserTest() : https_server_(net::EmbeddedTestServer::TYPE_HTTPS) {
    https_server_.AddDefaultHandlers(base::FilePath(kDocRoot));
  }

 protected:
  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();

    ASSERT_TRUE(https_server_.Start());

    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();

    ui::AXMode mode = ui::kAXModeComplete;
    mode.set_mode(ui::AXMode::kLabelImages, true);
    web_contents->SetAccessibilityMode(mode);
  }

 protected:
  net::EmbeddedTestServer https_server_;

 private:
  DISALLOW_COPY_AND_ASSIGN(WebViewBrowserTest);
};

// Flaky. https://crbug.com/1013805
IN_PROC_BROWSER_TEST_F(WebViewBrowserTest, DISABLED_ResizeWebView) {
  ui_test_utils::NavigateToURL(
      browser(), https_server_.GetURL("/fixed_size_document.html"));

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser());
  views::WebView* contents_web_view = browser_view->contents_web_view();

  // Resize the web view so that only one of the two buttons fits.
  contents_web_view->SetSize(gfx::Size(300, 140));

  content::WaitForAccessibilityTreeToContainNodeWithName(web_contents,
                                                         "Button 2");

  // Wait for just one button to be offscreen
  while (1 != CountOffscreenButtons(
                  content::GetAccessibilityTreeSnapshot(web_contents))) {
    content::WaitForAccessibilityTreeToChange(web_contents);
  }

  // Now resize the frame to be large enough for both buttons.
  contents_web_view->SetSize(gfx::Size(300, 500));

  // Now no buttons should be offscreen.
  while (0 != CountOffscreenButtons(
                  content::GetAccessibilityTreeSnapshot(web_contents))) {
    content::WaitForAccessibilityTreeToChange(web_contents);
  }
}
