// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSER_APIS_TAB_DRAG_TESTING_TOY_TAB_DRAG_SESSION_LISTENER_H_
#define COMPONENTS_BROWSER_APIS_TAB_DRAG_TESTING_TOY_TAB_DRAG_SESSION_LISTENER_H_

#include <vector>

#include "base/memory/raw_ptr.h"
#include "components/browser_apis/tab_drag/adapters/tab_drag_window_adapter.h"
#include "components/browser_apis/tab_drag/destinations/drop_target_id.h"
#include "components/browser_apis/tab_drag/sessions/tab_drag_session_listener.h"
#include "components/browser_apis/tab_strip/types/node_id.h"
#include "ui/gfx/geometry/point.h"

namespace tabs_api {

class ToyTabDragSessionListener : public TabDragSessionListener {
 public:
  struct Event {
    enum class Type {
      kStarted,
      kTargetChanged,
      kMoved,
      kDetached,
      kDropped,
      kCancelled,
    };
    Type type;
    TabDragWindowId window_id;
    DropTargetId target;
    gfx::Point point;
    std::vector<tabs_api::NodeId> dragged_tabs;
  };

  ToyTabDragSessionListener();
  ~ToyTabDragSessionListener() override;

  // TabDragSessionListener:
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

  const std::vector<Event>& events() const { return events_; }
  void Clear() { events_.clear(); }

 private:
  std::vector<Event> events_;
};

}  // namespace tabs_api

#endif  // COMPONENTS_BROWSER_APIS_TAB_DRAG_TESTING_TOY_TAB_DRAG_SESSION_LISTENER_H_
