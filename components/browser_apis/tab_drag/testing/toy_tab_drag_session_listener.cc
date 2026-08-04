// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browser_apis/tab_drag/testing/toy_tab_drag_session_listener.h"

#include <utility>


namespace tabs_api {

ToyTabDragSessionListener::ToyTabDragSessionListener() = default;
ToyTabDragSessionListener::~ToyTabDragSessionListener() = default;

void ToyTabDragSessionListener::OnSessionStarted(
    std::vector<tabs_api::NodeId> dragged_tabs,
    TabDragWindowId source_window_id,
    const gfx::Point& start_point,
    int32_t tab_original_offset_x) {
  CHECK(source_window_id);
  events_.push_back({.type = Event::Type::kStarted,
                     .window_id = source_window_id,
                     .point = start_point,
                     .dragged_tabs = std::move(dragged_tabs)});
}

void ToyTabDragSessionListener::OnTargetChanged(
    DropTargetId new_target,
    const gfx::Point& screen_point) {
  events_.push_back({.type = Event::Type::kTargetChanged,
                     .target = new_target,
                     .point = screen_point});
}

void ToyTabDragSessionListener::OnDragMoved(const gfx::Point& screen_point) {
  events_.push_back({.type = Event::Type::kMoved, .point = screen_point});
}

void ToyTabDragSessionListener::OnDragDetached(const gfx::Point& screen_point) {
  events_.push_back({.type = Event::Type::kDetached, .point = screen_point});
}

void ToyTabDragSessionListener::OnSessionDropped(
    const gfx::Point& screen_point) {
  events_.push_back({.type = Event::Type::kDropped, .point = screen_point});
}

void ToyTabDragSessionListener::OnSessionCancelled() {
  events_.push_back({.type = Event::Type::kCancelled});
}

}  // namespace tabs_api
