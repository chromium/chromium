// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/page_info/page_info_bubble_controller.h"

#include "base/command_line.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_manager.h"
#include "chrome/browser/ui/exclusive_access/fullscreen_controller.h"
#include "chrome/browser/ui/page_info/page_info_dialog.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/page_info/page_info_bubble_view.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "ui/base/page_transition_types.h"
#import "ui/base/test/scoped_fake_nswindow_fullscreen.h"
#include "ui/base/ui_base_features.h"
#include "ui/base/ui_base_switches.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/widget/widget.h"
#include "url/url_constants.h"

@interface PageInfoBubbleController (ExposedForTesting)
+ (PageInfoBubbleController*)getPageInfoBubbleForTest;
- (void)performLayout;
@end

namespace {

struct GURLBubbleTypePair {
  const char* const url;
  const PageInfoBubbleView::BubbleType bubble_type;
};

constexpr GURLBubbleTypePair kGurlBubbleTypePairs[] = {
    {url::kAboutBlankURL, PageInfoBubbleView::BUBBLE_PAGE_INFO},
    {"chrome://settings", PageInfoBubbleView::BUBBLE_INTERNAL_PAGE},
};

class PageInfoBubbleViewsMacTest
    : public InProcessBrowserTest,
      public testing::WithParamInterface<GURLBubbleTypePair> {
 protected:
  PageInfoBubbleViewsMacTest() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(PageInfoBubbleViewsMacTest);
};

}  // namespace

// Test the Page Info bubble doesn't crash upon entering full screen mode while
// it is open. This may occur when the bubble is trying to reanchor itself.
IN_PROC_BROWSER_TEST_P(PageInfoBubbleViewsMacTest, NoCrashOnFullScreenToggle) {
  ui::test::ScopedFakeNSWindowFullscreen fake_fullscreen;
  ui_test_utils::NavigateToURL(browser(), GURL(GetParam().url));
  ShowPageInfoDialog(browser()->tab_strip_model()->GetWebContentsAt(0),
                     base::BindOnce([](views::Widget::CloseReason, bool) {}));
  ExclusiveAccessManager* access_manager =
      browser()->exclusive_access_manager();
  FullscreenController* fullscreen_controller =
      access_manager->fullscreen_controller();

  fullscreen_controller->ToggleBrowserFullscreenMode();
  views::BubbleDialogDelegateView* page_info =
      PageInfoBubbleView::GetPageInfoBubbleForTesting();
  EXPECT_TRUE(page_info);
  views::Widget* page_info_bubble = page_info->GetWidget();
  EXPECT_TRUE(page_info_bubble);
  EXPECT_EQ(GetParam().bubble_type, PageInfoBubbleView::GetShownBubbleType());
  EXPECT_TRUE(page_info_bubble->IsVisible());

  // There should be no crash here from re-anchoring the Page Info bubble while
  // transitioning into full screen.
  fake_fullscreen.FinishTransition();
}

// Test |PageInfoBubbleView| or |InternalPageInfoBubbleView| is hidden on tab
// switches via keyboard shortcuts.
IN_PROC_BROWSER_TEST_P(PageInfoBubbleViewsMacTest,
                       BubbleClosesOnKeyboardTabSwitch) {
  ui_test_utils::NavigateToURL(browser(), GURL(GetParam().url));
  // Add a second tab, but make sure the first is selected.
  AddTabAtIndex(1, GURL("https://test_url.com"),
                ui::PageTransition::PAGE_TRANSITION_LINK);
  browser()->tab_strip_model()->ActivateTabAt(
      0, {TabStripModel::GestureType::kOther});
  EXPECT_EQ(0, browser()->tab_strip_model()->active_index());

  // Show the (internal or external) Page Info bubble and check it's visible.
  ShowPageInfoDialog(browser()->tab_strip_model()->GetWebContentsAt(0),
                     base::BindOnce([](views::Widget::CloseReason, bool) {}));
  EXPECT_EQ(GetParam().bubble_type, PageInfoBubbleView::GetShownBubbleType());
  views::BubbleDialogDelegateView* page_info =
      PageInfoBubbleView::GetPageInfoBubbleForTesting();
  EXPECT_TRUE(page_info);
  EXPECT_TRUE(page_info->GetWidget()->IsVisible());

  // Switch to the second tab without clicking (which would normally dismiss the
  // bubble).
  chrome::SelectNextTab(browser());
  EXPECT_EQ(1, browser()->tab_strip_model()->active_index());

  page_info = PageInfoBubbleView::GetPageInfoBubbleForTesting();
  // Check the bubble is no longer visible. BubbleDialogDelegateView's Widget is
  // destroyed when the native widget is destroyed, so it should still be alive.
  EXPECT_TRUE(page_info);
  EXPECT_TRUE(page_info->GetWidget());
  EXPECT_FALSE(page_info->GetWidget()->IsVisible());

  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(PageInfoBubbleView::GetPageInfoBubbleForTesting());
}

INSTANTIATE_TEST_SUITE_P(,
                         PageInfoBubbleViewsMacTest,
                         testing::ValuesIn(kGurlBubbleTypePairs));
