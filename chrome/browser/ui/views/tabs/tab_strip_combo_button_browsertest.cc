// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/tab_strip_combo_button.h"

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/tab_strip_region_view.h"
#include "chrome/browser/ui/views/tabs/tab_search_button.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"

class TabStripComboButtonBrowserTest : public InProcessBrowserTest {
 public:
  TabStripComboButtonBrowserTest() {
    feature_list_.InitWithFeaturesAndParameters(
        {{features::kTabstripComboButton,
          {{"tab_search_toolbar_button", "false"}}}},
        {});
  }

  BrowserView* browser_view() {
    return BrowserView::GetBrowserViewForBrowser(browser());
  }

  TabStripComboButton* tab_strip_combo_button() {
    return browser_view()->tab_strip_region_view()->tab_strip_combo_button();
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(TabStripComboButtonBrowserTest, BuildsComboButton) {
  EXPECT_EQ(
      nullptr,
      browser_view()->tab_strip_region_view()->new_tab_button_for_testing());
  EXPECT_EQ(nullptr, browser_view()
                         ->tab_strip_region_view()
                         ->tab_search_container_for_testing());

  EXPECT_NE(nullptr, tab_strip_combo_button());
  EXPECT_NE(nullptr, tab_strip_combo_button()->new_tab_button());
  EXPECT_NE(nullptr, tab_strip_combo_button()->tab_search_button());
}

IN_PROC_BROWSER_TEST_F(TabStripComboButtonBrowserTest, SeparatorVisibility) {
  EXPECT_TRUE(tab_strip_combo_button()->separator()->IsDrawn());

  tab_strip_combo_button()->new_tab_button()->SetState(
      views::Button::STATE_HOVERED);

  EXPECT_TRUE(tab_strip_combo_button()->separator()->IsDrawn());

  tab_strip_combo_button()->new_tab_button()->SetState(
      views::Button::STATE_NORMAL);

  EXPECT_TRUE(tab_strip_combo_button()->separator()->IsDrawn());

  tab_strip_combo_button()->tab_search_button()->SetState(
      views::Button::STATE_HOVERED);

  EXPECT_TRUE(tab_strip_combo_button()->separator()->IsDrawn());

  tab_strip_combo_button()->tab_search_button()->SetState(
      views::Button::STATE_NORMAL);

  EXPECT_TRUE(tab_strip_combo_button()->separator()->IsDrawn());
}
