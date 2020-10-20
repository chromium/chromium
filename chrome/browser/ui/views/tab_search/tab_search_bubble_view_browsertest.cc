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
#include "chrome/browser/ui/webui/tab_search/tab_search_ui.h"
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
    auto* anchor =
        BrowserView::GetBrowserViewForBrowser(browser())->GetTabSearchButton();
    auto bubble_delegate =
        std::make_unique<TabSearchBubbleView>(browser()->profile(), anchor);
    bubble_view_ = bubble_delegate.get();
    bubble_ = views::WebBubbleDialogView::CreateWebBubbleDialog<TabSearchUI>(
        std::move(bubble_delegate), GURL(chrome::kChromeUITabSearchURL));
  }

  views::Widget* bubble() { return bubble_; }
  TabSearchBubbleView* bubble_view() { return bubble_view_; }

 private:
  views::Widget* bubble_ = nullptr;
  TabSearchBubbleView* bubble_view_ = nullptr;
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(TabSearchBubbleBrowserTest, ShowTriggersTimer) {
  EXPECT_NE(nullptr, bubble());

  // Timer should not be active when the bubble is not visible.
  EXPECT_FALSE(bubble()->IsVisible());
  EXPECT_FALSE(bubble_view()->timer_for_testing());

  // Showing the bubble should trigger the timer.
  bubble_view()->ShowUI();
  EXPECT_TRUE(bubble()->IsVisible());
  EXPECT_TRUE(bubble_view()->timer_for_testing());

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
