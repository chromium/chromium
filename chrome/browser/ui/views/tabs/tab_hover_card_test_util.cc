// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/tab_hover_card_test_util.h"

#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/tabs/tab.h"
#include "chrome/browser/ui/views/tabs/tab_hover_card_bubble_view.h"
#include "chrome/browser/ui/views/tabs/tab_hover_card_controller.h"
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
TabStrip* TabHoverCardTestUtil::GetTabStrip(Browser* browser) {
  return BrowserView::GetBrowserViewForBrowser(browser)->tabstrip();
}

// static
TabHoverCardBubbleView* TabHoverCardTestUtil::GetHoverCard(
    TabStrip* tab_strip) {
  return tab_strip->hover_card_controller_for_testing()
      ->hover_card_for_testing();
}

// static
TabHoverCardBubbleView* TabHoverCardTestUtil::WaitForHoverCardVisible(
    TabStrip* tab_strip) {
  auto* const hover_card = GetHoverCard(tab_strip);
  DCHECK(hover_card);
  views::test::WidgetVisibleWaiter(hover_card->GetWidget()).Wait();
  return hover_card;
}

// static
bool TabHoverCardTestUtil::IsHoverCardVisible(TabStrip* tab_strip) {
  auto* const hover_card = GetHoverCard(tab_strip);
  return hover_card && hover_card->GetWidget() &&
         hover_card->GetWidget()->IsVisible();
}

// static
int TabHoverCardTestUtil::GetHoverCardsSeenCount(Browser* browser) {
  return GetTabStrip(browser)
      ->hover_card_controller_for_testing()
      ->hover_cards_seen_count_for_testing();
}

// static
TabHoverCardBubbleView* TabHoverCardTestUtil::SimulateHoverTab(Browser* browser,
                                                               int tab_index) {
  auto* const tab_strip = GetTabStrip(browser);

  // We don't use Tab::OnMouseEntered here to invoke the hover card because
  // that path is disabled in browser tests. If we enabled it, the real mouse
  // might interfere with the test.
  tab_strip->UpdateHoverCard(tab_strip->tab_at(tab_index),
                             TabSlotController::HoverCardUpdateType::kHover);

  return WaitForHoverCardVisible(tab_strip);
}

TabHoverCardTestUtil::HoverCardDestroyedWaiter::HoverCardDestroyedWaiter(
    TabStrip* tab_strip) {
  auto* const hover_card = GetHoverCard(tab_strip);
  if (hover_card && hover_card->GetWidget())
    observation_.Observe(hover_card->GetWidget());
}

TabHoverCardTestUtil::HoverCardDestroyedWaiter::~HoverCardDestroyedWaiter() =
    default;

void TabHoverCardTestUtil::HoverCardDestroyedWaiter::Wait() {
  if (!observation_.IsObserving())
    return;
  DCHECK(quit_closure_.is_null());
  quit_closure_ = run_loop_.QuitClosure();
  run_loop_.Run();
}

// views::WidgetObserver:
void TabHoverCardTestUtil::HoverCardDestroyedWaiter::OnWidgetDestroyed(
    views::Widget* widget) {
  observation_.Reset();
  if (!quit_closure_.is_null())
    std::move(quit_closure_).Run();
}

}  // namespace test
