// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tab_search/tab_search_bubble_view.h"

#include <string>

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/tabs/tab_search_button.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/common/webui_url_constants.h"
#include "content/public/test/browser_test.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"

class TabSearchBubbleBrowserTest : public InProcessBrowserTest {
 public:
  TabSearchBubbleBrowserTest() {
    feature_list_.InitAndEnableFeature(features::kTabSearch);
  }
  TabSearchBubbleBrowserTest(TabSearchBubbleBrowserTest&) = delete;
  TabSearchBubbleBrowserTest& operator=(TabSearchBubbleBrowserTest&) = delete;
  ~TabSearchBubbleBrowserTest() override = default;

  // InProcessBrowserTest:
  void SetUpOnMainThread() override {
    auto* anchor = BrowserView::GetBrowserViewForBrowser(browser())->toolbar();
    auto bubble_delegate =
        std::make_unique<TabSearchBubbleView>(browser()->profile(), anchor);
    bubble_view_ = bubble_delegate.get();
    bubble_ = views::BubbleDialogDelegateView::CreateBubble(
        bubble_delegate.release());
  }

  views::Widget* bubble() { return bubble_; }
  TabSearchBubbleView* bubble_view() { return bubble_view_; }

 private:
  views::Widget* bubble_ = nullptr;
  TabSearchBubbleView* bubble_view_ = nullptr;
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(TabSearchBubbleBrowserTest, TestShowAndCloseBubble) {
  EXPECT_NE(nullptr, bubble());

  // Test showing the bubble via ShowBubble() method.
  EXPECT_FALSE(bubble()->IsVisible());
  bubble_view()->ShowBubble();
  EXPECT_TRUE(bubble()->IsVisible());

  // Test closing the bubble via CloseBubble() method.
  EXPECT_FALSE(bubble()->IsClosed());
  bubble_view()->CloseBubble();
  EXPECT_TRUE(bubble()->IsClosed());

  bubble()->CloseNow();
}

IN_PROC_BROWSER_TEST_F(TabSearchBubbleBrowserTest, TestBubbleResize) {
  EXPECT_NE(nullptr, bubble());

  // Show the bubble
  EXPECT_FALSE(bubble()->IsVisible());
  bubble_view()->ShowBubble();
  EXPECT_TRUE(bubble()->IsVisible());

  views::WebView* const web_view = bubble_view()->web_view_for_testing();
  constexpr gfx::Size web_view_initial_size(100, 100);
  web_view->SetPreferredSize(gfx::Size(100, 100));
  bubble_view()->OnWebViewSizeChanged();
  const gfx::Size widget_initial_size =
      bubble()->GetWindowBoundsInScreen().size();
  // The bubble should be at least as big as the webview.
  EXPECT_GE(widget_initial_size.width(), web_view_initial_size.width());
  EXPECT_GE(widget_initial_size.height(), web_view_initial_size.height());

  // Resize the webview.
  constexpr gfx::Size web_view_final_size(200, 200);
  web_view->SetPreferredSize(web_view_final_size);
  bubble_view()->OnWebViewSizeChanged();

  // Ensure the bubble resizes as expected.
  const gfx::Size widget_final_size =
      bubble()->GetWindowBoundsInScreen().size();
  EXPECT_LT(widget_initial_size.width(), widget_final_size.width());
  EXPECT_LT(widget_initial_size.height(), widget_final_size.height());
  // The bubble should be at least as big as the webview.
  EXPECT_GE(widget_final_size.width(), web_view_final_size.width());
  EXPECT_GE(widget_final_size.height(), web_view_final_size.height());

  bubble()->CloseNow();
}

class TabSearchBubbleBrowserUITest : public DialogBrowserTest {
 public:
  TabSearchBubbleBrowserUITest() {
    feature_list_.InitAndEnableFeature(features::kTabSearch);
  }
  TabSearchBubbleBrowserUITest(TabSearchBubbleBrowserUITest&) = delete;
  TabSearchBubbleBrowserUITest& operator=(TabSearchBubbleBrowserUITest&) =
      delete;
  ~TabSearchBubbleBrowserUITest() override = default;

  // DialogBrowserTest:
  void ShowUi(const std::string& name) override {
    AppendTab(chrome::kChromeUISettingsURL);
    AppendTab(chrome::kChromeUIHistoryURL);
    AppendTab(chrome::kChromeUIBookmarksURL);
    BrowserView* browser_view =
        BrowserView::GetBrowserViewForBrowser(browser());
    DCHECK(browser_view);
    views::View* anchor_view = browser_view->GetTabSearchButton();
    TabSearchBubbleView::CreateTabSearchBubble(browser()->profile(),
                                               anchor_view);
  }

  void AppendTab(std::string url) {
    chrome::AddTabAt(browser(), GURL(url), -1, true);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Invokes a tab search bubble.
IN_PROC_BROWSER_TEST_F(TabSearchBubbleBrowserUITest, InvokeUi_default) {
  ShowAndVerifyUi();
}
