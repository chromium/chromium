// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/vertical/tab_drag_scroll_handler.h"

#include "base/functional/bind.h"
#include "base/time/time.h"
#include "ui/views/controls/scroll_view.h"

void TabDragScrollHandler::OnDraggedTabPositionUpdated(
    views::ScrollView& scroll_view,
    const gfx::Rect& dragged_view_bounds_in_scroll_view) {
  auto* host_view = scroll_view.contents();

  // TODO(crbug.com/476493398): Tune this value, potentially making it dynamic
  // based on the drag offset from the bounds of the scroll view.
  constexpr float kScrollIncrement = 10;

  const auto& visible_bounds = host_view->GetVisibleBounds();
  if (visible_bounds.bottom() < host_view->height() &&
      dragged_view_bounds_in_scroll_view.bottom() >= scroll_view.height()) {
    StartOrContinueScrolling(scroll_view, kScrollIncrement);
  } else if (visible_bounds.y() > 0 &&
             dragged_view_bounds_in_scroll_view.y() <= 0) {
    StartOrContinueScrolling(scroll_view, -1.0 * kScrollIncrement);
  } else {
    StopScrolling();
  }
}

void TabDragScrollHandler::StartOrContinueScrolling(
    views::ScrollView& scroll_view,
    float vertical_increments) {
  vertical_scroll_increment_ = vertical_increments;
  if (scroll_timer_.IsRunning()) {
    return;
  }

  constexpr base::TimeDelta kScrollTimerDelay = base::Milliseconds(10);
  scroll_timer_.Start(
      FROM_HERE, kScrollTimerDelay,
      base::BindRepeating(&TabDragScrollHandler::UpdateScrollOffset,
                          base::Unretained(this), std::ref(scroll_view)));
}

void TabDragScrollHandler::StopScrolling() {
  scroll_timer_.Stop();
}

void TabDragScrollHandler::UpdateScrollOffset(views::ScrollView& scroll_view) {
  scroll_view.ScrollByOffset({0, vertical_scroll_increment_});
}
