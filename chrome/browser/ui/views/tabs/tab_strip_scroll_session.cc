// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/tab_strip_scroll_session.h"

#include <algorithm>

#include "base/check.h"
#include "chrome/browser/ui/views/tabs/tab_drag_controller.h"
#include "ui/gfx/geometry/point.h"
#include "ui/views/controls/scroll_view.h"

namespace {
// Duration after which the repeating timer event is called
const base::TimeDelta kScrollTimerDelay = base::Milliseconds(10);
// This is used to calculate the offset but is  also helpful
// for different minimum sizes of tabs.
const int kNumberOfTabsScrolledPerSecond = 4;
}  // namespace

TabStripScrollSession::TabStripScrollSession(
    TabDragWithScrollManager& tab_drag_with_scroll_manager)
    : tab_drag_with_scroll_manager_(tab_drag_with_scroll_manager) {}

TabStripScrollSession::~TabStripScrollSession() = default;

TabStripScrollSessionWithTimer::TabStripScrollSessionWithTimer(
    TabDragWithScrollManager& tab_drag_with_scroll_manager,
    ScrollSessionTimerType timer_type)
    : TabStripScrollSession(tab_drag_with_scroll_manager),
      scroll_timer_(std::make_unique<base::RepeatingTimer>()),
      timer_type_(timer_type) {}

TabStripScrollSessionWithTimer::~TabStripScrollSessionWithTimer() = default;

void TabStripScrollSessionWithTimer::MaybeStart() {
  if (!tab_drag_with_scroll_manager_->GetAttachedContext() || IsRunning())
    return;

  const TabStripScrollSession::TabScrollDirection scroll_direction =
      GetTabScrollDirection();

  if (scroll_direction != TabStripScrollSession::TabScrollDirection::kNoScroll)
    Start(scroll_direction);
}

void TabStripScrollSessionWithTimer::Start(TabScrollDirection direction) {
  scroll_timer_->Start(
      FROM_HERE, kScrollTimerDelay,
      base::BindRepeating(&TabStripScrollSessionWithTimer::TabScrollCallback,
                          base::Unretained(this)));
  scroll_direction_ = direction;
  scroll_context_ = tab_drag_with_scroll_manager_->GetAttachedContext();
}

void TabStripScrollSessionWithTimer::TabScrollCallback() {
  DCHECK(scroll_direction_ !=
         TabStripScrollSession::TabScrollDirection::kNoScroll);
  if (GetTabScrollDirection() != scroll_direction_ ||
      !tab_drag_with_scroll_manager_->IsDraggingTabState() ||
      scroll_context_ != tab_drag_with_scroll_manager_->GetAttachedContext()) {
    scroll_timer_->Stop();
    return;
  }

  const int tab_scroll_offset = CalculateSpeed();
  views::ScrollView* const scroll_view =
      tab_drag_with_scroll_manager_->GetScrollView();
  scroll_view->ScrollByOffset(gfx::PointF(tab_scroll_offset, 0));

  tab_drag_with_scroll_manager_->MoveAttached(
      tab_drag_with_scroll_manager_->GetLastPointInScreen(), false);
}

void TabStripScrollSessionWithTimer::Stop() {
  scroll_timer_->Stop();
  scroll_direction_ = TabScrollDirection::kNoScroll;
  scroll_context_ = nullptr;
}

bool TabStripScrollSessionWithTimer::IsRunning() {
  return scroll_timer_->IsRunning();
}

int TabStripScrollSessionWithTimer::CalculateSpeed() {
  // TODO(crbug.com/40875170): Use the expected offset at a given time to
  // calculate the current offset. This can help with making up
  // for rounding off the calculation to int in the next call.
  // Also use the time elapsed to calculate the expected offset.
  DCHECK(scroll_direction_ !=
         TabStripScrollSession::TabScrollDirection::kNoScroll);
  const int tab_scroll_offset =
      (scroll_direction_ == TabScrollDirection::kScrollTowardsTrailingTabs)
          ? ceil(CalculateBaseScrollOffset())
          : floor(-CalculateBaseScrollOffset());

  switch (timer_type_) {
    case TabStripScrollSessionWithTimer::ScrollSessionTimerType::kConstantTimer:
      return tab_scroll_offset;
    case TabStripScrollSessionWithTimer::ScrollSessionTimerType::kVariableTimer:
      if (scroll_direction_ == TabScrollDirection::kScrollTowardsTrailingTabs) {
        return ceil(
            std::clamp(GetRatioInScrollableRegion() * tab_scroll_offset, 0.0,
                        CalculateBaseScrollOffset() * 3));
      } else {
        return floor(
            std::clamp(GetRatioInScrollableRegion() * tab_scroll_offset,
                        CalculateBaseScrollOffset() * -3, 0.0));
      }
    default:
      NOTREACHED();
  }
}

double TabStripScrollSessionWithTimer::CalculateBaseScrollOffset() {
  return kNumberOfTabsScrolledPerSecond *
         TabStyle::Get()->GetMinimumInactiveWidth() *
         (kScrollTimerDelay / base::Milliseconds(1000));
}

double TabStripScrollSessionWithTimer::GetRatioInScrollableRegion() {
  const gfx::Rect dragged_tabs_rect_drag_context_coord =
      tab_drag_with_scroll_manager_->GetEnclosingRectForDraggedTabs();
  const views::ScrollView* const scroll_view =
      tab_drag_with_scroll_manager_->GetScrollView();
  const gfx::Rect visible_rect_drag_context_coord =
      gfx::ToEnclosingRect(views::View::ConvertRectToTarget(
          scroll_view->contents(), scroll_context_,
          gfx::RectF(scroll_view->GetVisibleRect())));

  double ratio = 0;
  double scrollable_start = 0;

  switch (scroll_direction_) {
    case TabStripScrollSession::TabScrollDirection::kScrollTowardsTrailingTabs:
      scrollable_start =
          visible_rect_drag_context_coord.right() - GetScrollableOffset();
      ratio =
          (dragged_tabs_rect_drag_context_coord.right() - scrollable_start) /
          GetScrollableOffset();
      return ratio;
    case TabStripScrollSession::TabScrollDirection::kScrollTowardsLeadingTabs:
      scrollable_start =
          visible_rect_drag_context_coord.origin().x() + GetScrollableOffset();
      ratio = (scrollable_start -
               dragged_tabs_rect_drag_context_coord.origin().x()) /
              GetScrollableOffset();
      return ratio;
    default:
      return ratio;
  }
}

TabStripScrollSession::TabScrollDirection
TabStripScrollSessionWithTimer::GetTabScrollDirection() {
  const views::ScrollView* const scroll_view =
      tab_drag_with_scroll_manager_->GetScrollView();

  const gfx::Rect dragged_tabs_rect_drag_context_coord =
      tab_drag_with_scroll_manager_->GetEnclosingRectForDraggedTabs();
  const gfx::Rect visible_rect_drag_context_coord =
      gfx::ToEnclosingRect(views::View::ConvertRectToTarget(
          scroll_view->contents(),
          tab_drag_with_scroll_manager_->GetAttachedContext(),
          gfx::RectF(scroll_view->GetVisibleRect())));

  const bool maybe_scroll_towards_trailing_tabs =
      dragged_tabs_rect_drag_context_coord.right() >=
      (visible_rect_drag_context_coord.right() - GetScrollableOffset());

  const bool maybe_scroll_towards_leading_tabs =
      dragged_tabs_rect_drag_context_coord.origin().x() <=
      (visible_rect_drag_context_coord.origin().x() + GetScrollableOffset());

  // TODO(crbug.com/40875138): Add case for both maybe scroll left and right.
  // This would happen when many tabs are selected.
  if (maybe_scroll_towards_trailing_tabs) {
    return TabStripScrollSession::TabScrollDirection::
        kScrollTowardsTrailingTabs;
  } else if (maybe_scroll_towards_leading_tabs) {
    return TabStripScrollSession::TabScrollDirection::kScrollTowardsLeadingTabs;
  } else {
    return TabStripScrollSession::TabScrollDirection::kNoScroll;
  }
}

int TabStripScrollSession::GetScrollableOffset() const {
  return TabStyle::Get()->GetMinimumInactiveWidth() / 5;
}
