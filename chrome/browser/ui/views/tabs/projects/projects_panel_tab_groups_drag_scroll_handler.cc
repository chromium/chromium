// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/projects/projects_panel_tab_groups_drag_scroll_handler.h"

#include "base/functional/bind.h"
#include "base/time/time.h"
#include "ui/views/controls/scroll_view.h"

namespace {
// Space within the top and bottom bounds of the scroll view to trigger
// scrolling.
constexpr int kDragBoundary = 16;
}  // namespace

void ProjectsPanelTabGroupsDragScrollHandler::OnDraggedTabGroupPositionUpdated(
    views::ScrollView& scroll_view,
    const gfx::Point& drag_location_in_scroll_view) {
  auto* host_view = scroll_view.contents();

  constexpr float kScrollIncrement = 5;

  const auto& visible_bounds = host_view->GetVisibleBounds();
  if (visible_bounds.bottom() < host_view->height() &&
      drag_location_in_scroll_view.y() >=
          scroll_view.height() - kDragBoundary) {
    StartOrContinueScrolling(scroll_view, kScrollIncrement);
  } else if (visible_bounds.y() > 0 &&
             drag_location_in_scroll_view.y() <= kDragBoundary) {
    StartOrContinueScrolling(scroll_view, -1.0 * kScrollIncrement);
  } else {
    StopScrolling();
  }
}

void ProjectsPanelTabGroupsDragScrollHandler::StartOrContinueScrolling(
    views::ScrollView& scroll_view,
    float vertical_increments) {
  vertical_scroll_increment_ = vertical_increments;
  if (scroll_timer_.IsRunning()) {
    return;
  }

  constexpr base::TimeDelta kScrollTimerDelay = base::Milliseconds(10);
  scroll_timer_.Start(
      FROM_HERE, kScrollTimerDelay,
      base::BindRepeating(
          &ProjectsPanelTabGroupsDragScrollHandler::UpdateScrollOffset,
          base::Unretained(this), std::ref(scroll_view)));
}

void ProjectsPanelTabGroupsDragScrollHandler::StopScrolling() {
  scroll_timer_.Stop();
}

void ProjectsPanelTabGroupsDragScrollHandler::UpdateScrollOffset(
    views::ScrollView& scroll_view) {
  auto* contents = scroll_view.contents();
  if (!contents) {
    return;
  }

  // Prevent overscrolling.
  const float current_y = scroll_view.CurrentOffset().y();
  const float max_y =
      std::max(0, contents->height() - scroll_view.GetVisibleRect().height());
  const float new_y =
      std::clamp(current_y + vertical_scroll_increment_, 0.0f, max_y);

  scroll_view.ScrollToOffset({0, new_y});
}
