// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FULL_RESTORE_WINDOW_INFO_H_
#define COMPONENTS_FULL_RESTORE_WINDOW_INFO_H_

#include "base/optional.h"
#include "chromeos/ui/base/window_state_type.h"
#include "ui/aura/window.h"
#include "ui/gfx/geometry/rect.h"

namespace full_restore {

// This struct is the parameter for the interface SaveWindowInfo, to save the
// window information.
struct COMPONENT_EXPORT(FULL_RESTORE) WindowInfo {
 public:
  WindowInfo();
  WindowInfo(const WindowInfo&) = delete;
  WindowInfo& operator=(const WindowInfo&) = delete;
  ~WindowInfo();

  aura::Window* window;

  // Index in MruWindowTracker to restore window stack. A lower index
  // indicates a more recently used window.
  base::Optional<int32_t> activation_index;

  // Virtual desk id.
  base::Optional<int32_t> desk_id;

  // Whether the |window| is visible on all workspaces.
  base::Optional<bool> visible_on_all_workspaces;

  // The restored bounds in screen coordinates. Empty if the window is not
  // snapped/maximized/minimized.
  // TODO(sammiequon): This may not be needed as we save the restore bounds in
  // screen coordinates into |current_bounds|. On creating the widget,
  // |current_bounds| will be stored as restore bounds and the maximized or
  // snapped bounds will be determined by the system. Update the comment below
  // if this is removed.
  base::Optional<gfx::Rect> restore_bounds;

  // Current bounds in screen in coordinates. If the window has restore bounds,
  // then this contains the restore bounds.
  base::Optional<gfx::Rect> current_bounds;

  // Window state, minimized, maximized, inactive, etc.
  base::Optional<chromeos::WindowStateType> window_state_type;

  // Display id to launch an app.
  base::Optional<int64_t> display_id;

  std::string ToString() const;
};

}  // namespace full_restore

#endif  // COMPONENTS_FULL_RESTORE_WINDOW_INFO_H_
