// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_HOVERCARD_TAB_HOVER_CARD_TEST_UTIL_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_HOVERCARD_TAB_HOVER_CARD_TEST_UTIL_H_

#include "base/run_loop.h"
#include "chrome/browser/ui/views/tabs/hovercard/tab_hover_card_bubble_view.h"
#include "chrome/browser/ui/views/tabs/hovercard/tab_hover_card_controller.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "ui/gfx/animation/animation_test_api.h"
#include "ui/views/widget/widget.h"

class BrowserWindowInterface;

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

  static TabStrip* GetTabStrip(BrowserWindowInterface* browser);
  static TabHoverCardController* GetHoverCardController(
      BrowserWindowInterface* browser);
  static TabHoverCardBubbleView* GetHoverCard(BrowserWindowInterface* browser);
  static TabHoverCardBubbleView* WaitForHoverCardVisible(
      BrowserWindowInterface* browser);
  static bool IsHoverCardVisible(BrowserWindowInterface* browser);
  static int GetHoverCardsSeenCount(BrowserWindowInterface* browser);
  static TabHoverCardBubbleView* SimulateHoverTab(
      BrowserWindowInterface* browser,
      int tab_index);

 private:
  gfx::AnimationTestApi::RenderModeResetter animation_mode_reset_;
};

}  // namespace test

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_HOVERCARD_TAB_HOVER_CARD_TEST_UTIL_H_
