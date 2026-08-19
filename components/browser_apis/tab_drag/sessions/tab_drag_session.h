// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSER_APIS_TAB_DRAG_SESSIONS_TAB_DRAG_SESSION_H_
#define COMPONENTS_BROWSER_APIS_TAB_DRAG_SESSIONS_TAB_DRAG_SESSION_H_

#include <optional>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/types/expected.h"
#include "components/browser_apis/tab_drag/adapters/tab_drag_window_adapter.h"
#include "components/browser_apis/tab_drag/destinations/drop_target_id.h"
#include "components/browser_apis/tab_drag/tab_drag_types.h"
#include "components/browser_apis/tab_strip/types/node_id.h"
#include "mojo/public/mojom/base/error.mojom-forward.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/vector2d.h"

namespace tabs_api {

class DropTarget;
class TabDragSessionInjector;
class TabDragWindowRegistry;
struct TabDragInputEvent;

// Platform-agnostic coordinator for tab dragging.
// Managed and owned by TabDragSessionManager.
class TabDragSession {
 public:
  // `injector` must outlive this session.
  TabDragSession(TabDragSessionParams params,
                 base::OnceClosure end_callback,
                 TabDragSessionInjector* injector);
  TabDragSession(const TabDragSession&) = delete;
  TabDragSession& operator=(const TabDragSession&) = delete;
  ~TabDragSession();

  // Starts the session by initiating input capture.
  base::expected<void, mojo_base::mojom::ErrorPtr> Start();

  // Updates the window hosting the drag session and transfers input capture
  // to it.
  void UpdateDraggedWindow(TabDragWindowId new_window_id);

  // Called when a drop target is registered during an active session.
  void OnDropTargetRegistered(DropTargetId target_id,
                              TabDragWindowId window_id);

  TabDragWindowId dragged_window() const { return dragged_window_; }

  const gfx::Point& last_mouse_screen_point() const {
    return last_mouse_screen_point_;
  }
  const TabDragSessionParams& params() const { return params_; }
  TabDragSessionInjector* injector() const { return &*injector_; }

  enum class DragMode {
    kAttachedToWindow,
    kDetaching,
    kRunningWindowMoveLoop,
    kWaitingToExitMoveLoop,
    kAttaching,
  };
  void set_drag_mode_for_testing(DragMode mode) { drag_mode_ = mode; }
  DragMode drag_mode() const { return drag_mode_; }

 private:
  void EndSession();
  void OnInputEvent(const TabDragInputEvent& event);
  void HandleMovedEvent(const gfx::Point& screen_point);

  // Attached mode handlers
  void HandleMoveWhileAttached(const gfx::Point& screen_point);
  bool ShouldDragWholeWindow() const;
  bool ShouldTearOff(const gfx::Point& screen_point) const;
  void DetachAndStartWindowDrag(const gfx::Point& screen_point);
  void StartWindowDrag(TabDragWindowId window_id,
                       const gfx::Point& screen_point);

  // Detached mode handlers
  void HandleMoveWhileDetached(const gfx::Point& screen_point);
  DropTarget* FindReattachmentTargetAtPoint(
      const gfx::Point& screen_point) const;
  bool CanReattachToTarget(DropTarget* target,
                           const gfx::Point& screen_point) const;
  void OnWindowMoved(const gfx::Point& cursor_screen_point);
  void CompleteReattachment();
  void CompleteWindowDrop(DragMoveLoopResult loop_result,
                          const gfx::Point& screen_point);
  void TransferDragToWindow(TabDragWindowId target_window_id,
                            bool activate_target_window);

  const TabDragSessionParams params_;
  const raw_ref<TabDragSessionInjector> injector_;

  base::OnceClosure end_callback_;

  struct PendingReattachment {
    TabDragWindowId window_id;
    DropTargetId target_id;
    gfx::Point screen_point;
  };

  gfx::Point last_mouse_screen_point_;
  TabDragWindowId dragged_window_;
  TabDragWindowRegistry* registry() const;
  DragMode drag_mode_ = DragMode::kAttachedToWindow;
  std::optional<PendingReattachment> pending_reattachment_;

  base::WeakPtrFactory<TabDragSession> weak_factory_{this};
};

}  // namespace tabs_api

#endif  // COMPONENTS_BROWSER_APIS_TAB_DRAG_SESSIONS_TAB_DRAG_SESSION_H_
