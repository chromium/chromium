// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSER_APIS_TAB_DRAG_SESSIONS_TAB_DRAG_SESSION_INPUT_LISTENER_H_
#define COMPONENTS_BROWSER_APIS_TAB_DRAG_SESSIONS_TAB_DRAG_SESSION_INPUT_LISTENER_H_

#include "ui/gfx/geometry/point.h"

namespace tabs_api {

class TabDragSession;

struct TabDragSessionInputEvent {
  enum class Type {
    kMoved,
    kCancelled,
    kDropped,
  };
  Type type;
  gfx::Point screen_point;
};

class TabDragSessionInputListener {
 public:
  virtual ~TabDragSessionInputListener() = default;

  virtual void OnSessionStarted(TabDragSession* session) = 0;
  virtual void OnSessionEnded() = 0;
  virtual void OnDragSessionEvent(const TabDragSessionInputEvent& event) = 0;
};

}  // namespace tabs_api

#endif  // COMPONENTS_BROWSER_APIS_TAB_DRAG_SESSIONS_TAB_DRAG_SESSION_INPUT_LISTENER_H_
