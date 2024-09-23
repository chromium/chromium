// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/tab_scrubber_chromeos.h"

#include <stdint.h>

#include <algorithm>

#include "base/functional/bind.h"
#include "base/metrics/histogram_macros.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/immersive_mode_controller.h"
#include "chrome/browser/ui/views/tabs/tab.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "chrome/browser/ui/views/tabs/tab_style_views.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/event.h"
#include "ui/events/event_utils.h"
#include "ui/events/gesture_detection/gesture_configuration.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/public/activation_client.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/shell.h"
#include "chrome/browser/ash/crosapi/browser_manager.h"
#include "chrome/browser/ash/crosapi/browser_util.h"
#endif

// static
TabScrubberChromeOS* TabScrubberChromeOS::GetInstance() {
  static TabScrubberChromeOS* instance = nullptr;
  if (!instance)
    instance = new TabScrubberChromeOS();
  return instance;
}

// static
gfx::Point TabScrubberChromeOS::GetStartPoint(
    TabStrip* tab_strip,
    int index,
    TabScrubberChromeOS::Direction direction) {
  const Tab* tab = tab_strip->tab_at(index);
  gfx::Rect tab_bounds = tab->GetMirroredBounds();

  // Start the swipe where the tab contents start/end.  This provides a small
  // amount of slop inside the tab before a swipe will change tabs.
  auto contents_insets = tab->tab_style_views()->GetContentsInsets();
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
  tab_edges.Inset(
      gfx::Insets::VH(0, (tab->tab_style()->GetTabOverlap() + 1) / 2));
  const int x = (direction == LEFT)
                    ? std::min(tab_bounds.x() + left, tab_edges.right())
                    : std::max(tab_bounds.right() - right, tab_edges.x());

  return gfx::Point(x, tab_bounds.CenterPoint().y());
}

bool TabScrubberChromeOS::IsActivationPending() {
  return activate_timer_.IsRunning();
}

void TabScrubberChromeOS::SetEnabled(bool enabled) {
  enabled_ = enabled;
}

void TabScrubberChromeOS::SynthesizedScrollEvent(float x_offset,
                                                 bool is_fling_scroll_event) {
  // EventType::kScrollFlingStart and EventType::kScrollFlingCancel are both
  // handled in the same way inside OnScrollEvent(), so we can set
  // EventType::kScrollFlingStart if `is_fling_scroll_event` is true.
  // TODO(crbug.com/40207972): Instead of generating event here, use the real
  // event passed from wayland.
  ui::EventType event_type = is_fling_scroll_event
                                 ? ui::EventType::kScrollFlingStart
                                 : ui::EventType::kScroll;
  // Set `y_offset` as zero so that its absolute value is always not larger than
  // that of `x_offset` to represent the horizontal scroll.
  constexpr float y_offset = 0.f;
  ui::ScrollEvent event(event_type, gfx::PointF(), gfx::PointF(),
                        ui::EventTimeForNow(),
                        /*flags=*/0, x_offset, y_offset,
                        /*x_offset_ordinal=*/0.f,
                        /*y_offset_ordinal=*/0.f, kFingerCount);
  OnScrollEvent(&event);
}

TabScrubberChromeOS::TabScrubberChromeOS() {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  ash::Shell::Get()->AddPreTargetHandler(this);
#endif
  BrowserList::AddObserver(this);
}

TabScrubberChromeOS::~TabScrubberChromeOS() {
  BrowserList::RemoveObserver(this);
}

void TabScrubberChromeOS::OnScrollEvent(ui::ScrollEvent* event) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  // TODO(crbug.com/40207972): Generalize the interception/handling of
  // 3-finger swipes in Ash/ChromeOS.
  bool delegated = MaybeDelegateHandlingToLacros(event);
  if (delegated) {
    return;
  }
#endif

  if (!enabled_) {
    return;
  }

  if (event->IsFlingScrollEvent()) {
    // If we are not scrubbing, do not stop mark the event as handled here so
    // that other events can consume it.
    if (FinishScrub(true)) {
      event->SetHandled();
    }
    immersive_reveal_lock_.reset();
    return;
  }

  if (event->finger_count() != kFingerCount) {
    return;
  }

  Browser* browser = GetActiveBrowser();
  if (!browser || (scrubbing_ && browser_ && browser != browser_) ||
      (highlighted_tab_ != -1 &&
       highlighted_tab_ >= browser->tab_strip_model()->count())) {
    if (FinishScrub(false)) {
      event->SetHandled();
    }
    return;
  }

  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser);
  TabStrip* tab_strip = browser_view->tabstrip();

  if (tab_strip->IsAnimating()) {
    if (FinishScrub(false)) {
      event->SetHandled();
    }
    return;
  }

  // If the scroll is vertical, do not start scrubbing.
  if (!scrubbing_ &&
      std::abs(event->x_offset()) < std::abs(event->y_offset())) {
    return;
  }

  // We are handling the event.
  event->SetHandled();

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

  int new_index = tab_strip_->GetModelIndexOf(new_tab).value();
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
    new_tab->tab_style_views()->SetHoverLocation(hover_point);
  }
}

void TabScrubberChromeOS::OnBrowserRemoved(Browser* browser) {
  if (browser != browser_)
    return;

#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (browser_) {
    BrowserView::GetBrowserViewForBrowser(browser_)
        ->GetWidget()
        ->ReleaseCapture();
  }
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  activate_timer_.Stop();
  swipe_x_ = -1;
  swipe_y_ = -1;
  scrubbing_ = false;
  highlighted_tab_ = -1;
  browser_ = nullptr;
  tab_strip_ = nullptr;
}

void TabScrubberChromeOS::OnTabAdded(int index) {
  if (highlighted_tab_ == -1)
    return;

  if (index < highlighted_tab_)
    ++highlighted_tab_;
}

void TabScrubberChromeOS::OnTabMoved(int from_index, int to_index) {
  if (highlighted_tab_ == -1)
    return;

  if (from_index == highlighted_tab_)
    highlighted_tab_ = to_index;
  else if (from_index < highlighted_tab_ && highlighted_tab_ <= to_index)
    --highlighted_tab_;
  else if (from_index > highlighted_tab_ && highlighted_tab_ >= to_index)
    ++highlighted_tab_;
}

void TabScrubberChromeOS::OnTabRemoved(int index) {
  if (highlighted_tab_ == -1)
    return;
  if (index == highlighted_tab_) {
    FinishScrub(false);
    return;
  }
  if (index < highlighted_tab_)
    --highlighted_tab_;
}

Browser* TabScrubberChromeOS::GetActiveBrowser() {
  Browser* browser = chrome::FindLastActive();
  if (!browser || !browser->SupportsWindowFeature(Browser::FEATURE_TABSTRIP) ||
      !browser->window()->IsActive()) {
    return nullptr;
  }

  return browser;
}

void TabScrubberChromeOS::BeginScrub(BrowserView* browser_view,
                                     float x_offset) {
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
    immersive_reveal_lock_ = immersive_controller->GetRevealedLock(
        ImmersiveModeController::ANIMATE_REVEAL_YES);
  }

  tab_strip_->SetTabStripObserver(this);

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Capture the event so that the scroll event will not be handled by other
  // clients. This is required to work well with overview mode gesture, and not
  // needed for Lacros since the overview mode handling is done on Ash.
  browser_view->GetWidget()->SetCapture(/*view=*/nullptr);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
}

bool TabScrubberChromeOS::FinishScrub(bool activate) {
  const int stops_scrubbing = scrubbing_;
  activate_timer_.Stop();

  if (browser_ && browser_->window()) {
    BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser_);

#if BUILDFLAG(IS_CHROMEOS_ASH)
    browser_view->GetWidget()->ReleaseCapture();
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

    TabStrip* tab_strip = browser_view->tabstrip();
    if (activate && highlighted_tab_ != -1) {
      Tab* tab = tab_strip->tab_at(highlighted_tab_);
      tab->tab_style_views()->HideHover(TabStyle::HideHoverStyle::kImmediate);
      int distance = std::abs(highlighted_tab_ -
                              browser_->tab_strip_model()->active_index());
      UMA_HISTOGRAM_CUSTOM_COUNTS("Tabs.ScrubDistance", distance, 1, 20, 21);
      UMA_HISTOGRAM_TIMES("Tabs.ScrubDuration",
                          base::TimeTicks::Now() - scrubbing_start_time_);
      browser_->tab_strip_model()->ActivateTabAt(
          highlighted_tab_,
          TabStripUserGestureDetails(
              TabStripUserGestureDetails::GestureType::kOther));
    }
    tab_strip->SetTabStripObserver(nullptr);
  }

  browser_ = nullptr;
  tab_strip_ = nullptr;
  swipe_x_ = -1;
  swipe_y_ = -1;
  scrubbing_ = false;
  highlighted_tab_ = -1;

  return stops_scrubbing;
}

void TabScrubberChromeOS::ScheduleFinishScrubIfNeeded() {
  // Tests use a really long delay to ensure RunLoops don't unnecessarily
  // trigger the timer running.
  const base::TimeDelta delay =
      base::Milliseconds(use_default_activation_delay_ ? 200 : 20000);
  activate_timer_.Start(
      FROM_HERE, delay,
      base::BindRepeating(base::IgnoreResult(&TabScrubberChromeOS::FinishScrub),
                          base::Unretained(this), true));
}

void TabScrubberChromeOS::ScrubDirectionChanged(Direction direction) {
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

void TabScrubberChromeOS::UpdateSwipeX(float x_offset) {
  DCHECK(browser_);
  DCHECK(tab_strip_);
  DCHECK(scrubbing_);

  // Make the swipe speed inversely proportional with the number or tabs:
  // Each added tab introduces a reduction of 2% in |x_offset|, with a value of
  // one fourth of |x_offset| as the minimum (i.e. we need 38 tabs to reach
  // that minimum reduction). Please note, x_offset might be negative.
  float min = 0.25 * x_offset;
  float max = x_offset;
  if (x_offset < 0)
    std::swap(min, max);
  swipe_x_ += std::clamp(
      x_offset - (tab_strip_->GetTabCount() * 0.02f * x_offset), min, max);

  // In an RTL layout, everything is mirrored, i.e. the index of the first tab
  // (with the smallest X mirrored co-ordinates) is actually the index of the
  // last tab. Same for the index of the last tab.
  int first_tab_index = base::i18n::IsRTL() ? tab_strip_->GetTabCount() - 1 : 0;
  int last_tab_index = base::i18n::IsRTL() ? 0 : tab_strip_->GetTabCount() - 1;

  Tab* first_tab = tab_strip_->tab_at(first_tab_index);
  int first_tab_center = first_tab->GetMirroredBounds().CenterPoint().x();
  Tab* last_tab = tab_strip_->tab_at(last_tab_index);
  int last_tab_center = last_tab->GetMirroredBounds().CenterPoint().x();

  if (swipe_x_ < first_tab_center)
    swipe_x_ = first_tab_center;
  if (swipe_x_ > last_tab_center)
    swipe_x_ = last_tab_center;
}

void TabScrubberChromeOS::UpdateHighlightedTab(Tab* new_tab, int new_index) {
  DCHECK(scrubbing_);
  DCHECK(new_tab);

  if (new_index == highlighted_tab_)
    return;

  if (highlighted_tab_ != -1) {
    Tab* tab = tab_strip_->tab_at(highlighted_tab_);
    tab->tab_style_views()->HideHover(TabStyle::HideHoverStyle::kImmediate);
  }

  if (new_index != browser_->tab_strip_model()->active_index()) {
    highlighted_tab_ = new_index;
    new_tab->tab_style_views()->ShowHover(
        TabStyle::ShowHoverStyle::kPronounced);
  } else {
    highlighted_tab_ = -1;
  }
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
// static
bool TabScrubberChromeOS::MaybeDelegateHandlingToLacros(
    ui::ScrollEvent* event) {
  auto* active_window =
      ash::Shell::Get()->activation_client()->GetActiveWindow();
  if (!active_window) {
    return false;
  }

  if (!crosapi::browser_util::IsLacrosWindow(active_window)) {
    return false;
  }

  if (event->IsFlingScrollEvent()) {
    // Do NOT stop propagation for fling scroll event since it may be consumed
    // elsewhere.
    crosapi::BrowserManager::Get()->HandleTabScrubbing(
        event->x_offset(), /*is_fling_scroll_event=*/true);
    return true;
  } else if (event->finger_count() == kFingerCount) {
    crosapi::BrowserManager::Get()->HandleTabScrubbing(
        event->x_offset(),
        /*is_fling_scroll_event=*/false);
    event->SetHandled();
    return true;
  }

  return false;
}
#endif
