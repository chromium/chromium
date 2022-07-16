// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_APP_RESTORE_WINDOW_INFO_H_
#define COMPONENTS_APP_RESTORE_WINDOW_INFO_H_

#include "chromeos/ui/base/window_state_type.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/aura/window.h"
#include "ui/base/ui_base_types.h"
#include "ui/gfx/geometry/rect.h"

namespace app_restore {

// This struct is the parameter for the interface SaveWindowInfo, to save the
// window information.
struct COMPONENT_EXPORT(APP_RESTORE) WindowInfo {
 public:
  // This struct is the ARC specific window info.
  struct ArcExtraInfo {
    ArcExtraInfo();
    ArcExtraInfo(const WindowInfo::ArcExtraInfo&);
    ArcExtraInfo& operator=(const WindowInfo::ArcExtraInfo&);
    ~ArcExtraInfo();

    absl::optional<gfx::Size> maximum_size;
    absl::optional<gfx::Size> minimum_size;
    absl::optional<std::u16string> title;
    absl::optional<gfx::Rect> bounds_in_root;
  };

  WindowInfo();
  WindowInfo(const WindowInfo&) = delete;
  WindowInfo& operator=(const WindowInfo&) = delete;
  ~WindowInfo();

  WindowInfo* Clone();

  aura::Window* window;

  // Index in MruWindowTracker to restore window stack. A lower index
  // indicates a more recently used window.
  absl::optional<int32_t> activation_index;

  // Virtual desk id.
  absl::optional<int32_t> desk_id;

  // Current bounds in screen in coordinates. If the window has restore bounds,
  // then this contains the restore bounds.
  absl::optional<gfx::Rect> current_bounds;

  // Window state, minimized, maximized, inactive, etc.
  absl::optional<chromeos::WindowStateType> window_state_type;

  // Show state of a window before it was minimized. Empty for non-minimized
  // windows.
  absl::optional<ui::WindowShowState> pre_minimized_show_state_type;

  // Display id to launch an app.
  absl::optional<int64_t> display_id;

  // Extra window info of ARC app window.
  absl::optional<ArcExtraInfo> arc_extra_info;

  std::string ToString() const;
};

}  // namespace app_restore

#endif  // COMPONENTS_APP_RESTORE_WINDOW_INFO_H_
