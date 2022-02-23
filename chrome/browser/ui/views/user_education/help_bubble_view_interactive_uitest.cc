// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/user_education/help_bubble_view.h"

#include "chrome/browser/ui/view_ids.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/toolbar/browser_app_menu_button.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "content/public/test/browser_test.h"
#include "ui/events/base_event_utils.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/test/widget_test.h"

class HelpBubbleViewInteractiveTest : public InProcessBrowserTest {
 public:
  HelpBubbleViewInteractiveTest() = default;
  ~HelpBubbleViewInteractiveTest() override = default;

 protected:
  views::TrackedElementViews* GetAnchorElement() {
    return views::ElementTrackerViews::GetInstance()->GetElementForView(
        BrowserView::GetBrowserViewForBrowser(browser())
            ->toolbar()
            ->app_menu_button());
  }

  HelpBubbleParams GetBubbleParams() {
    HelpBubbleParams params;
    params.body_text = u"To X, do Y";
    params.arrow = HelpBubbleArrow::kTopRight;
    return params;
  }
};

IN_PROC_BROWSER_TEST_F(HelpBubbleViewInteractiveTest,
                       WidgetNotActivatedByDefault) {
  auto params = GetBubbleParams();

  auto* const browser_view = BrowserView::GetBrowserViewForBrowser(browser());
  auto* const focus_manager = browser_view->GetWidget()->GetFocusManager();
  EXPECT_TRUE(browser_view->GetWidget()->IsActive());

  browser_view->FocusToolbar();
  views::View* const initial_focused_view = focus_manager->GetFocusedView();
  EXPECT_NE(nullptr, initial_focused_view);

  auto* const bubble =
      new HelpBubbleView(GetAnchorElement()->view(), std::move(params));
  views::test::WidgetVisibleWaiter(bubble->GetWidget()).Wait();

  EXPECT_TRUE(browser_view->GetWidget()->IsActive());
  EXPECT_FALSE(bubble->GetWidget()->IsActive());
  bubble->Close();
}
