// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/page_action/page_action_icon_type.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/location_bar/icon_label_bubble_view.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/browser/ui/views/location_bar/location_icon_view.h"
#include "chrome/browser/ui/views/page_action/page_action_icon_view.h"
#include "chrome/browser/ui/views/page_action/page_action_view.h"
#include "chrome/browser/ui/views/page_info/page_info_bubble_view.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/browser/ui/views/translate/translate_bubble_controller.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_utils.h"

namespace {

class LocationIconViewTest : public InProcessBrowserTest {
 public:
  LocationIconViewTest() = default;

  LocationIconViewTest(const LocationIconViewTest&) = delete;
  LocationIconViewTest& operator=(const LocationIconViewTest&) = delete;

  ~LocationIconViewTest() override = default;
};

// Verify that clicking the location icon a second time hides the bubble.
// TODO(crbug.com/40251927) flaky on mac11-arm64-rel, disabled via filter
// TODO(crbug.com/41481796) Fails consistently on Linux, disabled via filter.
IN_PROC_BROWSER_TEST_F(LocationIconViewTest, HideOnSecondClick) {
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser());
  views::View* location_icon_view =
      browser_view->toolbar()->location_bar()->location_icon_view();
  ASSERT_TRUE(location_icon_view);

  // Verify that clicking once shows the location icon bubble.
  scoped_refptr<content::MessageLoopRunner> runner1 =
      new content::MessageLoopRunner;
  ui_test_utils::MoveMouseToCenterAndClick(
      location_icon_view, ui_controls::LEFT,
      ui_controls::DOWN | ui_controls::UP, runner1->QuitClosure());
  runner1->Run();

  EXPECT_EQ(PageInfoBubbleView::BUBBLE_PAGE_INFO,
            PageInfoBubbleView::GetShownBubbleType());

  // Verify that clicking again doesn't reshow it.
  scoped_refptr<content::MessageLoopRunner> runner2 =
      new content::MessageLoopRunner;
  ui_test_utils::MoveMouseToCenterAndClick(
      location_icon_view, ui_controls::LEFT,
      ui_controls::DOWN | ui_controls::UP, runner2->QuitClosure());
  runner2->Run();

  EXPECT_EQ(PageInfoBubbleView::BUBBLE_NONE,
            PageInfoBubbleView::GetShownBubbleType());
}
}  // namespace
