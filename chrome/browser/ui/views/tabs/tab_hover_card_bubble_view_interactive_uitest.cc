// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/tabs/tab.h"
#include "chrome/browser/ui/views/tabs/tab_hover_card_bubble_view.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_observer.h"

using views::Widget;

// Helper to wait until the hover card widget is visible.
class HoverCardVisibleWaiter : public views::WidgetObserver {
 public:
  explicit HoverCardVisibleWaiter(Widget* hover_card)
      : hover_card_(hover_card) {
    hover_card_->AddObserver(this);
  }
  ~HoverCardVisibleWaiter() override { hover_card_->RemoveObserver(this); }

  void Wait() {
    if (hover_card_->IsVisible())
      return;
    run_loop_.Run();
  }

  // WidgetObserver overrides:
  void OnWidgetVisibilityChanged(Widget* widget, bool visible) override {
    if (visible)
      run_loop_.Quit();
  }

 private:
  Widget* const hover_card_;
  base::RunLoop run_loop_;
};

class TabHoverCardBubbleViewInteractiveUiTest : public InProcessBrowserTest {
 public:
  TabHoverCardBubbleViewInteractiveUiTest() {
    TabHoverCardBubbleView::disable_animations_for_testing_ = true;
    scoped_feature_list_.InitAndEnableFeature(features::kTabHoverCards);
  }

  ~TabHoverCardBubbleViewInteractiveUiTest() override = default;

  static TabHoverCardBubbleView* GetHoverCard(const TabStrip* tabstrip) {
    return tabstrip->hover_card_;
  }

  static Widget* GetHoverCardWidget(const TabHoverCardBubbleView* hover_card) {
    return hover_card->widget_;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(TabHoverCardBubbleViewInteractiveUiTest);

  base::test::ScopedFeatureList scoped_feature_list_;
};

#if defined(USE_AURA)
// Verify that the hover card is not visible when any key is pressed.
// TODO(crbug.com/947668): The test is flaky on Win10.
#if defined(OS_WIN)
#define MAYBE_HoverCardHidesOnAnyKeyPressInSameWindow DISABLED_HoverCardHidesOnAnyKeyPressInSameWindow
#else
#define MAYBE_HoverCardHidesOnAnyKeyPressInSameWindow HoverCardHidesOnAnyKeyPressInSameWindow
#endif
IN_PROC_BROWSER_TEST_F(TabHoverCardBubbleViewInteractiveUiTest,
                       MAYBE_HoverCardHidesOnAnyKeyPressInSameWindow) {
  TabStrip* tab_strip =
      BrowserView::GetBrowserViewForBrowser(browser())->tabstrip();
  Tab* tab = tab_strip->tab_at(0);
  ui::MouseEvent hover_event(ui::ET_MOUSE_ENTERED, gfx::Point(), gfx::Point(),
                             base::TimeTicks(), ui::EF_NONE, 0);
  tab->OnMouseEntered(hover_event);
  TabHoverCardBubbleView* hover_card = GetHoverCard(tab_strip);
  Widget* widget = GetHoverCardWidget(hover_card);
  HoverCardVisibleWaiter waiter(widget);
  waiter.Wait();

  EXPECT_TRUE(widget != nullptr);
  EXPECT_TRUE(widget->IsVisible());

  EXPECT_TRUE(ui_test_utils::SendKeyPressSync(browser(), ui::VKEY_DOWN, false,
                                              false, false, false));
  // Note, fade in/out animations are disabled for testing so there is no need
  // to account for them here.
  EXPECT_FALSE(widget->IsVisible());
}
#endif
