// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/common/tab_drag_scroll_handler.h"

#include "base/functional/bind.h"
#include "base/i18n/rtl.h"
#include "base/time/time.h"
#include "ui/views/controls/scroll_view.h"

namespace {
bool IsHorizontalScrollEnabled(const views::ScrollView& scroll_view) {
  return scroll_view.GetHorizontalScrollBarMode() !=
         views::ScrollView::ScrollBarMode::kDisabled;
}

bool IsVerticalScrollEnabled(const views::ScrollView& scroll_view) {
  return scroll_view.GetVerticalScrollBarMode() !=
         views::ScrollView::ScrollBarMode::kDisabled;
}
}  // namespace

void TabDragScrollHandler::OnDraggedTabPositionUpdated(
    views::ScrollView& scroll_view,
    const gfx::Rect& dragged_view_bounds_in_scroll_view,
    const gfx::Rect& scroll_bounds_in_scroll_view) {
  auto* host_view = scroll_view.contents();

  // TODO(crbug.com/476493398): Tune this value, potentially making it dynamic
  // based on the drag offset from the bounds of the scroll view.
  constexpr float kScrollIncrement = 5;

  const gfx::Rect visible_bounds = scroll_view.GetVisibleRect();

  if (IsHorizontalScrollEnabled(scroll_view)) {
    const bool is_rtl = base::i18n::IsRTL();
    const bool can_scroll_trailing =
        visible_bounds.right() < host_view->width();
    const bool can_scroll_leading = visible_bounds.x() > 0;
    const bool is_dragging_to_right =
        dragged_view_bounds_in_scroll_view.right() >=
        scroll_bounds_in_scroll_view.right();
    const bool is_dragging_to_left = dragged_view_bounds_in_scroll_view.x() <=
                                     scroll_bounds_in_scroll_view.x();

    const bool is_dragging_to_trailing =
        can_scroll_trailing &&
        (is_rtl ? is_dragging_to_left : is_dragging_to_right);
    const bool is_dragging_to_leading =
        can_scroll_leading &&
        (is_rtl ? is_dragging_to_right : is_dragging_to_left);

    if (is_dragging_to_trailing) {
      StartOrContinueScrolling(scroll_view, kScrollIncrement);
    } else if (is_dragging_to_leading) {
      StartOrContinueScrolling(scroll_view, -1.0f * kScrollIncrement);
    } else {
      StopScrolling(scroll_view);
    }
  } else if (IsVerticalScrollEnabled(scroll_view)) {
    const bool is_dragging_to_bottom =
        visible_bounds.bottom() < host_view->height() &&
        dragged_view_bounds_in_scroll_view.bottom() >=
            scroll_bounds_in_scroll_view.bottom();

    const bool is_dragging_to_top =
        visible_bounds.y() > 0 && dragged_view_bounds_in_scroll_view.y() <=
                                      scroll_bounds_in_scroll_view.y();

    if (is_dragging_to_bottom) {
      StartOrContinueScrolling(scroll_view, kScrollIncrement);
    } else if (is_dragging_to_top) {
      StartOrContinueScrolling(scroll_view, -1.0f * kScrollIncrement);
    } else {
      StopScrolling(scroll_view);
    }
  } else {
    StopScrolling(scroll_view);
  }
}

void TabDragScrollHandler::StartOrContinueScrolling(
    views::ScrollView& scroll_view,
    float scroll_increment) {
  scroll_increment_ = scroll_increment;
  if (IsVerticalScrollEnabled(scroll_view)) {
    if (scroll_increment_ > 0) {
      scroll_view.SetOverflowGradientMask(
          views::ScrollView::GradientDirection::kVerticalTrailing);
    } else if (scroll_increment_ < 0) {
      scroll_view.SetOverflowGradientMask(
          views::ScrollView::GradientDirection::kVerticalLeading);
    }
  }

  if (scroll_timer_.IsRunning()) {
    return;
  }

  constexpr base::TimeDelta kScrollTimerDelay = base::Milliseconds(20);
  scroll_timer_.Start(
      FROM_HERE, kScrollTimerDelay,
      base::BindRepeating(&TabDragScrollHandler::UpdateScrollOffset,
                          base::Unretained(this), std::ref(scroll_view)));
}

void TabDragScrollHandler::StopScrolling(views::ScrollView& scroll_view) {
  scroll_timer_.Stop();
  if (IsVerticalScrollEnabled(scroll_view)) {
    scroll_view.SetOverflowGradientMask(
        views::ScrollView::GradientDirection::kVertical);
  }
}

void TabDragScrollHandler::UpdateScrollOffset(views::ScrollView& scroll_view) {
  if (IsHorizontalScrollEnabled(scroll_view)) {
    scroll_view.ScrollByOffset({scroll_increment_, 0});
  } else if (IsVerticalScrollEnabled(scroll_view)) {
    scroll_view.ScrollByOffset({0, scroll_increment_});
  }
}
