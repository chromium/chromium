// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXO_XDG_SHELL_SURFACE_H_
#define COMPONENTS_EXO_XDG_SHELL_SURFACE_H_

#include <cstdint>

#include "ash/display/window_tree_host_manager.h"
#include "ash/wm/window_state_observer.h"
#include "components/exo/shell_surface.h"
#include "components/exo/surface_observer.h"
#include "components/exo/surface_tree_host.h"

#include "ui/aura/window_observer.h"
#include "ui/base/hit_test.h"
#include "ui/compositor/compositor_lock.h"
#include "ui/display/display_observer.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/vector2d.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/wm/public/activation_change_observer.h"

namespace ash {
namespace mojom {
enum class WindowPinType;
}
}  // namespace ash

namespace exo {
class Surface;

// This class implements shell surface for XDG protocol.
class XdgShellSurface : public ShellSurface {
 public:
  // The |origin| is the initial position in screen coordinates. The position
  // specified as part of the geometry is relative to the shell surface.
  XdgShellSurface(Surface* surface,
                  const gfx::Point& origin,
                  bool can_minimize,
                  int container);

  XdgShellSurface(const XdgShellSurface&) = delete;
  XdgShellSurface& operator=(const XdgShellSurface&) = delete;

  ~XdgShellSurface() override;

  // ShellSurfaceBase::
  void OverrideInitParams(views::Widget::InitParams* params) override;

 private:
  // Xdg surfaces have the behaviour that they should maximize themselves if
  // their bounds are larger or equal to the display area. This behaviour is
  // implemented in linux display managers (e.g. Muffin/Cinnamon).
  bool ShouldAutoMaximize();
};

}  // namespace exo

#endif  // COMPONENTS_EXO_XDG_SHELL_SURFACE_H_
