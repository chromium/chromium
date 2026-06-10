// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSER_APIS_TAB_DRAG_SESSIONS_TAB_DRAG_SESSION_H_
#define COMPONENTS_BROWSER_APIS_TAB_DRAG_SESSIONS_TAB_DRAG_SESSION_H_

#include <memory>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "components/browser_apis/tab_drag/adapters/tab_drag_session_input_adapter.h"
#include "components/browser_apis/tab_drag/sessions/tab_drag_session_input_listener.h"
#include "components/browser_apis/tab_strip/types/node_id.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/vector2d.h"

namespace tabs_api {

// Platform-agnostic coordinator for tab dragging.
// Managed and owned by TabDragSessionManager.
class TabDragSession {
 public:
  TabDragSession(const std::vector<tabs_api::NodeId>& source_tab_ids,
                 const gfx::Point& start_point,
                 TabDragSessionInputAdapter& input_adapter,
                 TabDragSessionInputListener* listener,
                 base::OnceClosure end_callback);
  TabDragSession(const TabDragSession&) = delete;
  TabDragSession& operator=(const TabDragSession&) = delete;
  ~TabDragSession();

  // Explicitly cancel the session.
  void Cancel();

  // Starts the session by initiating input capture.
  base::expected<void, mojo_base::mojom::ErrorPtr> Start();

  const gfx::Point& start_point_in_screen() const {
    return start_point_in_screen_;
  }
  const gfx::Point& last_mouse_screen_point() const {
    return last_mouse_screen_point_;
  }
  const gfx::Vector2d& delta() const { return delta_; }

 private:
  void EndSession();
  void OnInputEvent(const TabDragInputEvent& event);

  std::vector<tabs_api::NodeId> dragged_tabs_;
  const raw_ref<TabDragSessionInputAdapter> input_adapter_;
  const raw_ptr<TabDragSessionInputListener> listener_;

  base::OnceClosure end_callback_;

  const gfx::Point start_point_in_screen_;
  gfx::Point last_mouse_screen_point_;
  gfx::Vector2d delta_;
};

}  // namespace tabs_api

#endif  // COMPONENTS_BROWSER_APIS_TAB_DRAG_SESSIONS_TAB_DRAG_SESSION_H_
