// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/session_crashed_bubble_view.h"

#include <string>

#include "build/build_config.h"
#include "build/buildflag.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/toolbar/browser_app_menu_button.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"
#include "ui/base/buildflags.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/test/ax_event_counter.h"
#include "ui/views/view.h"

class SessionCrashedBubbleViewTest : public DialogBrowserTest {
 public:
  SessionCrashedBubbleViewTest() {}
  ~SessionCrashedBubbleViewTest() override {}

  void ShowUi(const std::string& name) override {
    // TODO(pbos): Set up UMA opt-in conditions instead of providing this bool.
    crash_bubble_ = SessionCrashedBubbleView::ShowBubble(
        browser(), false, name == "SessionCrashedBubbleOfferUma");
  }

 protected:
  views::BubbleDialogDelegateView* crash_bubble_;

 private:
  DISALLOW_COPY_AND_ASSIGN(SessionCrashedBubbleViewTest);
};

IN_PROC_BROWSER_TEST_F(SessionCrashedBubbleViewTest,
                       InvokeUi_SessionCrashedBubble) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(SessionCrashedBubbleViewTest,
                       InvokeUi_SessionCrashedBubbleOfferUma) {
  ShowAndVerifyUi();
}

// Regression test for https://crbug.com/1042010, it should be possible to focus
// the bubble with the "focus dialog" hotkey combination (Alt+Shift+A).
IN_PROC_BROWSER_TEST_F(SessionCrashedBubbleViewTest,
                       CanFocusBubbleWithFocusDialogHotkey) {
  ShowUi("SessionCrashedBubble");

  views::FocusManager* focus_manager = crash_bubble_->GetFocusManager();
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser());
  views::View* bubble_focused_view = crash_bubble_->GetInitiallyFocusedView();

  focus_manager->ClearFocus();
  EXPECT_FALSE(bubble_focused_view->HasFocus());

  browser_view->FocusInactivePopupForAccessibility();
  EXPECT_TRUE(bubble_focused_view->HasFocus());
}

// Regression test for https://crbug.com/1042010, it should be possible to focus
// the bubble with the "rotate pane focus" (F6) hotkey.
IN_PROC_BROWSER_TEST_F(SessionCrashedBubbleViewTest,
                       CanFocusBubbleWithRotatePaneFocusHotkey) {
  ShowUi("SessionCrashedBubble");
  views::FocusManager* focus_manager = crash_bubble_->GetFocusManager();
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser());
  views::View* bubble_focused_view = crash_bubble_->GetInitiallyFocusedView();

  focus_manager->ClearFocus();
  EXPECT_FALSE(bubble_focused_view->HasFocus());

  browser_view->RotatePaneFocus(true);
  // Rotate pane focus is expected to keep the bubble focused until the user
  // deals with it, so a second call should have no effect.
  browser_view->RotatePaneFocus(true);
  EXPECT_TRUE(bubble_focused_view->HasFocus());
}

IN_PROC_BROWSER_TEST_F(SessionCrashedBubbleViewTest, AlertAccessibleEvent) {
  views::test::AXEventCounter counter(views::AXEventManager::Get());
  EXPECT_EQ(0, counter.GetCount(ax::mojom::Event::kAlert));
  ShowUi("SessionCrashedBubble");
  EXPECT_EQ(1, counter.GetCount(ax::mojom::Event::kAlert));
}

// Regression test for https://crbug.com/1081393.
IN_PROC_BROWSER_TEST_F(SessionCrashedBubbleViewTest, HasCloseButton) {
  ShowUi("SessionCrashedBubble");
  EXPECT_TRUE(crash_bubble_->ShouldShowCloseButton());
}
