// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browser_apis/tab_drag/sessions/tab_drag_event_router.h"

#include <utility>

#include "base/check.h"
#include "components/browser_apis/tab_drag/destinations/drop_target.h"
#include "components/browser_apis/tab_drag/destinations/drop_target_registry.h"

namespace tabs_api {

TabDragEventRouter::TabDragEventRouter(DropTargetRegistry& registry)
    : registry_(registry) {}

TabDragEventRouter::~TabDragEventRouter() = default;

void TabDragEventRouter::OnSessionStarted(
    std::vector<tabs_api::NodeId> dragged_tabs,
    TabDragWindowId source_window_id,
    const gfx::Point& start_point,
    int32_t tab_original_offset_x) {
  CHECK(source_window_id);
  dragged_tabs_ = std::move(dragged_tabs);
  tab_original_offset_x_ = tab_original_offset_x;
  DropTargetId source_target = registry_->FindTargetForWindow(source_window_id);
  TransitionToTarget(source_target, start_point);
}

void TabDragEventRouter::OnTargetChanged(DropTargetId new_target,
                                         const gfx::Point& screen_point) {
  TransitionToTarget(new_target, screen_point);
}

void TabDragEventRouter::OnDragMoved(const gfx::Point& screen_point) {
  if (current_drop_target_) {
    DispatchEvent(current_drop_target_, DropTargetEvent::kDrag, screen_point);
  }
}

void TabDragEventRouter::OnDragDetached(const gfx::Point& screen_point) {
  TransitionToTarget(DropTargetId(), screen_point);
}

void TabDragEventRouter::OnSessionDropped(const gfx::Point& screen_point) {
  if (current_drop_target_) {
    DispatchEvent(current_drop_target_, DropTargetEvent::kDrop, screen_point);
    current_drop_target_ = DropTargetId();
  }
  dragged_tabs_.clear();
}

void TabDragEventRouter::OnSessionCancelled() {
  if (current_drop_target_) {
    DispatchEvent(current_drop_target_, DropTargetEvent::kCancelled);
    current_drop_target_ = DropTargetId();
  }
  dragged_tabs_.clear();
}

void TabDragEventRouter::TransitionToTarget(DropTargetId new_target,
                                            const gfx::Point& screen_point) {
  if (current_drop_target_) {
    DispatchEvent(current_drop_target_, DropTargetEvent::kLeave);
  }
  current_drop_target_ = new_target;
  if (current_drop_target_) {
    DispatchEvent(current_drop_target_, DropTargetEvent::kEntered,
                  screen_point);
  }
}

void TabDragEventRouter::DispatchEvent(DropTargetId target_id,
                                       DropTargetEvent event,
                                       const gfx::Point& screen_point) {
  DropTarget* target = registry_->GetDropTarget(target_id);
  if (!target) {
    return;
  }

  switch (event) {
    case DropTargetEvent::kEntered:
      target->DragEnter(dragged_tabs_, screen_point, tab_original_offset_x_);
      break;
    case DropTargetEvent::kDrag:
      target->DragOver(screen_point);
      break;
    case DropTargetEvent::kLeave:
      target->DragLeave();
      break;
    case DropTargetEvent::kDrop:
      target->Drop(dragged_tabs_, screen_point);
      break;
    case DropTargetEvent::kCancelled:
      target->DragCancel();
      break;
  }
}

}  // namespace tabs_api
