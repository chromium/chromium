// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/xdg_shell_surface.h"

#include "ash/frame/non_client_frame_view_ash.h"
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
                                 bool activatable,
                                 bool can_minimize,
                                 int container)
    : ShellSurface(surface, origin, activatable, can_minimize, container) {}

XdgShellSurface::~XdgShellSurface() {}

bool XdgShellSurface::ShouldAutoMaximize() {
  if (initial_show_state() != ui::SHOW_STATE_DEFAULT || is_popup_ ||
      !CanMaximize())
    return false;

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
    window_bounds.Inset(0, 0, 0,
                        -views::GetCaptionButtonLayoutSize(
                             views::CaptionButtonLayoutSize::kNonBrowserCaption)
                             .height());
  }
  return window_bounds.width() >= work_area_size.width() &&
         window_bounds.height() >= work_area_size.height();
}

}  // namespace exo
