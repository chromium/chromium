// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSER_APIS_TAB_DRAG_ADAPTERS_TAB_DRAG_WINDOW_ADAPTER_H_
#define COMPONENTS_BROWSER_APIS_TAB_DRAG_ADAPTERS_TAB_DRAG_WINDOW_ADAPTER_H_

#include "base/memory/weak_ptr.h"
#include "ui/gfx/geometry/point.h"

namespace tabs_api {

// Represents a browser window for the TabDragAPI.
class TabDragWindowAdapter {
 public:
  virtual ~TabDragWindowAdapter() = default;

  // Converts a point in screen coordinates to local window coordinates.
  virtual gfx::Point ConvertScreenPointToLocal(
      const gfx::Point& screen_point) const = 0;

  virtual base::WeakPtr<TabDragWindowAdapter> AsWeakPtr() = 0;
};

}  // namespace tabs_api

#endif  // COMPONENTS_BROWSER_APIS_TAB_DRAG_ADAPTERS_TAB_DRAG_WINDOW_ADAPTER_H_
