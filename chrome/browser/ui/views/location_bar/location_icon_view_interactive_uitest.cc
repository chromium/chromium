// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/location_bar/location_icon_view.h"

#include "build/build_config.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/browser/ui/views/page_action/page_action_icon_view.h"
#include "chrome/browser/ui/views/page_info/page_info_bubble_view.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "content/public/test/test_utils.h"

namespace {

class LocationIconViewTest : public InProcessBrowserTest {
 public:
  LocationIconViewTest() = default;
  ~LocationIconViewTest() override = default;

 private:
  DISALLOW_COPY_AND_ASSIGN(LocationIconViewTest);
};

// Verify that clicking the location icon a second time hides the bubble.
IN_PROC_BROWSER_TEST_F(LocationIconViewTest, HideOnSecondClick) {
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser());
  views::View* location_icon_view =
      browser_view->toolbar()->location_bar()->location_icon_view();
  ASSERT_TRUE(location_icon_view);

  // Verify that clicking once shows the location icon bubble.
  scoped_refptr<content::MessageLoopRunner> runner1 =
      new content::MessageLoopRunner;
  ui_test_utils::MoveMouseToCenterAndPress(
      location_icon_view, ui_controls::LEFT,
      ui_controls::DOWN | ui_controls::UP, runner1->QuitClosure());
  runner1->Run();

  EXPECT_EQ(PageInfoBubbleView::BUBBLE_PAGE_INFO,
            PageInfoBubbleView::GetShownBubbleType());

  // Verify that clicking again doesn't reshow it.
  scoped_refptr<content::MessageLoopRunner> runner2 =
      new content::MessageLoopRunner;
  ui_test_utils::MoveMouseToCenterAndPress(
      location_icon_view, ui_controls::LEFT,
      ui_controls::DOWN | ui_controls::UP, runner2->QuitClosure());
  runner2->Run();

  EXPECT_EQ(PageInfoBubbleView::BUBBLE_NONE,
            PageInfoBubbleView::GetShownBubbleType());
}

#if defined(OS_MACOSX)
// TODO(jongkwon.lee): https://crbug.com/825834 NativeWidgetMac::Deactivate is
// not implemented on Mac.
#define MAYBE_ActivateFirstInactiveBubbleForAccessibility \
  DISABLED_ActivateFirstInactiveBubbleForAccessibility
#else
#define MAYBE_ActivateFirstInactiveBubbleForAccessibility \
  ActivateFirstInactiveBubbleForAccessibility
#endif
IN_PROC_BROWSER_TEST_F(LocationIconViewTest,
                       MAYBE_ActivateFirstInactiveBubbleForAccessibility) {
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser());
  LocationBarView* location_bar_view = browser_view->GetLocationBarView();
  EXPECT_FALSE(
      location_bar_view->ActivateFirstInactiveBubbleForAccessibility());

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  browser_view->ShowTranslateBubble(
      web_contents, translate::TRANSLATE_STEP_AFTER_TRANSLATE, "en", "fr",
      translate::TranslateErrors::NONE, true);

  PageActionIconView* icon_view =
      browser_view->toolbar_button_provider()
          ->GetPageActionIconView(PageActionIconType::kTranslate);
  ASSERT_TRUE(icon_view);
  EXPECT_TRUE(icon_view->GetVisible());

  // Ensure the bubble's widget is visible, but inactive. Active widgets are
  // focused by accessibility, so not of concern.
  views::Widget* widget = icon_view->GetBubble()->GetWidget();
  widget->Deactivate();
  widget->ShowInactive();
  EXPECT_TRUE(widget->IsVisible());
  EXPECT_FALSE(widget->IsActive());

  EXPECT_TRUE(location_bar_view->ActivateFirstInactiveBubbleForAccessibility());

  // Ensure the bubble's widget refreshed appropriately.
  EXPECT_TRUE(icon_view->GetVisible());
  EXPECT_TRUE(widget->IsVisible());
  EXPECT_TRUE(widget->IsActive());
}

}  // namespace
