// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browser_apis/tab_drag/testing/toy_tab_drag_session_listener.h"

#include "components/browser_apis/tab_drag/tab_drag_types.h"

namespace tabs_api {

ToyTabDragSessionListener::ToyTabDragSessionListener() = default;
ToyTabDragSessionListener::~ToyTabDragSessionListener() = default;

void ToyTabDragSessionListener::OnSessionStarted(
    const TabDragSessionParams& params) {
  CHECK(params.source_window_id);
  events_.push_back({.type = Event::Type::kStarted,
                     .window_id = params.source_window_id,
                     .point = params.start_point,
                     .dragged_tabs = params.source_tab_ids});
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
