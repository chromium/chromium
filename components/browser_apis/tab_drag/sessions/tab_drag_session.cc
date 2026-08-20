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
                               base::OnceClosure end_callback,
                               TabDragSessionInjector* injector)
    : params_(std::move(params)),
      injector_(CHECK_DEREF(injector)),
      end_callback_(std::move(end_callback)),
      last_mouse_screen_point_(params_.start_point),
      dragged_window_(params_.source_window_id) {
  CHECK(registry());
  CHECK(dragged_window_);
  TabDragWindowAdapter* source_window = registry()->Get(dragged_window_);
  CHECK(source_window);
}

base::expected<void, mojo_base::mojom::ErrorPtr> TabDragSession::Start() {
  TabDragWindowAdapter* window = registry()->Get(dragged_window_);
  CHECK(window);

  auto result = injector_->GetInputAdapter().StartInputCapture(
      base::BindRepeating(&TabDragSession::OnInputEvent,
                          base::Unretained(this)),
      window);
  if (!result.has_value()) {
    return result;
  }

  injector_->GetSessionListener().OnSessionStarted(params_);

  if (ShouldDragWholeWindow()) {
    StartWindowDrag(dragged_window_, params_.start_point);
  }

  return base::ok();
}

TabDragSession::~TabDragSession() {
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

void TabDragSession::OnDropTargetRegistered(DropTargetId target_id,
                                            TabDragWindowId window_id) {
  if (dragged_window_ == window_id &&
      drag_mode_ == DragMode::kRunningWindowMoveLoop) {
    injector_->GetSessionListener().OnTargetChanged(target_id,
                                                    last_mouse_screen_point_);
  }
}

void TabDragSession::UpdateDraggedWindow(TabDragWindowId new_window_id) {
  TransferDragToWindow(new_window_id, /*activate_target_window=*/false);
}

void TabDragSession::TransferDragToWindow(TabDragWindowId target_window_id,
                                          bool activate_target_window) {
  CHECK(target_window_id);
  dragged_window_ = target_window_id;
  if (TabDragWindowAdapter* target_window = registry()->Get(dragged_window_)) {
    injector_->GetInputAdapter().SetActiveWindowContext(target_window);
    if (activate_target_window) {
      target_window->Activate();
    }
  }
}

void TabDragSession::OnInputEvent(const TabDragInputEvent& event) {
  if (event.type == TabDragInputEvent::Type::kMoved ||
      event.type == TabDragInputEvent::Type::kDropped) {
    last_mouse_screen_point_ = event.screen_point;
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
          drag_mode_ == DragMode::kRunningWindowMoveLoop) {
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
      if (drag_mode_ != DragMode::kRunningWindowMoveLoop) {
        HandleMovedEvent(event.screen_point);
      }
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
    case DragMode::kRunningWindowMoveLoop:
      HandleMoveWhileDetached(screen_point);
      break;
  }
}

void TabDragSession::HandleMoveWhileAttached(const gfx::Point& screen_point) {
  if (ShouldDragWholeWindow()) {
    StartWindowDrag(dragged_window_, screen_point);
    return;
  }

  if (ShouldTearOff(screen_point)) {
    DetachAndStartWindowDrag(screen_point);
  } else {
    injector_->GetSessionListener().OnDragMoved(screen_point);
  }
}

bool TabDragSession::ShouldDragWholeWindow() const {
  TabDragWindowAdapter* source_window = registry()->Get(dragged_window_);
  if (!source_window) {
    return false;
  }
  return source_window->ShouldDragWholeWindow(params_.source_tab_ids.size());
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

  if (!bounds.Contains(local_point)) {
    return true;
  }

  // Prevent tab from invading left-side controls (e.g. traffic lights).
  // If the tab's leading visual edge crosses past the drop target origin minus
  // threshold, trigger tear-off.
  const int tab_leading_edge_x =
      local_point.x() - params_.tab_original_offset_x;
  if (tab_leading_edge_x < (bounds_opt->x() - kTearThreshold)) {
    return true;
  }

  return false;
}

void TabDragSession::DetachAndStartWindowDrag(const gfx::Point& screen_point) {
  drag_mode_ = DragMode::kDetaching;
  injector_->GetSessionListener().OnDragDetached(screen_point);

  TabDragWindowAdapter* source_window = registry()->Get(dragged_window_);
  CHECK(source_window);

  DropTargetRegistry& drop_target_registry = injector_->GetDropTargetRegistry();
  DropTargetId target_id =
      drop_target_registry.FindTargetForWindow(dragged_window_);

  DropTarget* target = drop_target_registry.GetDropTarget(target_id);
  std::optional<gfx::Rect> bounds_opt =
      target ? target->cached_bounds() : std::nullopt;

  constexpr int kTearThreshold = 15;
  bool is_vertical_detachment = false;

  if (target && bounds_opt) {
    gfx::Point local_point = target->ConvertScreenPointToLocal(screen_point);
    const int top_threshold = bounds_opt->y() - kTearThreshold;
    const int bottom_threshold = bounds_opt->bottom() + kTearThreshold;
    if (local_point.y() < top_threshold || local_point.y() > bottom_threshold) {
      is_vertical_detachment = true;
    }
  }

  int detach_x = 0;
  if (is_vertical_detachment) {
    // For vertical detachment, preserve the tab's horizontal position as it
    // pertains to the tabstrip by matching the source window's horizontal
    // offset.
    detach_x = screen_point.x() - source_window->GetBoundsInScreen().x();
  } else {
    // For horizontal stretch detachment (dragged beyond left or right
    // boundary), the tab is set as the 1st tab of the new window (at
    // drop_target_x).
    const int drop_target_x = bounds_opt ? bounds_opt->x() : 0;
    detach_x = drop_target_x + params_.tab_original_offset_x;
  }

  gfx::Vector2d detach_window_offset(
      detach_x,
      params_.start_point.y() - source_window->GetBoundsInScreen().y());

  auto detach_result = source_window->DetachToNewWindow(
      params_.source_tab_ids, screen_point, detach_window_offset);
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

void TabDragSession::StartWindowDrag(TabDragWindowId window_id,
                                     const gfx::Point& screen_point) {
  CHECK(drag_mode_ == DragMode::kAttachedToWindow ||
        drag_mode_ == DragMode::kDetaching);

  // Transition to kRunningWindowMoveLoop before releasing capture so
  // OnInputEvent ignores the resulting kMouseCaptureChanged event.
  drag_mode_ = DragMode::kRunningWindowMoveLoop;

  TabDragWindowAdapter* window = registry()->Get(window_id);
  CHECK(window);

  injector_->GetInputAdapter().SuspendInputCapture();

  base::WeakPtr<TabDragSession> weak_this = weak_factory_.GetWeakPtr();

  gfx::Vector2d window_drag_offset =
      screen_point - window->GetBoundsInScreen().origin();

  DragMoveLoopResult loop_result = window->RunWindowMoveLoop(
      screen_point, window_drag_offset,
      base::BindRepeating(&TabDragSession::OnWindowMoved, weak_this));

  if (!weak_this) {
    return;
  }

  injector_->GetInputAdapter().ResumeInputCapture();

  if (drag_mode_ == DragMode::kWaitingToExitMoveLoop) {
    CompleteReattachment();
  } else {
    CompleteWindowDrop(loop_result, screen_point);
  }
}

void TabDragSession::HandleMoveWhileDetached(const gfx::Point& screen_point) {
  DropTarget* target = FindReattachmentTargetAtPoint(screen_point);
  if (!target) {
    return;
  }

  TabDragWindowAdapter* detached_window = registry()->Get(dragged_window_);
  CHECK(detached_window);

  // Defer tab migration and target transition until the native move loop
  // has completely returned and unwound on the callstack.
  pending_reattachment_ = PendingReattachment{
      .window_id = target->window_id(),
      .target_id = target->id(),
      .screen_point = screen_point,
  };
  drag_mode_ = DragMode::kWaitingToExitMoveLoop;

  detached_window->EndWindowMoveLoop();
}

DropTarget* TabDragSession::FindReattachmentTargetAtPoint(
    const gfx::Point& screen_point) const {
  DropTargetRegistry& drop_target_registry = injector_->GetDropTargetRegistry();
  DropTargetId exclude_target =
      drop_target_registry.FindTargetForWindow(dragged_window_);
  DropTargetId target_id =
      drop_target_registry.FindTargetAtPoint(screen_point, exclude_target);
  if (!target_id) {
    return nullptr;
  }

  DropTarget* target = drop_target_registry.GetDropTarget(target_id);
  if (!target || !CanReattachToTarget(target, screen_point)) {
    return nullptr;
  }

  return target;
}

bool TabDragSession::CanReattachToTarget(DropTarget* target,
                                         const gfx::Point& screen_point) const {
  std::optional<gfx::Rect> bounds_opt = target->cached_bounds();
  if (!bounds_opt) {
    return false;
  }
  TabDragWindowAdapter* window = target->window();
  if (!window) {
    return false;
  }

  gfx::Point local_point =
      window->ConvertScreenPointToLocal(target->native_view(), screen_point);

  // Must be within the target drop target bounds.
  if (!bounds_opt->Contains(local_point)) {
    return false;
  }

  // Tab leading edge must be at or inside the tabstrip origin (bounds.x()).
  // This ensures that upon reattachment, ShouldTearOff (which triggers at
  // bounds.x() - kTearThreshold) will NOT immediately fire, preventing
  // detach/reattach oscillation and jitter.
  const int tab_leading_edge_x =
      local_point.x() - params_.tab_original_offset_x;
  if (tab_leading_edge_x < bounds_opt->x()) {
    return false;
  }

  return true;
}

void TabDragSession::OnWindowMoved(const gfx::Point& cursor_screen_point) {
  last_mouse_screen_point_ = cursor_screen_point;
  HandleMovedEvent(cursor_screen_point);
}

void TabDragSession::CompleteReattachment() {
  CHECK(pending_reattachment_.has_value());
  PendingReattachment target =
      *std::exchange(pending_reattachment_, std::nullopt);

  drag_mode_ = DragMode::kAttaching;
  TabDragWindowAdapter* detached_window = registry()->Get(dragged_window_);
  if (!detached_window) {
    drag_mode_ = DragMode::kRunningWindowMoveLoop;
    injector_->GetSessionListener().OnSessionCancelled();
    EndSession();
    return;
  }

  auto migrate_result =
      detached_window->MigrateTabs(target.window_id, params_.source_tab_ids);
  if (!migrate_result.has_value()) {
    drag_mode_ = DragMode::kRunningWindowMoveLoop;
    injector_->GetSessionListener().OnSessionCancelled();
    EndSession();
    return;
  }

  TransferDragToWindow(target.window_id, /*activate_target_window=*/true);
  drag_mode_ = DragMode::kAttachedToWindow;
  injector_->GetSessionListener().OnTargetChanged(target.target_id,
                                                  target.screen_point);
}

void TabDragSession::CompleteWindowDrop(DragMoveLoopResult loop_result,
                                        const gfx::Point& screen_point) {
  if (drag_mode_ == DragMode::kRunningWindowMoveLoop) {
    if (loop_result == DragMoveLoopResult::kSuccess) {
      injector_->GetSessionListener().OnSessionDropped(screen_point);
    } else {
      injector_->GetSessionListener().OnSessionCancelled();
    }
    EndSession();
  }
}

}  // namespace tabs_api
