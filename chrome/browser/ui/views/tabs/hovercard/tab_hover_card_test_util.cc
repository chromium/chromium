// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/hovercard/tab_hover_card_test_util.h"

#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/features.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/frame/base_tab_strip_region_view.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/tabs/common/tab_strip_collection_controller.h"
#include "chrome/browser/ui/views/tabs/common/tab_view.h"
#include "chrome/browser/ui/views/tabs/hovercard/tab_hover_card_bubble_view.h"
#include "chrome/browser/ui/views/tabs/hovercard/tab_hover_card_controller.h"
#include "chrome/browser/ui/views/tabs/tab.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "ui/views/test/widget_test.h"

namespace test {

TabHoverCardTestUtil::TabHoverCardTestUtil()
    : animation_mode_reset_(gfx::AnimationTestApi::SetRichAnimationRenderMode(
          gfx::Animation::RichAnimationRenderMode::FORCE_DISABLED)) {
  TabHoverCardController::set_disable_animations_for_testing(true);
}

TabHoverCardTestUtil::~TabHoverCardTestUtil() {
  TabHoverCardController::set_disable_animations_for_testing(false);
}

// static
TabStrip* TabHoverCardTestUtil::GetTabStrip(BrowserWindowInterface* browser) {
  return BrowserView::GetBrowserViewForBrowser(browser)
      ->horizontal_tab_strip_for_testing();
}

// static
TabHoverCardController* TabHoverCardTestUtil::GetHoverCardController(
    BrowserWindowInterface* browser) {
  auto* browser_view = BrowserView::GetBrowserViewForBrowser(browser);
  if (!browser_view) {
    return nullptr;
  }
  if (base::FeatureList::IsEnabled(tabs::kTabStripUnification)) {
    auto* base_region_view = views::AsViewClass<BaseTabStripRegionView>(
        browser_view->tab_strip_view());
    return base_region_view
               ? base_region_view->GetTabStripCollectionController()
                     ->GetHoverCardController()
               : nullptr;
  }
  auto* tab_strip = browser_view->horizontal_tab_strip_for_testing();
  return tab_strip ? tab_strip->hover_card_controller() : nullptr;
}

// static
TabHoverCardBubbleView* TabHoverCardTestUtil::GetHoverCard(
    BrowserWindowInterface* browser) {
  auto* controller = GetHoverCardController(browser);
  return controller ? controller->hover_card_for_testing() : nullptr;
}

// static
TabHoverCardBubbleView* TabHoverCardTestUtil::WaitForHoverCardVisible(
    BrowserWindowInterface* browser) {
  auto* const hover_card = GetHoverCard(browser);
  DCHECK(hover_card);
  views::test::WidgetVisibleWaiter(hover_card->GetWidget()).Wait();
  return hover_card;
}

// static
bool TabHoverCardTestUtil::IsHoverCardVisible(BrowserWindowInterface* browser) {
  auto* const hover_card = GetHoverCard(browser);
  return hover_card && hover_card->GetWidget() &&
         hover_card->GetWidget()->IsVisible();
}

// static
int TabHoverCardTestUtil::GetHoverCardsSeenCount(
    BrowserWindowInterface* browser) {
  auto* controller = GetHoverCardController(browser);
  return controller ? controller->hover_cards_seen_count_for_testing() : 0;
}

// static
TabHoverCardBubbleView* TabHoverCardTestUtil::SimulateHoverTab(
    BrowserWindowInterface* browser,
    int tab_index) {
  auto* browser_view = BrowserView::GetBrowserViewForBrowser(browser);
  if (base::FeatureList::IsEnabled(tabs::kTabStripUnification)) {
    auto* tab_view = views::AsViewClass<
        TabView>(browser_view->tab_strip_view()->GetTabAnchorView(
        browser->GetTabStripModel()->GetTabAtIndex(tab_index)->GetHandle()));
    if (tab_view) {
      if (auto* controller = GetHoverCardController(browser)) {
        controller->UpdateHoverCard(
            tab_view, TabSlotController::HoverCardUpdateType::kHover);
      }
    }
  } else {
    auto* const tab_strip = GetTabStrip(browser);
    if (tab_strip) {
      tab_strip->UpdateHoverCard(
          tab_strip->tab_at(tab_index),
          TabSlotController::HoverCardUpdateType::kHover);
    }
  }

  return WaitForHoverCardVisible(browser);
}

}  // namespace test
