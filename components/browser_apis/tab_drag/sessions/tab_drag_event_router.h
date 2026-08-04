// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSER_APIS_TAB_DRAG_SESSIONS_TAB_DRAG_EVENT_ROUTER_H_
#define COMPONENTS_BROWSER_APIS_TAB_DRAG_SESSIONS_TAB_DRAG_EVENT_ROUTER_H_

#include <vector>

#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "components/browser_apis/tab_drag/adapters/tab_drag_window_adapter.h"
#include "components/browser_apis/tab_drag/sessions/tab_drag_session_listener.h"
#include "components/browser_apis/tab_strip/types/node_id.h"
#include "ui/gfx/geometry/point.h"

namespace tabs_api {

class DropTargetRegistry;

// Routes tab drag events to the right DropTarget. It reacts to high-level
// events from the TabDragSession and dispatches Mojo calls using the
// DropTargetRegistry.
enum class DropTargetEvent {
  kEntered,
  kDrag,
  kLeave,
  kDrop,
  kCancelled,
};

class TabDragEventRouter : public TabDragSessionListener {
 public:
  explicit TabDragEventRouter(DropTargetRegistry& registry);
  TabDragEventRouter(const TabDragEventRouter&) = delete;
  TabDragEventRouter& operator=(const TabDragEventRouter&) = delete;
  ~TabDragEventRouter() override;

  // TabDragSessionListener overrides:
  void OnSessionStarted(std::vector<tabs_api::NodeId> dragged_tabs,
                        TabDragWindowId source_window_id,
                        const gfx::Point& start_point,
                        int32_t tab_original_offset_x) override;
  void OnTargetChanged(DropTargetId new_target,
                       const gfx::Point& screen_point) override;
  void OnDragMoved(const gfx::Point& screen_point) override;
  void OnDragDetached(const gfx::Point& screen_point) override;
  void OnSessionDropped(const gfx::Point& screen_point) override;
  void OnSessionCancelled() override;

  base::WeakPtr<TabDragEventRouter> AsWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

 private:
  void TransitionToTarget(DropTargetId new_target,
                          const gfx::Point& screen_point);

  void DispatchEvent(DropTargetId target_id,
                     DropTargetEvent event,
                     const gfx::Point& screen_point = gfx::Point());

  const raw_ref<DropTargetRegistry> registry_;
  std::vector<tabs_api::NodeId> dragged_tabs_;
  int32_t tab_original_offset_x_ = 0;
  DropTargetId current_drop_target_;
  base::WeakPtrFactory<TabDragEventRouter> weak_factory_{this};
};

}  // namespace tabs_api

#endif  // COMPONENTS_BROWSER_APIS_TAB_DRAG_SESSIONS_TAB_DRAG_EVENT_ROUTER_H_
