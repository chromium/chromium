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
#include "content/public/test/browser_test.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/widget/widget.h"

using views::Widget;

class TabHoverCardBubbleViewInteractiveUiTest : public InProcessBrowserTest {
 public:
  TabHoverCardBubbleViewInteractiveUiTest() {
    TabHoverCardBubbleView::disable_animations_for_testing_ = true;
    scoped_feature_list_.InitAndEnableFeature(features::kTabHoverCards);
  }
  TabHoverCardBubbleViewInteractiveUiTest(
      const TabHoverCardBubbleViewInteractiveUiTest&) = delete;
  TabHoverCardBubbleViewInteractiveUiTest& operator=(
      const TabHoverCardBubbleViewInteractiveUiTest&) = delete;
  ~TabHoverCardBubbleViewInteractiveUiTest() override = default;

  static TabHoverCardBubbleView* GetHoverCard(const TabStrip* tabstrip) {
    return tabstrip->hover_card_;
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

#if defined(USE_AURA)
// Verify that the hover card is not visible when any key is pressed.
IN_PROC_BROWSER_TEST_F(TabHoverCardBubbleViewInteractiveUiTest,
                       HoverCardHidesOnAnyKeyPressInSameWindow) {
  TabStrip* tab_strip =
      BrowserView::GetBrowserViewForBrowser(browser())->tabstrip();
  Tab* tab = tab_strip->tab_at(0);
  tab_strip->UpdateHoverCard(tab);
  TabHoverCardBubbleView* hover_card = GetHoverCard(tab_strip);
  Widget* widget = hover_card->GetWidget();
  EXPECT_NE(nullptr, widget);
  views::test::WidgetVisibleWaiter(widget).Wait();
  EXPECT_TRUE(widget->IsVisible());

  EXPECT_TRUE(ui_test_utils::SendKeyPressSync(browser(), ui::VKEY_DOWN, false,
                                              false, false, false));
  // Note, fade in/out animations are disabled for testing so there is no need
  // to account for them here.
  EXPECT_FALSE(widget->IsVisible());
}
#endif
