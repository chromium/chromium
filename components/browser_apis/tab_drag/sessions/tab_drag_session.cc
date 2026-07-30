// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browser_apis/tab_drag/sessions/tab_drag_session.h"

#include "base/check_deref.h"
#include "base/functional/bind.h"
#include "components/browser_apis/tab_drag/adapters/tab_drag_session_input_adapter.h"
#include "components/browser_apis/tab_drag/adapters/tab_drag_window_adapter.h"
#include "components/browser_apis/tab_drag/destinations/drop_target.h"
#include "components/browser_apis/tab_drag/destinations/drop_target_registry.h"
#include "components/browser_apis/tab_drag/sessions/tab_drag_session_injector.h"
#include "components/browser_apis/tab_drag/sessions/tab_drag_session_listener.h"
#include "components/browser_apis/tab_drag/sessions/tab_drag_window_registry.h"
#include "ui/gfx/geometry/vector2d.h"

namespace tabs_api {

TabDragSession::TabDragSession(TabDragSessionParams params,
                               TabDragSessionInjector* injector)
    : dragged_tabs_(std::move(params.source_tab_ids)),
      injector_(CHECK_DEREF(injector)),
      end_callback_(std::move(params.end_callback)),
      start_point_in_screen_(params.start_point),
      last_mouse_screen_point_(params.start_point),
      dragged_window_(params.source_window_id) {
  CHECK(registry());
  CHECK(dragged_window_);
  TabDragWindowAdapter* source_window = registry()->Get(dragged_window_);
  CHECK(source_window);
  start_window_offset_ =
      params.start_point - source_window->GetBoundsInScreen().origin();
}

base::expected<void, mojo_base::mojom::ErrorPtr> TabDragSession::Start() {
  auto result =
      injector_->GetInputAdapter().StartInputCapture(base::BindRepeating(
          &TabDragSession::OnInputEvent, base::Unretained(this)));
  if (result.has_value()) {
    TabDragWindowAdapter* window = registry()->Get(dragged_window_);
    CHECK(window);
    window->SetCapture();
    injector_->GetSessionListener().OnSessionStarted(
        dragged_tabs_, dragged_window_, start_point_in_screen_);
  }
  return result;
}

TabDragSession::~TabDragSession() {
  if (TabDragWindowAdapter* window = registry()->Get(dragged_window_)) {
    window->ReleaseCapture();
  }
  injector_->GetInputAdapter().ReleaseInputCapture();
}

TabDragWindowRegistry* TabDragSession::registry() const {
  return injector_->GetWindowRegistry();
}

void TabDragSession::EndSession() {
  if (end_callback_) {
    std::move(end_callback_).Run();
  }
}

void TabDragSession::UpdateDraggedWindow(TabDragWindowId new_window_id) {
  CHECK(new_window_id);
  if (TabDragWindowAdapter* window = registry()->Get(dragged_window_)) {
    window->ReleaseCapture();
  }
  dragged_window_ = new_window_id;
  if (TabDragWindowAdapter* window = registry()->Get(dragged_window_)) {
    window->SetCapture();
  }
}

void TabDragSession::OnInputEvent(const TabDragInputEvent& event) {
  if (event.type == TabDragInputEvent::Type::kMoved ||
      event.type == TabDragInputEvent::Type::kDropped) {
    last_mouse_screen_point_ = event.screen_point;
    delta_ = event.screen_point - start_point_in_screen_;
  }

  switch (event.type) {
    case TabDragInputEvent::Type::kCancelled:
      injector_->GetSessionListener().OnSessionCancelled();
      EndSession();
      break;
    case TabDragInputEvent::Type::kCaptureChanged: {
      if (drag_mode_ == DragMode::kDetaching ||
          drag_mode_ == DragMode::kAttaching ||
          drag_mode_ == DragMode::kWaitingToExitMoveLoop ||
          drag_mode_ == DragMode::kDetachedWindow) {
        break;
      }
      TabDragWindowAdapter* window = registry()->Get(dragged_window_);
      if (window && window->HasCapture()) {
        break;
      }
      injector_->GetSessionListener().OnSessionCancelled();
      EndSession();
      break;
    }
    case TabDragInputEvent::Type::kDropped:
      injector_->GetSessionListener().OnSessionDropped(event.screen_point);
      EndSession();
      break;
    case TabDragInputEvent::Type::kMoved:
      HandleMovedEvent(event.screen_point);
      break;
  }
}

void TabDragSession::HandleMovedEvent(const gfx::Point& screen_point) {
  switch (drag_mode_) {
    case DragMode::kAttachedToWindow:
      HandleMoveWhileAttached(screen_point);
      break;
    case DragMode::kDetaching:
    case DragMode::kAttaching:
    case DragMode::kWaitingToExitMoveLoop:
      // Transient state; ignore move events to prevent reentrancy during loop
      // exit.
      break;
    case DragMode::kDetachedWindow:
      HandleMoveWhileDetached(screen_point);
      break;
  }
}

void TabDragSession::HandleMoveWhileAttached(const gfx::Point& screen_point) {
  if (IsDraggingEntireWindow()) {
    StartWindowDrag(dragged_window_, screen_point);
    return;
  }

  if (ShouldTearOff(screen_point)) {
    DetachAndStartWindowDrag(screen_point);
  } else {
    injector_->GetSessionListener().OnDragMoved(screen_point);
  }
}

void TabDragSession::HandleMoveWhileDetached(const gfx::Point& screen_point) {
  DropTargetRegistry& drop_target_registry = injector_->GetDropTargetRegistry();
  DropTargetId exclude_target =
      drop_target_registry.FindTargetForWindow(dragged_window_);
  DropTargetId new_target_id =
      drop_target_registry.FindTargetAtPoint(screen_point, exclude_target);

  if (new_target_id) {
    if (DropTarget* target =
            drop_target_registry.GetDropTarget(new_target_id)) {
      TabDragWindowId target_window_id = target->window_id();
      CHECK(target_window_id);

      TabDragWindowAdapter* detached_window = registry()->Get(dragged_window_);
      CHECK(detached_window);

      drag_mode_ = DragMode::kWaitingToExitMoveLoop;
      detached_window->EndWindowMoveLoop();
      drag_mode_ = DragMode::kAttaching;

      auto migrate_result =
          detached_window->MigrateTabs(target_window_id, dragged_tabs_);
      if (!migrate_result.has_value()) {
        drag_mode_ = DragMode::kDetachedWindow;
        injector_->GetSessionListener().OnSessionCancelled();
        EndSession();
        return;
      }

      UpdateDraggedWindow(target_window_id);
      drag_mode_ = DragMode::kAttachedToWindow;
      injector_->GetSessionListener().OnTargetChanged(new_target_id,
                                                      screen_point);
      return;
    }
  }
}

bool TabDragSession::IsDraggingEntireWindow() const {
  TabDragWindowAdapter* source_window = registry()->Get(dragged_window_);
  if (!source_window) {
    return false;
  }
  return source_window->IsDraggingEntireWindow(dragged_tabs_.size());
}

bool TabDragSession::ShouldTearOff(const gfx::Point& screen_point) const {
  DropTargetRegistry& drop_target_registry = injector_->GetDropTargetRegistry();
  DropTargetId target_id =
      drop_target_registry.FindTargetForWindow(dragged_window_);

  CHECK(target_id) << "Source window must have a registered drop target";

  DropTarget* target = drop_target_registry.GetDropTarget(target_id);
  CHECK(target) << "Active drop target must exist";

  std::optional<gfx::Rect> bounds_opt = target->cached_bounds();
  CHECK(bounds_opt) << "Active drop target must have cached bounds";

  constexpr int kTearThreshold = 15;

  gfx::Point local_point = target->ConvertScreenPointToLocal(screen_point);
  gfx::Rect bounds = *bounds_opt;
  bounds.Inset(-kTearThreshold);
  return !bounds.Contains(local_point);
}

void TabDragSession::StartWindowDrag(TabDragWindowId window_id,
                                     const gfx::Point& screen_point) {
  drag_mode_ = DragMode::kDetachedWindow;
  injector_->GetSessionListener().OnDragDetached(screen_point);

  TabDragWindowAdapter* window = registry()->Get(window_id);
  CHECK(window);

  base::WeakPtr<TabDragSession> weak_this = weak_factory_.GetWeakPtr();

  DragMoveLoopResult loop_result = window->RunWindowMoveLoop(
      screen_point, start_window_offset_,
      base::BindRepeating(&TabDragSession::OnWindowMoved, weak_this));

  if (!weak_this) {
    return;
  }

  if (drag_mode_ == DragMode::kDetachedWindow) {
    if (loop_result == DragMoveLoopResult::kSuccess) {
      injector_->GetSessionListener().OnSessionDropped(screen_point);
    } else {
      injector_->GetSessionListener().OnSessionCancelled();
    }
    EndSession();
  }
}

void TabDragSession::DetachAndStartWindowDrag(const gfx::Point& screen_point) {
  drag_mode_ = DragMode::kDetaching;
  TabDragWindowAdapter* source_window = registry()->Get(dragged_window_);
  CHECK(source_window);
  auto detach_result = source_window->DetachToNewWindow(
      dragged_tabs_, screen_point, start_window_offset_);
  if (!detach_result.has_value()) {
    drag_mode_ = DragMode::kAttachedToWindow;
    injector_->GetSessionListener().OnSessionCancelled();
    EndSession();
    return;
  }
  TabDragWindowId new_window_id = detach_result.value();
  UpdateDraggedWindow(new_window_id);
  StartWindowDrag(new_window_id, screen_point);
}

void TabDragSession::OnWindowMoved(const gfx::Point& cursor_screen_point) {
  last_mouse_screen_point_ = cursor_screen_point;
  HandleMovedEvent(cursor_screen_point);
}

}  // namespace tabs_api
