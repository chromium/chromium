// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSER_APIS_TAB_DRAG_TAB_DRAG_TYPES_H_
#define COMPONENTS_BROWSER_APIS_TAB_DRAG_TAB_DRAG_TYPES_H_

#include <vector>

#include "components/browser_apis/tab_drag/adapters/tab_drag_window_adapter.h"
#include "components/browser_apis/tab_strip/types/node_id.h"
#include "ui/gfx/geometry/point.h"

namespace tabs_api {

// Parameters describing the configuration of a tab drag session.
struct TabDragSessionParams {
  TabDragWindowId source_window_id;
  std::vector<tabs_api::NodeId> source_tab_ids;
  gfx::Point start_point;
  int tab_original_offset_x = 0;
  float mouse_to_tab_x_ratio = 0.0f;
};

}  // namespace tabs_api

#endif  // COMPONENTS_BROWSER_APIS_TAB_DRAG_TAB_DRAG_TYPES_H_
