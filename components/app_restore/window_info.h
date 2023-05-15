// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_APP_RESTORE_WINDOW_INFO_H_
#define COMPONENTS_APP_RESTORE_WINDOW_INFO_H_

#include "base/memory/raw_ptr.h"
#include "base/uuid.h"
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
    absl::optional<gfx::Rect> bounds_in_root;
  };

  WindowInfo();
  WindowInfo(const WindowInfo&) = delete;
  WindowInfo& operator=(const WindowInfo&) = delete;
  ~WindowInfo();

  WindowInfo* Clone();

  raw_ptr<aura::Window, DanglingUntriaged | ExperimentalAsh> window;

  // Index in MruWindowTracker to restore window stack. A lower index
  // indicates a more recently used window.
  absl::optional<int32_t> activation_index;

  // Virtual desk id.
  absl::optional<int32_t> desk_id;

  // Virtual desk guid.
  base::Uuid desk_guid;

  // Current bounds in screen in coordinates. If the window has restore bounds,
  // then this contains the restore bounds.
  absl::optional<gfx::Rect> current_bounds;

  // Window state, minimized, maximized, inactive, etc.
  absl::optional<chromeos::WindowStateType> window_state_type;

  // Show state of a window before it was minimized. Empty for non-minimized
  // windows.
  absl::optional<ui::WindowShowState> pre_minimized_show_state_type;

  // The snap percentage of a window, if it is snapped. For instance a snap
  // percentage of 75 means the window takes up three quarters of the work area.
  // The primary axis is determined when restoring; if it is portrait, it will
  // be three quarters of the height.
  absl::optional<uint32_t> snap_percentage;

  // Display id to launch an app.
  absl::optional<int64_t> display_id;

  // The title of the app window. Used for saved desks in case one of the
  // windows in the template is uninstalled, we can show a nice error message.
  // Also used for the ARC ghost window.
  absl::optional<std::u16string> app_title;

  // Extra window info of ARC app window.
  absl::optional<ArcExtraInfo> arc_extra_info;

  std::string ToString() const;
};

}  // namespace app_restore

#endif  // COMPONENTS_APP_RESTORE_WINDOW_INFO_H_
