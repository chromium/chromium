// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/views/tabs/tab.h"
#include "chrome/browser/ui/views/tabs/tab_hover_card_bubble_view.h"
#include "chrome/browser/ui/views/tabs/tab_hover_card_controller.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "content/public/test/browser_test.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/widget/widget.h"

using views::Widget;

class TabHoverCardBubbleViewInteractiveUiTest : public InProcessBrowserTest {
 public:
  TabHoverCardBubbleViewInteractiveUiTest() {
    TabHoverCardController::disable_animations_for_testing_ = true;
  }
  TabHoverCardBubbleViewInteractiveUiTest(
      const TabHoverCardBubbleViewInteractiveUiTest&) = delete;
  TabHoverCardBubbleViewInteractiveUiTest& operator=(
      const TabHoverCardBubbleViewInteractiveUiTest&) = delete;
  ~TabHoverCardBubbleViewInteractiveUiTest() override = default;

  static TabHoverCardBubbleView* GetHoverCard(const TabStrip* tabstrip) {
    return tabstrip->hover_card_controller_->hover_card_;
  }
};

#if defined(USE_AURA)

namespace {

// Similar to views::test::WidgetDestroyedWaiter but waiting after the widget
// has been closed is a no-op rather than an error.
class SafeWidgetDestroyedWaiter : public views::WidgetObserver {
 public:
  explicit SafeWidgetDestroyedWaiter(views::Widget* widget) {
    observation_.Observe(widget);
  }

  // views::WidgetObserver:
  void OnWidgetDestroyed(Widget* widget) override {
    observation_.Reset();
    if (!quit_closure_.is_null())
      std::move(quit_closure_).Run();
  }

  void Wait() {
    if (!observation_.IsObserving())
      return;
    DCHECK(quit_closure_.is_null());
    quit_closure_ = run_loop_.QuitClosure();
    run_loop_.Run();
  }

 private:
  base::RunLoop run_loop_;
  base::OnceClosure quit_closure_;
  base::ScopedObservation<views::Widget, views::WidgetObserver> observation_{
      this};
};

}  // namespace

// Verify that the hover card is not visible when any key is pressed.
IN_PROC_BROWSER_TEST_F(TabHoverCardBubbleViewInteractiveUiTest,
                       HoverCardHidesOnAnyKeyPressInSameWindow) {
  TabStrip* tab_strip =
      BrowserView::GetBrowserViewForBrowser(browser())->tabstrip();
  Tab* tab = tab_strip->tab_at(0);
  tab_strip->UpdateHoverCard(tab, TabController::HoverCardUpdateType::kHover);
  TabHoverCardBubbleView* hover_card = GetHoverCard(tab_strip);
  Widget* widget = hover_card->GetWidget();
  EXPECT_NE(nullptr, widget);
  views::test::WidgetVisibleWaiter(widget).Wait();
  EXPECT_TRUE(widget->IsVisible());

  // Verify that the hover card widget is destroyed sometime between now and
  // when we check afterwards. Depending on platform, the destruction could be
  // synchronous or asynchronous.
  SafeWidgetDestroyedWaiter widget_destroyed_waiter(widget);

  EXPECT_TRUE(ui_test_utils::SendKeyPressSync(browser(), ui::VKEY_DOWN, false,
                                              false, false, false));

  // Note, fade in/out animations are disabled for testing so this should be
  // relatively quick.
  widget_destroyed_waiter.Wait();
  EXPECT_EQ(nullptr, GetHoverCard(tab_strip));
}
#endif
