// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXO_XDG_SHELL_SURFACE_H_
#define COMPONENTS_EXO_XDG_SHELL_SURFACE_H_

#include <cstdint>
#include <memory>
#include <string>

#include "ash/display/window_tree_host_manager.h"
#include "ash/wm/window_state_observer.h"
#include "base/containers/circular_deque.h"
#include "base/macros.h"
#include "base/optional.h"
#include "base/strings/string16.h"
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
                  bool activatable,
                  bool can_minimize,
                  int container);
  ~XdgShellSurface() override;

  // Xdg surfaces have the behaviour that they should maximize themselves if
  // their bounds are larger or equal to the display area. This behaviour is
  // implemented in linux display managers (e.g. Muffin/Cinnamon).
  bool ShouldAutoMaximize() override;

  bool x_flipped() const { return x_flipped_; }
  void set_x_flipped(bool flipped) { x_flipped_ = flipped; }

  bool y_flipped() const { return y_flipped_; }
  void set_y_flipped(bool flipped) { y_flipped_ = flipped; }

 private:
  // Used by positioner to layout cascading menus in opposite
  // direction when the layout does not fit to the work area.
  bool y_flipped_ = false;
  bool x_flipped_ = false;

  DISALLOW_COPY_AND_ASSIGN(XdgShellSurface);
};

}  // namespace exo

#endif  // COMPONENTS_EXO_XDG_SHELL_SURFACE_H_
