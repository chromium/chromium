// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/lacros/float_controller_lacros.h"

#include "ui/aura/window.h"
#include "ui/aura/window_tree_host_platform.h"
#include "ui/platform_window/extensions/wayland_extension.h"

namespace {

ui::WaylandToplevelExtension* GetWaylandToplevelExtension(
    aura::Window* window) {
  auto* wth_platform = aura::WindowTreeHostPlatform::GetHostForWindow(window);
  auto* wayland_extension =
      ui::GetWaylandToplevelExtension(*wth_platform->platform_window());
  DCHECK(wayland_extension);
  return wayland_extension;
}

}  // namespace

FloatControllerLacros::FloatControllerLacros() = default;

FloatControllerLacros::~FloatControllerLacros() = default;

void FloatControllerLacros::SetFloat(
    aura::Window* window,
    chromeos::FloatStartLocation float_start_location) {
  switch (float_start_location) {
    case chromeos::FloatStartLocation::kBottomRight:
      GetWaylandToplevelExtension(window)->SetFloatToLocation(
          ui::WaylandFloatStartLocation::kBottomRight);
      break;
    case chromeos::FloatStartLocation::kBottomLeft:
      GetWaylandToplevelExtension(window)->SetFloatToLocation(
          ui::WaylandFloatStartLocation::kBottomLeft);
      break;
  }
}

void FloatControllerLacros::UnsetFloat(aura::Window* window) {
  GetWaylandToplevelExtension(window)->UnSetFloat();
}
