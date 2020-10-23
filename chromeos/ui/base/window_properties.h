// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_UI_BASE_WINDOW_PROPERTIES_H_
#define CHROMEOS_UI_BASE_WINDOW_PROPERTIES_H_

#include "base/component_export.h"
#include "ui/base/class_property.h"

namespace gfx {
class Rect;
}

namespace chromeos {

enum class WindowStateType;
enum class WindowPinType;

// Shell-specific window property keys for use by ash and lacros clients.

// Alphabetical sort.

// Whether the shelf should be hidden when this window is put into fullscreen.
// Exposed because some windows want to explicitly opt-out of this.
COMPONENT_EXPORT(CHROMEOS_UI_BASE)
extern const ui::ClassProperty<bool>* const kHideShelfWhenFullscreenKey;

// Whether entering fullscreen means that a window should automatically enter
// immersive mode. This is false for some client windows, such as Chrome Apps.
COMPONENT_EXPORT(CHROMEOS_UI_BASE)
extern const ui::ClassProperty<bool>* const kImmersiveImpliedByFullscreen;

// Whether immersive is currently active (in ImmersiveFullscreenController
// parlance, "enabled").
COMPONENT_EXPORT(CHROMEOS_UI_BASE)
extern const ui::ClassProperty<bool>* const kImmersiveIsActive;

// The bounds of the top container in screen coordinates.
COMPONENT_EXPORT(CHROMEOS_UI_BASE)
extern const ui::ClassProperty<gfx::Rect*>* const
    kImmersiveTopContainerBoundsInScreen;

// If true, the window is currently showing in overview mode.
COMPONENT_EXPORT(CHROMEOS_UI_BASE)
extern const ui::ClassProperty<bool>* const kIsShowingInOverviewKey;

// A property key to indicate ash's extended window state.
COMPONENT_EXPORT(CHROMEOS_UI_BASE)
extern const ui::ClassProperty<WindowStateType>* const kWindowStateTypeKey;

// A property key to store ash::WindowPinType for a window.
// When setting this property to PINNED or TRUSTED_PINNED, the window manager
// will try to fullscreen the window and pin it on the top of the screen. If the
// window manager failed to do it, the property will be restored to NONE. When
// setting this property to NONE, the window manager will restore the window.
COMPONENT_EXPORT(CHROMEOS_UI_BASE)
extern const ui::ClassProperty<WindowPinType>* const kWindowPinTypeKey;

// Alphabetical sort.

}  // namespace chromeos

#endif  // CHROMEOS_UI_BASE_WINDOW_PROPERTIES_H_
