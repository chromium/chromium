// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSER_APIS_TAB_DRAG_SESSIONS_TAB_DRAG_SESSION_H_
#define COMPONENTS_BROWSER_APIS_TAB_DRAG_SESSIONS_TAB_DRAG_SESSION_H_

#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/types/expected.h"
#include "components/browser_apis/tab_drag/adapters/tab_drag_window_adapter.h"
#include "components/browser_apis/tab_strip/types/node_id.h"
#include "mojo/public/mojom/base/error.mojom-forward.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/vector2d.h"

namespace tabs_api {

class TabDragSessionInjector;
class TabDragWindowRegistry;
struct TabDragInputEvent;

struct TabDragSessionParams {
  TabDragWindowId source_window_id;
  std::vector<tabs_api::NodeId> source_tab_ids;
  gfx::Point start_point;
  base::OnceClosure end_callback;
};

// Platform-agnostic coordinator for tab dragging.
// Managed and owned by TabDragSessionManager.
class TabDragSession {
 public:
  // `injector` must outlive this session.
  TabDragSession(TabDragSessionParams params, TabDragSessionInjector* injector);
  TabDragSession(const TabDragSession&) = delete;
  TabDragSession& operator=(const TabDragSession&) = delete;
  ~TabDragSession();

  // Starts the session by initiating input capture.
  base::expected<void, mojo_base::mojom::ErrorPtr> Start();

  // Updates the window hosting the drag session and transfers input capture
  // to it.
  void UpdateDraggedWindow(TabDragWindowId new_window_id);

  const gfx::Point& start_point_in_screen() const {
    return start_point_in_screen_;
  }
  const gfx::Point& last_mouse_screen_point() const {
    return last_mouse_screen_point_;
  }
  const gfx::Vector2d& delta() const { return delta_; }
  const std::vector<tabs_api::NodeId>& dragged_tabs() const {
    return dragged_tabs_;
  }
  TabDragSessionInjector* injector() const { return &*injector_; }

  enum class DragMode {
    kAttachedToWindow,
    kDetaching,
    kAttaching,
    kDetachedWindow,
    kWaitingToExitMoveLoop,
  };
  void set_drag_mode_for_testing(DragMode mode) { drag_mode_ = mode; }

 private:
  void OnWindowMoved(const gfx::Point& cursor_screen_point);

  void EndSession();
  void OnInputEvent(const TabDragInputEvent& event);

  void HandleMovedEvent(const gfx::Point& screen_point);
  void HandleMoveWhileAttached(const gfx::Point& screen_point);
  void HandleMoveWhileDetached(const gfx::Point& screen_point);

  bool IsDraggingEntireWindow() const;
  bool ShouldTearOff(const gfx::Point& screen_point) const;
  void StartWindowDrag(TabDragWindowId window_id,
                       const gfx::Point& screen_point);
  void DetachAndStartWindowDrag(const gfx::Point& screen_point);

  std::vector<tabs_api::NodeId> dragged_tabs_;
  const raw_ref<TabDragSessionInjector> injector_;

  base::OnceClosure end_callback_;

  const gfx::Point start_point_in_screen_;
  gfx::Point last_mouse_screen_point_;
  gfx::Vector2d delta_;
  TabDragWindowId dragged_window_;
  TabDragWindowRegistry* registry() const;
  DragMode drag_mode_ = DragMode::kAttachedToWindow;
  gfx::Vector2d start_window_offset_;

  base::WeakPtrFactory<TabDragSession> weak_factory_{this};
};

}  // namespace tabs_api

#endif  // COMPONENTS_BROWSER_APIS_TAB_DRAG_SESSIONS_TAB_DRAG_SESSION_H_
