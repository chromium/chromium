// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_TAB_HOVER_CARD_TEST_UTIL_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_TAB_HOVER_CARD_TEST_UTIL_H_

#include "base/run_loop.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/views/tabs/tab_hover_card_bubble_view.h"
#include "chrome/browser/ui/views/tabs/tab_hover_card_controller.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "ui/gfx/animation/animation_test_api.h"
#include "ui/views/widget/widget.h"

namespace test {

// Class that disables hover card animations and provides a lot of convenience
// methods that can be used across various tab hover card tests. Your test
// fixture should inherit this class.
class TabHoverCardTestUtil {
 public:
  TabHoverCardTestUtil();
  virtual ~TabHoverCardTestUtil();
  TabHoverCardTestUtil(const TabHoverCardTestUtil&) = delete;
  void operator=(const TabHoverCardTestUtil&) = delete;

  static TabStrip* GetTabStrip(Browser* browser);
  static TabHoverCardBubbleView* GetHoverCard(TabStrip* tab_strip);
  static TabHoverCardBubbleView* WaitForHoverCardVisible(TabStrip* tab_strip);
  static bool IsHoverCardVisible(TabStrip* tab_strip);
  static int GetHoverCardsSeenCount(Browser* browser);
  static TabHoverCardBubbleView* SimulateHoverTab(Browser* browser,
                                                  int tab_index);

  // Similar to views::test::WidgetDestroyedWaiter but for the hover card for
  // TabStrip. Waiting after the hover card's widget is destroyed (or if there
  // is no hover card) is a no-op rather than an error.
  class HoverCardDestroyedWaiter : public views::WidgetObserver {
   public:
    explicit HoverCardDestroyedWaiter(TabStrip* tab_strip);
    ~HoverCardDestroyedWaiter() override;
    HoverCardDestroyedWaiter(const HoverCardDestroyedWaiter&) = delete;
    void operator=(const HoverCardDestroyedWaiter&) = delete;

    // Waits for the hover card and its widget to be destroyed; is a no-op if
    void Wait();

   private:
    // views::WidgetObserver:
    void OnWidgetDestroyed(views::Widget* widget) override;

    base::RunLoop run_loop_{base::RunLoop::Type::kNestableTasksAllowed};
    base::OnceClosure quit_closure_;
    base::ScopedObservation<views::Widget, views::WidgetObserver> observation_{
        this};
  };

 private:
  gfx::AnimationTestApi::RenderModeResetter animation_mode_reset_;
};

}  // namespace test

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_TAB_HOVER_CARD_TEST_UTIL_H_
