// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSER_APIS_TAB_DRAG_SESSIONS_TAB_DRAG_SESSION_LISTENER_H_
#define COMPONENTS_BROWSER_APIS_TAB_DRAG_SESSIONS_TAB_DRAG_SESSION_LISTENER_H_

#include "components/browser_apis/tab_drag/destinations/drop_target_id.h"
#include "components/browser_apis/tab_drag/tab_drag_types.h"
#include "ui/gfx/geometry/point.h"

namespace tabs_api {

class TabDragSessionListener {
 public:
  virtual ~TabDragSessionListener() = default;

  // Called when a new drag session starts.
  virtual void OnSessionStarted(const TabDragSessionParams& params) = 0;

  // Called when the active drop target for the drag changes.
  virtual void OnTargetChanged(DropTargetId new_target,
                               const gfx::Point& screen_point) = 0;

  // Called when the drag moves within the current target window.
  virtual void OnDragMoved(const gfx::Point& screen_point) = 0;

  // Called when the drag transitions from attached to detached (outside any
  // window).
  virtual void OnDragDetached(const gfx::Point& screen_point) = 0;

  // Called when the session ends with a drop.
  virtual void OnSessionDropped(const gfx::Point& screen_point) = 0;

  // Called when the session is cancelled.
  virtual void OnSessionCancelled() = 0;
};

}  // namespace tabs_api

#endif  // COMPONENTS_BROWSER_APIS_TAB_DRAG_SESSIONS_TAB_DRAG_SESSION_LISTENER_H_
