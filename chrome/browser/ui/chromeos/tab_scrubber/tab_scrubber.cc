// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/chromeos/tab_scrubber/tab_scrubber.h"

#include <stdint.h>

#include <algorithm>

#include "ash/shell.h"
#include "base/check.h"
#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_macros.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/immersive/immersive_mode_controller.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/tab_strip_user_gesture_details.h"
#include "chrome/browser/ui/tabs/tab_style.h"
#include "chrome/browser/ui/views/frame/base_tab_strip_region_view.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/tab_strip_region_view.h"
#include "chrome/browser/ui/views/tabs/common/tab_drag_handler.h"
#include "chrome/browser/ui/views/tabs/common/tab_strip_collection_controller.h"
#include "chrome/browser/ui/views/tabs/common/tab_strip_view.h"
#include "chrome/browser/ui/views/tabs/common/tab_view.h"
#include "chrome/browser/ui/views/tabs/tab.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "chrome/browser/ui/views/tabs/tab_style_views.h"
#include "chrome/browser/ui/window_feature_controller/window_feature_controller.h"
#include "chromeos/ash/components/browser_delegate/browser_controller.h"
#include "chromeos/ash/components/browser_delegate/browser_delegate.h"
#include "components/tabs/public/tab_interface.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/event.h"
#include "ui/events/event_utils.h"
#include "ui/events/gesture_detection/gesture_configuration.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/view_utils.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/public/activation_client.h"

namespace ash {

namespace {

TabScrubber* g_tab_scrubber = nullptr;

BrowserDelegate* GetActiveBrowser() {
  BrowserDelegate* browser =
      BrowserController::GetInstance()->GetLastUsedBrowser();
  if (!browser ||
      !WindowFeatureController::From(&browser->GetBrowser())
           ->SupportsWindowFeature(
               WindowFeatureController::WindowFeature::kFeatureTabStrip) ||
      !browser->IsActive()) {
    return nullptr;
  }
  return browser;
}

views::Widget* GetWidget(BrowserDelegate* browser) {
  CHECK(browser);
  return views::Widget::GetWidgetForNativeWindow(browser->GetNativeWindow());
}

gfx::Point ComputeStartPoint(const gfx::Rect& tab_bounds,
                             const gfx::Insets& contents_insets,
                             int tab_overlap,
                             TabScrubber::Direction direction) {
  // Start the swipe where the tab contents start/end.  This provides a small
  // amount of slop inside the tab before a swipe will change tabs.
  int left = contents_insets.left();
  int right = contents_insets.right();

  // The contents insets are logical rather than physical, so reverse them for
  // RTL.
  if (base::i18n::IsRTL()) {
    std::swap(left, right);
  }

  // For very narrow tabs, the contents insets may be too large.  Clamp to the
  // opposite edges of the tab, which should be at (overlap / 2).
  gfx::Rect tab_edges = tab_bounds;
  // For odd overlap values, be conservative and inset both edges rounding up.
  tab_edges.Inset(gfx::Insets::VH(0, (tab_overlap + 1) / 2));
  const int x = (direction == TabScrubber::LEFT)
                    ? std::min(tab_bounds.x() + left, tab_edges.right())
                    : std::max(tab_bounds.right() - right, tab_edges.x());

  return gfx::Point(x, tab_bounds.CenterPoint().y());
}

TabView* GetUnifiedTabViewAt(BrowserView* browser_view, int index) {
  if (!browser_view || !browser_view->browser()) {
    return nullptr;
  }
  auto* region_view = views::AsViewClass<BaseTabStripRegionView>(
      browser_view->tab_strip_view());
  if (!region_view) {
    return nullptr;
  }
  TabStripModel* model = browser_view->browser()->GetTabStripModel();
  if (!model || !model->ContainsIndex(index)) {
    return nullptr;
  }
  tabs::TabInterface* tab = model->GetTabAtIndex(index);
  if (!tab) {
    return nullptr;
  }
  return views::AsViewClass<TabView>(
      region_view->GetTabAnchorView(tab->GetHandle()));
}

struct UnifiedStripLayout {
  raw_ptr<views::View> pinned_contents = nullptr;
  raw_ptr<views::View> unpinned_contents = nullptr;
  int pinned_x_offset = 0;
  int unpinned_x_offset = 0;
};

// Computes an unscrolled virtual horizontal coordinate space across the pinned
// and unpinned scroll containers so scrub coordinates remain stable when the
// tab strip overflows and scrolls.
UnifiedStripLayout GetUnifiedStripLayout(views::View* tab_strip_view) {
  UnifiedStripLayout layout;
  auto* unified_strip = views::AsViewClass<TabStripView>(tab_strip_view);
  if (!unified_strip) {
    return layout;
  }
  if (views::ScrollView* pinned_scroll =
          unified_strip->pinned_tabs_scroll_view()) {
    layout.pinned_contents = pinned_scroll->contents();
  }
  if (views::ScrollView* unpinned_scroll =
          unified_strip->unpinned_tabs_scroll_view()) {
    layout.unpinned_contents = unpinned_scroll->contents();
  }
  const bool has_pinned =
      layout.pinned_contents && !layout.pinned_contents->children().empty();
  const bool has_unpinned =
      layout.unpinned_contents && !layout.unpinned_contents->children().empty();
  const int pinned_width = has_pinned ? layout.pinned_contents->width() : 0;
  const int unpinned_width =
      has_unpinned ? layout.unpinned_contents->width() : 0;
  const int overlap =
      (has_pinned && has_unpinned) ? TabStyle::Get()->GetTabOverlap() : 0;
  layout.pinned_x_offset =
      base::i18n::IsRTL() ? std::max(0, unpinned_width - overlap) : 0;
  layout.unpinned_x_offset =
      base::i18n::IsRTL() ? 0 : std::max(0, pinned_width - overlap);
  return layout;
}

TabView* FindTabViewInContainer(views::View* container,
                                const gfx::Point& local_point) {
  if (!container || container->children().empty() ||
      !container->GetLocalBounds().Contains(local_point)) {
    return nullptr;
  }
  views::View* view = container->GetEventHandlerForPoint(local_point);
  while (view && view != container && !views::IsViewClass<TabView>(view)) {
    view = view->parent();
  }
  return views::AsViewClass<TabView>(view);
}

TabView* FindUnifiedTabViewAtPoint(views::View* tab_strip_view,
                                   const gfx::Point& point) {
  if (!tab_strip_view) {
    return nullptr;
  }
  const UnifiedStripLayout layout = GetUnifiedStripLayout(tab_strip_view);
  if (TabView* tab_view = FindTabViewInContainer(
          layout.pinned_contents,
          gfx::Point(point.x() - layout.pinned_x_offset, point.y()))) {
    return tab_view;
  }
  if (TabView* tab_view = FindTabViewInContainer(
          layout.unpinned_contents,
          gfx::Point(point.x() - layout.unpinned_x_offset, point.y()))) {
    return tab_view;
  }
  return nullptr;
}

bool IsTabStripAnimating(BrowserView* browser_view, TabStrip* tab_strip) {
  if (tab_strip) {
    return tab_strip->IsAnimatingInTabStrip();
  }
  if (!browser_view) {
    return false;
  }
  auto* region_view = views::AsViewClass<BaseTabStripRegionView>(
      browser_view->tab_strip_view());
  if (!region_view) {
    return false;
  }
  const auto* controller = region_view->GetTabStripCollectionController();
  return controller && (controller->animation_coordinator().IsAnimating() ||
                        controller->GetDragHandler().IsDragging());
}

void ShowTabHoverAt(BrowserView* browser_view,
                    TabStrip* tab_strip,
                    int index,
                    TabStyle::ShowHoverStyle style) {
  if (tab_strip) {
    tab_strip->tab_at(index)->ShowHover(style);
    return;
  }
  if (TabView* tab_view = GetUnifiedTabViewAt(browser_view, index)) {
    tab_view->ShowHover(style);
  }
}

void HideTabHoverAt(BrowserView* browser_view,
                    TabStrip* tab_strip,
                    int index,
                    TabStyle::HideHoverStyle style) {
  if (tab_strip) {
    tab_strip->tab_at(index)->HideHover(style);
    return;
  }
  if (TabView* tab_view = GetUnifiedTabViewAt(browser_view, index)) {
    tab_view->HideHover(style);
  }
}

}  // namespace

// static
TabScrubber* TabScrubber::GetInstance() {
  return g_tab_scrubber;
}

// static
gfx::Point TabScrubber::GetStartPoint(TabStrip* tab_strip,
                                      int index,
                                      TabScrubber::Direction direction) {
  const Tab* tab = tab_strip->tab_at(index);
  return ComputeStartPoint(tab->GetMirroredBounds(),
                           tab->tab_style_views()->GetContentsInsets(),
                           tab->tab_style()->GetTabOverlap(), direction);
}

// static
gfx::Point TabScrubber::GetStartPoint(BrowserView* browser_view,
                                      int index,
                                      TabScrubber::Direction direction) {
  CHECK(browser_view);
  views::View* tab_strip_view =
      browser_view->tab_strip_view()->GetTabStripView();
  if (auto* tab_strip = views::AsViewClass<TabStrip>(tab_strip_view)) {
    return GetStartPoint(tab_strip, index, direction);
  }
  TabView* tab_view = GetUnifiedTabViewAt(browser_view, index);
  CHECK(tab_view);
  CHECK(tab_strip_view);
  const gfx::Rect tab_bounds = GetTabBoundsInStrip(browser_view, index);
  return ComputeStartPoint(
      tab_bounds, tab_view->tab_styling()->GetContentsInsets(),
      tab_view->tab_styling()->tab_style()->GetTabOverlap(), direction);
}

// static
gfx::Rect TabScrubber::GetTabBoundsInStrip(BrowserView* browser_view,
                                           int index) {
  if (!browser_view || !browser_view->tab_strip_view()) {
    return gfx::Rect();
  }
  views::View* tab_strip_view =
      browser_view->tab_strip_view()->GetTabStripView();
  if (!tab_strip_view) {
    return gfx::Rect();
  }
  if (auto* tab_strip = views::AsViewClass<TabStrip>(tab_strip_view)) {
    return tab_strip->tab_at(index)->GetMirroredBounds();
  }
  TabView* tab_view = GetUnifiedTabViewAt(browser_view, index);
  if (!tab_view) {
    return gfx::Rect();
  }
  const UnifiedStripLayout layout = GetUnifiedStripLayout(tab_strip_view);
  if (layout.pinned_contents && layout.pinned_contents->Contains(tab_view)) {
    gfx::Rect bounds = views::View::ConvertRectToTarget(
        tab_view, layout.pinned_contents, tab_view->GetLocalBounds());
    bounds.Offset(layout.pinned_x_offset, 0);
    return bounds;
  }
  if (layout.unpinned_contents &&
      layout.unpinned_contents->Contains(tab_view)) {
    gfx::Rect bounds = views::View::ConvertRectToTarget(
        tab_view, layout.unpinned_contents, tab_view->GetLocalBounds());
    bounds.Offset(layout.unpinned_x_offset, 0);
    return bounds;
  }
  return views::View::ConvertRectToTarget(tab_view, tab_strip_view,
                                          tab_view->GetLocalBounds());
}

bool TabScrubber::IsActivationPending() {
  return activate_timer_.IsRunning();
}

void TabScrubber::SetEnabled(bool enabled) {
  enabled_ = enabled;
}

void TabScrubber::SynthesizedScrollEvent(float x_offset,
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

TabScrubber::TabScrubber() {
  CHECK(!g_tab_scrubber);
  g_tab_scrubber = this;
  ash::Shell::Get()->AddPreTargetHandler(this);
  browser_controller_observation_.Observe(BrowserController::GetInstance());
}

TabScrubber::~TabScrubber() {
  CHECK_EQ(g_tab_scrubber, this);
  g_tab_scrubber = nullptr;
  ash::Shell::Get()->RemovePreTargetHandler(this);
}

void TabScrubber::OnScrollEvent(ui::ScrollEvent* event) {
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

  BrowserDelegate* browser = GetActiveBrowser();
  if (!browser || (scrubbing_ && browser_ && browser != browser_) ||
      (highlighted_tab_ != -1 && static_cast<size_t>(highlighted_tab_) >=
                                     browser->GetWebContentsCount())) {
    if (FinishScrub(false)) {
      event->SetHandled();
    }
    return;
  }

  BrowserView* browser_view =
      BrowserView::GetBrowserViewForBrowser(&browser->GetBrowser());

  if (scrubbing_ && IsTabStripAnimating(browser_view, tab_strip_)) {
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
  if (browser_view->ShouldDrawVerticalTabStrip()) {
    // TODO(crbug.com/484364227): TabScrubbing is not supported in VerticalTabs
    // at this point in time.
    return;
  }

  float x_offset = event->x_offset();
  if (!scrubbing_) {
    BeginScrub(browser_view, x_offset);
  } else if (highlighted_tab_ == -1) {
    // Has the direction of the swipe changed while scrubbing?
    Direction direction = (x_offset < 0) ? LEFT : RIGHT;
    if (direction != swipe_direction_) {
      ScrubDirectionChanged(direction);
    }
  }

  UpdateSwipeX(x_offset);

  const gfx::Point swipe_point(swipe_x_, swipe_y_);
  std::optional<int> new_index;
  if (tab_strip_) {
    if (Tab* new_tab = tab_strip_->GetTabAt(swipe_point)) {
      new_index = tab_strip_->GetModelIndexOf(new_tab);
    }
  } else {
    if (TabView* tab_view =
            FindUnifiedTabViewAtPoint(tab_strip_view_, swipe_point)) {
      if (const tabs::TabInterface* tab_interface =
              tab_view->GetTabInterface()) {
        const int model_index =
            browser_->GetBrowser().GetTabStripModel()->GetIndexOfTab(
                tab_interface);
        if (model_index != TabStripModel::kNoTab) {
          new_index = model_index;
        }
      }
    }
  }
  if (!new_index) {
    return;
  }

  if (highlighted_tab_ == -1 &&
      new_index.value() ==
          browser_->GetBrowser().GetTabStripModel()->active_index()) {
    return;
  }

  if (new_index.value() != highlighted_tab_) {
    if (activate_timer_.IsRunning()) {
      activate_timer_.Reset();
    } else {
      ScheduleFinishScrubIfNeeded();
    }
  }

  UpdateHighlightedTab(new_index.value());
}

void TabScrubber::OnBrowserClosed(BrowserDelegate* browser) {
  if (browser != browser_) {
    return;
  }

  if (browser_) {
    GetWidget(browser_)->ReleaseCapture();
  }

  activate_timer_.Stop();
  swipe_x_ = -1;
  swipe_y_ = -1;
  scrubbing_ = false;
  highlighted_tab_ = -1;
  browser_ = nullptr;
  tab_strip_ = nullptr;
  tab_strip_view_ = nullptr;
}

void TabScrubber::OnTabAdded(int index) {
  if (highlighted_tab_ == -1) {
    return;
  }

  if (index < highlighted_tab_) {
    ++highlighted_tab_;
  }
}

void TabScrubber::OnTabMoved(int from_index, int to_index) {
  if (highlighted_tab_ == -1) {
    return;
  }

  if (from_index == highlighted_tab_) {
    highlighted_tab_ = to_index;
  } else if (from_index < highlighted_tab_ && highlighted_tab_ <= to_index) {
    --highlighted_tab_;
  } else if (from_index > highlighted_tab_ && highlighted_tab_ >= to_index) {
    ++highlighted_tab_;
  }
}

void TabScrubber::OnTabRemoved(int index) {
  if (highlighted_tab_ == -1) {
    return;
  }
  if (index == highlighted_tab_) {
    FinishScrub(false);
    return;
  }
  if (index < highlighted_tab_) {
    --highlighted_tab_;
  }
}

void TabScrubber::BeginScrub(BrowserView* browser_view, float x_offset) {
  CHECK(browser_view, base::NotFatalUntil::M162);
  CHECK(browser_view->browser(), base::NotFatalUntil::M162);

  if (browser_view->ShouldDrawVerticalTabStrip()) {
    // TODO(crbug.com/484364227): TabScrubbing is not supported in VerticalTabs
    // at this point in time.
    return;
  }

  scrubbing_start_time_ = base::TimeTicks::Now();
  // TODO(crbug.com/465835455): Move TabScrubber into
  // HorizontalTabStripRegionView since the current implementation won't work
  // for Vertical Tabs.
  tab_strip_view_ = browser_view->tab_strip_view()->GetTabStripView();
  tab_strip_ = views::AsViewClass<TabStrip>(tab_strip_view_);
  scrubbing_ = true;
  browser_ =
      BrowserController::GetInstance()->GetDelegate(browser_view->browser());

  Direction direction = (x_offset < 0) ? LEFT : RIGHT;
  ScrubDirectionChanged(direction);

  auto* const immersive_controller =
      ImmersiveModeController::From(&browser_->GetBrowser());
  if (immersive_controller->IsEnabled()) {
    immersive_reveal_lock_ = immersive_controller->GetRevealedLock(
        ImmersiveModeController::ANIMATE_REVEAL_YES);
  }

  browser_view->tab_strip_view()->SetTabStripObserver(this);

  // Capture the event so that the scroll event will not be handled by other
  // clients. This is required to work well with overview mode gesture.
  GetWidget(browser_)->SetCapture(/*view=*/nullptr);
}

bool TabScrubber::FinishScrub(bool activate) {
  const int stops_scrubbing = scrubbing_;
  activate_timer_.Stop();

  if (browser_) {
    GetWidget(browser_)->ReleaseCapture();

    BrowserView* browser_view =
        BrowserView::GetBrowserViewForBrowser(&browser_->GetBrowser());
    if (activate && highlighted_tab_ != -1) {
      HideTabHoverAt(browser_view, tab_strip_, highlighted_tab_,
                     TabStyle::HideHoverStyle::kImmediate);
      int distance =
          std::abs(highlighted_tab_ -
                   browser_->GetBrowser().GetTabStripModel()->active_index());
      UMA_HISTOGRAM_CUSTOM_COUNTS("Tabs.ScrubDistance", distance, 1, 20, 21);
      UMA_HISTOGRAM_TIMES("Tabs.ScrubDuration",
                          base::TimeTicks::Now() - scrubbing_start_time_);
      browser_->GetBrowser().GetTabStripModel()->ActivateTabAt(
          highlighted_tab_,
          TabStripUserGestureDetails(
              TabStripUserGestureDetails::GestureType::kOther));
    }

    TabStripRegionView* tab_strip_view = browser_view->tab_strip_view();
    tab_strip_view->SetTabStripObserver(nullptr);
  }

  browser_ = nullptr;
  tab_strip_ = nullptr;
  tab_strip_view_ = nullptr;
  swipe_x_ = -1;
  swipe_y_ = -1;
  scrubbing_ = false;
  highlighted_tab_ = -1;

  return stops_scrubbing;
}

void TabScrubber::ScheduleFinishScrubIfNeeded() {
  // Tests use a really long delay to ensure RunLoops don't unnecessarily
  // trigger the timer running.
  const base::TimeDelta delay =
      base::Milliseconds(use_default_activation_delay_ ? 200 : 20000);
  activate_timer_.Start(
      FROM_HERE, delay,
      base::BindRepeating(base::IgnoreResult(&TabScrubber::FinishScrub),
                          base::Unretained(this), true));
}

void TabScrubber::ScrubDirectionChanged(Direction direction) {
  CHECK(browser_, base::NotFatalUntil::M162);
  CHECK(tab_strip_view_, base::NotFatalUntil::M162);
  CHECK(scrubbing_, base::NotFatalUntil::M162);

  swipe_direction_ = direction;
  BrowserView* browser_view =
      BrowserView::GetBrowserViewForBrowser(&browser_->GetBrowser());
  const gfx::Point start_point = GetStartPoint(
      browser_view, browser_->GetBrowser().GetTabStripModel()->active_index(),
      swipe_direction_);
  swipe_x_ = start_point.x();
  swipe_y_ = start_point.y();
}

void TabScrubber::UpdateSwipeX(float x_offset) {
  CHECK(browser_, base::NotFatalUntil::M162);
  CHECK(tab_strip_view_, base::NotFatalUntil::M162);
  CHECK(scrubbing_, base::NotFatalUntil::M162);

  BrowserView* browser_view =
      BrowserView::GetBrowserViewForBrowser(&browser_->GetBrowser());
  const int tab_count =
      tab_strip_ ? tab_strip_->GetTabCount()
                 : browser_->GetBrowser().GetTabStripModel()->count();
  if (tab_count <= 0) {
    return;
  }

  // Make the swipe speed inversely proportional with the number or tabs:
  // Each added tab introduces a reduction of 2% in |x_offset|, with a value of
  // one fourth of |x_offset| as the minimum (i.e. we need 38 tabs to reach
  // that minimum reduction). Please note, x_offset might be negative.
  float min = 0.25 * x_offset;
  float max = x_offset;
  if (x_offset < 0) {
    std::swap(min, max);
  }
  swipe_x_ += std::clamp(x_offset - (tab_count * 0.02f * x_offset), min, max);

  // In an RTL layout, everything is mirrored, i.e. the index of the first tab
  // (with the smallest X mirrored co-ordinates) is actually the index of the
  // last tab. Same for the index of the last tab.
  int first_tab_index = base::i18n::IsRTL() ? tab_count - 1 : 0;
  int last_tab_index = base::i18n::IsRTL() ? 0 : tab_count - 1;

  int first_tab_center =
      GetTabBoundsInStrip(browser_view, first_tab_index).CenterPoint().x();
  int last_tab_center =
      GetTabBoundsInStrip(browser_view, last_tab_index).CenterPoint().x();

  swipe_x_ = std::clamp(swipe_x_, static_cast<float>(first_tab_center),
                        static_cast<float>(last_tab_center));
}

void TabScrubber::UpdateHighlightedTab(int new_index) {
  CHECK(scrubbing_, base::NotFatalUntil::M162);

  if (new_index == highlighted_tab_) {
    return;
  }

  BrowserView* browser_view =
      BrowserView::GetBrowserViewForBrowser(&browser_->GetBrowser());
  if (highlighted_tab_ != -1) {
    HideTabHoverAt(browser_view, tab_strip_, highlighted_tab_,
                   TabStyle::HideHoverStyle::kImmediate);
  }

  if (new_index != browser_->GetBrowser().GetTabStripModel()->active_index()) {
    highlighted_tab_ = new_index;
    ShowTabHoverAt(browser_view, tab_strip_, new_index,
                   TabStyle::ShowHoverStyle::kPronounced);
  } else {
    highlighted_tab_ = -1;
  }

  if (!tab_strip_) {
    if (TabView* tab_view = GetUnifiedTabViewAt(browser_view, new_index)) {
      views::View* scroll_target = (tab_view->split() && tab_view->parent())
                                       ? tab_view->parent()
                                       : tab_view;
      scroll_target->ScrollViewToVisible();
    }
  }
}

}  // namespace ash
