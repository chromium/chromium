// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_UI_WM_WINDOW_UTIL_H_
#define CHROMEOS_UI_WM_WINDOW_UTIL_H_

#include "base/component_export.h"
#include "chromeos/ui/base/window_properties.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

namespace aura {
class Window;
}

namespace chromeos::wm {

// Installs a resize handler on the window that makes it easier to resize
// the window.
COMPONENT_EXPORT(CHROMEOS_UI_WM)
void InstallResizeHandleWindowTargeterForWindow(
    aura::Window* window,
    chromeos::ResizeBorderInsets border_insets =
        chromeos::ResizeBorderInsets());

// Returns whether the display nearest `window` is in landscape orientation.
COMPONENT_EXPORT(CHROMEOS_UI_WM)
bool IsLandscapeOrientationForWindow(aura::Window* window);

// Returns the size for a `window` in tablet mode. Returns an empty size if the
// window cannot be floated.
COMPONENT_EXPORT(CHROMEOS_UI_WM)
gfx::Size GetFloatedWindowTabletSize(aura::Window* window);

// Checks whether a `window` can be floated.
COMPONENT_EXPORT(CHROMEOS_UI_WM) bool CanFloatWindow(aura::Window* window);

}  // namespace chromeos::wm

#endif  // CHROMEOS_UI_WM_WINDOW_UTIL_H_
