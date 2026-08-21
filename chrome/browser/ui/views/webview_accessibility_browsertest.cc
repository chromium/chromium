// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <optional>

#include "base/check.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/browser_accessibility_state.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/scoped_accessibility_mode.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/scoped_accessibility_mode_override.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/request_handler_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node.h"
#include "ui/accessibility/ax_tree.h"
#include "ui/accessibility/ax_tree_id.h"
#include "ui/accessibility/platform/browser_accessibility.h"
#include "ui/accessibility/platform/browser_accessibility_manager.h"
#include "ui/views/accessibility/tree/widget_ax_manager.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/widget/widget.h"
#include "url/gurl.h"

constexpr base::FilePath::CharType kDocRoot[] =
    FILE_PATH_LITERAL("chrome/test/data/accessibility");

namespace {

int CountOffscreenButtons(const ui::AXTree* tree, const ui::AXNode* node) {
  int count = 0;
  if (node->GetRole() == ax::mojom::Role::kButton) {
    bool offscreen = false;
    tree->GetTreeBounds(node, &offscreen, /*clip_bounds=*/true);
    if (offscreen) {
      count++;
    }
  }

  for (const ui::AXNode* child : node->children()) {
    count += CountOffscreenButtons(tree, child);
  }

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

  WebViewBrowserTest(const WebViewBrowserTest&) = delete;
  WebViewBrowserTest& operator=(const WebViewBrowserTest&) = delete;

 protected:
  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();

    ASSERT_TRUE(https_server_.Start());

    scoped_accessibility_mode_.emplace(
        browser()->GetTabStripModel()->GetActiveWebContents(),
        ui::kAXModeComplete | ui::AXMode::kLabelImages);
  }

  void TearDownOnMainThread() override { scoped_accessibility_mode_.reset(); }

  net::EmbeddedTestServer https_server_;

 private:
  std::optional<content::ScopedAccessibilityModeOverride>
      scoped_accessibility_mode_;
};

// Flaky. https://crbug.com/40652843
IN_PROC_BROWSER_TEST_F(WebViewBrowserTest, DISABLED_ResizeWebView) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_server_.GetURL("/fixed_size_document.html")));

  content::WebContents* web_contents =
      browser()->GetTabStripModel()->GetActiveWebContents();

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

class WebViewInAXTreeBrowserTest : public WebViewBrowserTest {
 public:
  WebViewInAXTreeBrowserTest() {
    scoped_feature_list_.InitAndEnableFeature(
        ::features::kAccessibilityTreeForViews);
  }

 protected:
  void SetUpOnMainThread() override {
    WebViewBrowserTest::SetUpOnMainThread();
    if (!views::ViewAccessibility::IsViewsAccessibilityTreeEnabled()) {
      GTEST_SKIP() << "This platform builds no Views accessibility tree.";
    }
    scoped_ax_mode_for_process_ =
        content::BrowserAccessibilityState::GetInstance()
            ->CreateScopedModeForProcess(ui::AXMode::kNativeAPIs);
  }

  void TearDownOnMainThread() override {
    scoped_ax_mode_for_process_.reset();
    WebViewBrowserTest::TearDownOnMainThread();
  }

  BrowserView* browser_view() {
    return BrowserView::GetBrowserViewForBrowser(browser());
  }

  views::WebView* contents_web_view() {
    return browser_view()->contents_web_view();
  }

  views::WidgetAXManager* widget_ax_manager() {
    views::Widget* widget = browser_view()->GetWidget();
    return widget ? widget->ax_manager() : nullptr;
  }

  ui::BrowserAccessibilityManager* GetWebContentsAXManager() {
    content::WebContents* contents =
        browser()->GetTabStripModel()->GetActiveWebContents();
    return contents ? ui::BrowserAccessibilityManager::FromID(
                          contents->GetPrimaryMainFrame()->GetAXTreeID())
                    : nullptr;
  }

  [[nodiscard]] bool NavigateAndWaitForAXManager() {
    if (!ui_test_utils::NavigateToURL(
            browser(), https_server_.GetURL("/fixed_size_document.html"))) {
      return false;
    }
    return base::test::RunUntil(
        [&]() { return GetWebContentsAXManager() != nullptr; });
  }

  [[nodiscard]] bool WaitForParentTreeID(ui::AXTreeID expected) {
    return base::test::RunUntil([&]() {
      ui::BrowserAccessibilityManager* manager = GetWebContentsAXManager();
      return manager && manager->GetParentTreeID() == expected;
    });
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<content::ScopedAccessibilityMode> scoped_ax_mode_for_process_;
};

// This test validates that the WebContents is properly connected to the parent
// AXTree.
IN_PROC_BROWSER_TEST_F(WebViewInAXTreeBrowserTest,
                       WebContentsPointsToParentAXTree) {
  ASSERT_TRUE(NavigateAndWaitForAXManager());

  views::WidgetAXManager* manager = widget_ax_manager();
  ASSERT_TRUE(manager);
  const ui::AXTreeID views_tree_id = manager->GetAXTreeID();
  ASSERT_NE(views_tree_id, ui::AXTreeIDUnknown());

  EXPECT_EQ(contents_web_view()->GetViewAccessibility().GetChildTreeID(),
            GetWebContentsAXManager()->GetTreeID());

  ASSERT_TRUE(WaitForParentTreeID(views_tree_id));

  // Wait for the Views tree to serialize so the connection is made.
  ASSERT_TRUE(base::test::RunUntil([&]() {
    ui::BrowserAccessibilityManager* manager = GetWebContentsAXManager();
    return manager &&
           manager->GetParentNodeFromParentTreeAsBrowserAccessibility();
  }));

  ui::BrowserAccessibilityManager* web_manager = GetWebContentsAXManager();
  ui::BrowserAccessibility* host =
      web_manager->GetParentNodeFromParentTreeAsBrowserAccessibility();
  ASSERT_TRUE(host);
  EXPECT_EQ(host->manager(),
            ui::BrowserAccessibilityManager::FromID(views_tree_id));

  ui::BrowserAccessibility* web_root =
      web_manager->GetBrowserAccessibilityRoot();
  ASSERT_TRUE(web_root);
  EXPECT_TRUE(web_root->PlatformGetParent());
}

IN_PROC_BROWSER_TEST_F(WebViewInAXTreeBrowserTest,
                       LosingTheWebContentsTakesTheParentAway) {
  ASSERT_TRUE(NavigateAndWaitForAXManager());

  views::WidgetAXManager* manager = widget_ax_manager();
  ASSERT_TRUE(manager);
  ASSERT_TRUE(WaitForParentTreeID(manager->GetAXTreeID()));

  ui::BrowserAccessibilityManager* web_manager = GetWebContentsAXManager();
  ASSERT_TRUE(web_manager);

  contents_web_view()->SetWebContents(nullptr);

  EXPECT_EQ(web_manager->GetParentTreeID(), ui::AXTreeIDUnknown());
}

// This test validates that a temporary disconnect signal should break the link
// between the Views AXTree and the web content AXTree in both directions.
IN_PROC_BROWSER_TEST_F(WebViewInAXTreeBrowserTest,
                       DisconnectTakesBothDirectionsOfTheBridge) {
  ASSERT_TRUE(NavigateAndWaitForAXManager());

  views::WidgetAXManager* manager = widget_ax_manager();
  ASSERT_TRUE(manager);
  const ui::AXTreeID views_tree_id = manager->GetAXTreeID();
  ASSERT_TRUE(WaitForParentTreeID(views_tree_id));

  {
    auto lock = contents_web_view()->DisconnectWebContentsAccessibility();

    EXPECT_EQ(contents_web_view()->GetViewAccessibility().GetChildTreeID(),
              ui::AXTreeIDUnknown());
    EXPECT_EQ(GetWebContentsAXManager()->GetParentTreeID(),
              ui::AXTreeIDUnknown());
  }

  // The lock goes away, so both directions come back and agree.
  EXPECT_NE(contents_web_view()->GetViewAccessibility().GetChildTreeID(),
            ui::AXTreeIDUnknown());
  EXPECT_TRUE(WaitForParentTreeID(views_tree_id));
}
