// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/tab_scrubber.h"

#include <stdint.h>

#include <algorithm>

#include "ash/shell.h"
#include "base/bind.h"
#include "base/metrics/histogram_macros.h"
#include "base/numerics/ranges.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/immersive_mode_controller.h"
#include "chrome/browser/ui/views/tabs/tab.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "chrome/browser/ui/views/tabs/tab_style_views.h"
#include "ui/events/event.h"
#include "ui/events/event_utils.h"
#include "ui/events/gesture_detection/gesture_configuration.h"

// static
TabScrubber* TabScrubber::GetInstance() {
  static TabScrubber* instance = nullptr;
  if (!instance)
    instance = new TabScrubber();
  return instance;
}

// static
gfx::Point TabScrubber::GetStartPoint(TabStrip* tab_strip,
                                      int index,
                                      TabScrubber::Direction direction) {
  const Tab* tab = tab_strip->tab_at(index);
  gfx::Rect tab_bounds = tab->GetMirroredBounds();

  // Start the swipe where the tab contents start/end.  This provides a small
  // amount of slop inside the tab before a swipe will change tabs.
  auto contents_insets = tab->tab_style()->GetContentsInsets();
  int left = contents_insets.left();
  int right = contents_insets.right();

  // The contents insets are logical rather than physical, so reverse them for
  // RTL.
  if (base::i18n::IsRTL())
    std::swap(left, right);

  // For very narrow tabs, the contents insets may be too large.  Clamp to the
  // opposite edges of the tab, which should be at (overlap / 2).
  gfx::Rect tab_edges = tab_bounds;
  // For odd overlap values, be conservative and inset both edges rounding up.
  tab_edges.Inset((TabStyle::GetTabOverlap() + 1) / 2, 0);
  const int x = (direction == LEFT)
                    ? std::min(tab_bounds.x() + left, tab_edges.right())
                    : std::max(tab_bounds.right() - right, tab_edges.x());

  return gfx::Point(x, tab_bounds.CenterPoint().y());
}

bool TabScrubber::IsActivationPending() {
  return activate_timer_.IsRunning();
}

TabScrubber::TabScrubber() {
  // TODO(mash): Add window server API to observe swipe gestures. Observing
  // gestures on browser windows is not sufficient, as this feature works when
  // the cursor is over the shelf, desktop, etc. https://crbug.com/796366
  ash::Shell::Get()->AddPreTargetHandler(this);
  BrowserList::AddObserver(this);
}

TabScrubber::~TabScrubber() {
  BrowserList::RemoveObserver(this);
}

void TabScrubber::OnScrollEvent(ui::ScrollEvent* event) {
  if (event->type() == ui::ET_SCROLL_FLING_CANCEL ||
      event->type() == ui::ET_SCROLL_FLING_START) {
    FinishScrub(true);
    immersive_reveal_lock_.reset();
    return;
  }

  if (event->finger_count() != 3)
    return;

  Browser* browser = GetActiveBrowser();
  if (!browser || (scrubbing_ && browser_ && browser != browser_) ||
      (highlighted_tab_ != -1 &&
       highlighted_tab_ >= browser->tab_strip_model()->count())) {
    FinishScrub(false);
    return;
  }

  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser);
  TabStrip* tab_strip = browser_view->tabstrip();

  if (tab_strip->IsAnimating()) {
    FinishScrub(false);
    return;
  }

  // We are handling the event.
  event->StopPropagation();

  // The event's x_offset doesn't change in an RTL layout. Negative value means
  // left, positive means right.
  float x_offset = event->x_offset();
  if (!scrubbing_) {
    BeginScrub(browser_view, x_offset);
  } else if (highlighted_tab_ == -1) {
    // Has the direction of the swipe changed while scrubbing?
    Direction direction = (x_offset < 0) ? LEFT : RIGHT;
    if (direction != swipe_direction_)
      ScrubDirectionChanged(direction);
  }

  UpdateSwipeX(x_offset);

  Tab* new_tab = tab_strip_->GetTabAt(gfx::Point(swipe_x_, swipe_y_));
  if (!new_tab)
    return;

  int new_index = tab_strip_->GetModelIndexOf(new_tab);
  if (highlighted_tab_ == -1 &&
      new_index == browser_->tab_strip_model()->active_index()) {
    return;
  }

  if (new_index != highlighted_tab_) {
    if (activate_timer_.IsRunning())
      activate_timer_.Reset();
    else
      ScheduleFinishScrubIfNeeded();
  }

  UpdateHighlightedTab(new_tab, new_index);

  if (highlighted_tab_ != -1) {
    gfx::Point hover_point(swipe_x_, swipe_y_);
    views::View::ConvertPointToTarget(tab_strip_, new_tab, &hover_point);
    new_tab->tab_style()->SetHoverLocation(hover_point);
  }
}

void TabScrubber::OnBrowserRemoved(Browser* browser) {
  if (browser != browser_)
    return;

  activate_timer_.Stop();
  swipe_x_ = -1;
  swipe_y_ = -1;
  scrubbing_ = false;
  highlighted_tab_ = -1;
  browser_ = nullptr;
  tab_strip_ = nullptr;
}

void TabScrubber::OnTabAdded(int index) {
  if (highlighted_tab_ == -1)
    return;

  if (index < highlighted_tab_)
    ++highlighted_tab_;
}

void TabScrubber::OnTabMoved(int from_index, int to_index) {
  if (highlighted_tab_ == -1)
    return;

  if (from_index == highlighted_tab_)
    highlighted_tab_ = to_index;
  else if (from_index < highlighted_tab_ && highlighted_tab_ <= to_index)
    --highlighted_tab_;
  else if (from_index > highlighted_tab_ && highlighted_tab_ >= to_index)
    ++highlighted_tab_;
}

void TabScrubber::OnTabRemoved(int index) {
  if (highlighted_tab_ == -1)
    return;
  if (index == highlighted_tab_) {
    FinishScrub(false);
    return;
  }
  if (index < highlighted_tab_)
    --highlighted_tab_;
}

Browser* TabScrubber::GetActiveBrowser() {
  Browser* browser = chrome::FindLastActive();
  if (!browser || !browser->is_type_normal() ||
      !browser->window()->IsActive()) {
    return nullptr;
  }

  return browser;
}

void TabScrubber::BeginScrub(BrowserView* browser_view, float x_offset) {
  DCHECK(browser_view);
  DCHECK(browser_view->browser());

  scrubbing_start_time_ = base::TimeTicks::Now();
  tab_strip_ = browser_view->tabstrip();
  scrubbing_ = true;
  browser_ = browser_view->browser();

  Direction direction = (x_offset < 0) ? LEFT : RIGHT;
  ScrubDirectionChanged(direction);

  ImmersiveModeController* immersive_controller =
      browser_view->immersive_mode_controller();
  if (immersive_controller->IsEnabled()) {
    immersive_reveal_lock_.reset(immersive_controller->GetRevealedLock(
        ImmersiveModeController::ANIMATE_REVEAL_YES));
  }

  tab_strip_->AddObserver(this);
}

void TabScrubber::FinishScrub(bool activate) {
  activate_timer_.Stop();

  if (browser_ && browser_->window()) {
    BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser_);
    TabStrip* tab_strip = browser_view->tabstrip();
    if (activate && highlighted_tab_ != -1) {
      Tab* tab = tab_strip->tab_at(highlighted_tab_);
      tab->tab_style()->HideHover(TabStyle::HideHoverStyle::kImmediate);
      int distance = std::abs(highlighted_tab_ -
                              browser_->tab_strip_model()->active_index());
      UMA_HISTOGRAM_CUSTOM_COUNTS("Tabs.ScrubDistance", distance, 1, 20, 21);
      UMA_HISTOGRAM_TIMES("Tabs.ScrubDuration",
                          base::TimeTicks::Now() - scrubbing_start_time_);
      browser_->tab_strip_model()->ActivateTabAt(
          highlighted_tab_, {TabStripModel::GestureType::kOther});
    }
    tab_strip->RemoveObserver(this);
  }

  browser_ = nullptr;
  tab_strip_ = nullptr;
  swipe_x_ = -1;
  swipe_y_ = -1;
  scrubbing_ = false;
  highlighted_tab_ = -1;
}

void TabScrubber::ScheduleFinishScrubIfNeeded() {
  // Tests use a really long delay to ensure RunLoops don't unnecessarily
  // trigger the timer running.
  const base::TimeDelta delay = base::TimeDelta::FromMilliseconds(
      use_default_activation_delay_ ? 200 : 20000);
  activate_timer_.Start(FROM_HERE, delay,
                        base::BindRepeating(&TabScrubber::FinishScrub,
                                            base::Unretained(this), true));
}

void TabScrubber::ScrubDirectionChanged(Direction direction) {
  DCHECK(browser_);
  DCHECK(tab_strip_);
  DCHECK(scrubbing_);

  swipe_direction_ = direction;
  const gfx::Point start_point =
      GetStartPoint(tab_strip_, browser_->tab_strip_model()->active_index(),
                    swipe_direction_);
  swipe_x_ = start_point.x();
  swipe_y_ = start_point.y();
}

void TabScrubber::UpdateSwipeX(float x_offset) {
  DCHECK(browser_);
  DCHECK(tab_strip_);
  DCHECK(scrubbing_);

  // Make the swipe speed inversely proportional with the number or tabs:
  // Each added tab introduces a reduction of 2% in |x_offset|, with a value of
  // one fourth of |x_offset| as the minimum (i.e. we need 38 tabs to reach
  // that minimum reduction).
  swipe_x_ += base::ClampToRange(
      x_offset - (tab_strip_->tab_count() * 0.02f * x_offset), 0.25f * x_offset,
      x_offset);

  // In an RTL layout, everything is mirrored, i.e. the index of the first tab
  // (with the smallest X mirrored co-ordinates) is actually the index of the
  // last tab. Same for the index of the last tab.
  int first_tab_index = base::i18n::IsRTL() ? tab_strip_->tab_count() - 1 : 0;
  int last_tab_index = base::i18n::IsRTL() ? 0 : tab_strip_->tab_count() - 1;

  Tab* first_tab = tab_strip_->tab_at(first_tab_index);
  int first_tab_center = first_tab->GetMirroredBounds().CenterPoint().x();
  Tab* last_tab = tab_strip_->tab_at(last_tab_index);
  int last_tab_center = last_tab->GetMirroredBounds().CenterPoint().x();

  if (swipe_x_ < first_tab_center)
    swipe_x_ = first_tab_center;
  if (swipe_x_ > last_tab_center)
    swipe_x_ = last_tab_center;
}

void TabScrubber::UpdateHighlightedTab(Tab* new_tab, int new_index) {
  DCHECK(scrubbing_);
  DCHECK(new_tab);

  if (new_index == highlighted_tab_)
    return;

  if (highlighted_tab_ != -1) {
    Tab* tab = tab_strip_->tab_at(highlighted_tab_);
    tab->tab_style()->HideHover(TabStyle::HideHoverStyle::kImmediate);
  }

  if (new_index != browser_->tab_strip_model()->active_index()) {
    highlighted_tab_ = new_index;
    new_tab->tab_style()->ShowHover(TabStyle::ShowHoverStyle::kPronounced);
  } else {
    highlighted_tab_ = -1;
  }
}
