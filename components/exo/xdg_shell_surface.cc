// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/xdg_shell_surface.h"

#include "ash/frame/non_client_frame_view_ash.h"
#include "chromeos/ui/base/window_properties.h"
#include "ui/base/mojom/window_show_state.mojom.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/window/caption_button_layout_constants.h"

namespace exo {

////////////////////////////////////////////////////////////////////////////////
// XdgShellSurface, public:

XdgShellSurface::XdgShellSurface(Surface* surface,
                                 const gfx::Point& origin,
                                 bool can_minimize,
                                 int container)
    : ShellSurface(surface, origin, can_minimize, container) {}

XdgShellSurface::~XdgShellSurface() = default;

void XdgShellSurface::OverrideInitParams(views::Widget::InitParams* params) {
  DCHECK(params);

  // Auto-maximize can override the initial show_state, if it's enabled via
  // window property.
  bool auto_maximize_enabled = params->init_properties_container.GetProperty(
      chromeos::kAutoMaximizeXdgShellEnabled);
  if (auto_maximize_enabled && ShouldAutoMaximize()) {
    params->show_state = ui::mojom::WindowShowState::kMaximized;
  }
  if (!frame_enabled() && !has_frame_colors()) {
    params->layer_type = ui::LAYER_NOT_DRAWN;
  }
}

bool XdgShellSurface::ShouldAutoMaximize() {
  if (initial_show_state() != ui::mojom::WindowShowState::kDefault ||
      is_popup_ || !CanMaximize()) {
    return false;
  }

  DCHECK(!widget_);
  gfx::Size work_area_size = display::Screen::GetScreen()
                                 ->GetDisplayNearestWindow(host_window())
                                 .work_area_size();
  DCHECK(!work_area_size.IsEmpty());

  gfx::Rect window_bounds = GetVisibleBounds();
  // This way to predict the size of the widget if it were maximized is brittle.
  // We rely on unit tests to guard against changes in the size of the window
  // decorations.
  if (frame_enabled()) {
    window_bounds.Inset(gfx::Insets().set_bottom(
        -views::GetCaptionButtonLayoutSize(
             views::CaptionButtonLayoutSize::kNonBrowserCaption)
             .height()));
  }
  return window_bounds.width() >= work_area_size.width() &&
         window_bounds.height() >= work_area_size.height();
}

}  // namespace exo
