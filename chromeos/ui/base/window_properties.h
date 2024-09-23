// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_UI_BASE_WINDOW_PROPERTIES_H_
#define CHROMEOS_UI_BASE_WINDOW_PROPERTIES_H_

#include <string>

#include "base/component_export.h"
#include "chromeos/ui/base/app_types.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/class_property.h"

namespace gfx {
class Rect;
}

namespace chromeos {

enum class WindowStateType;
enum class WindowPinType;

// Shell-specific window property keys for use by ash and lacros clients.

// Alphabetical sort.

// A property key to store the type of window that will be used to record
// pointer metrics. See AppType in chromeos/ui/base/app_types.h for more
// details.
COMPONENT_EXPORT(CHROMEOS_UI_BASE)
extern const ui::ClassProperty<AppType>* const kAppTypeKey;

// Whether resizable windows equal to or larger than the screen should be
// automatically maximized. Affects Exo's xdg-shell clients only.
COMPONENT_EXPORT(CHROMEOS_UI_BASE)
extern const ui::ClassProperty<bool>* const kAutoMaximizeXdgShellEnabled;

// If set to true, the window will be replaced by a black rectangle when taking
// screenshot for assistant. Used to preserve privacy for incognito windows.
COMPONENT_EXPORT(CHROMEOS_UI_BASE)
extern const ui::ClassProperty<bool>* const kBlockedForAssistantSnapshotKey;

// Whether holding esc should exit fullscreen. Used by Plugin VM.
COMPONENT_EXPORT(CHROMEOS_UI_BASE)
extern const ui::ClassProperty<bool>* const kEscHoldToExitFullscreen;

// Do not exit fullscreen on a screen lock. Note that this property becomes
// active only if `kUseOverviewToExitFullscreen` is true. Borealis apps set this
// to avoid exiting fullscreen on a screen lock.
// Do NOT use this property without consulting the security team for other use
// cases.
COMPONENT_EXPORT(CHROMEOS_UI_BASE)
extern const ui::ClassProperty<bool>* const kNoExitFullscreenOnLock;

// Whether to promote users to use Overview to exit fullscreen.
// Borealis apps set this since they do not handle window size changes.
COMPONENT_EXPORT(CHROMEOS_UI_BASE)
extern const ui::ClassProperty<bool>* const kUseOverviewToExitFullscreen;

// If true, Exo clients may request pointer lock for this window.
// When the lock activates, users will be notified to use Overview to exit
// pointer lock.
// Only ARC++ and Lacros may use pointer lock without this property being set.
COMPONENT_EXPORT(CHROMEOS_UI_BASE)
extern const ui::ClassProperty<bool>* const kUseOverviewToExitPointerLock;

// True if clients expect the window to track the system's default frame colors.
// This is used to determine whether a frame's color should be kept in sync with
// default colors during system theme transitions, or if frame colors should be
// left unmodified (e.g. system app custom frame colors).
COMPONENT_EXPORT(CHROMEOS_UI_BASE)
extern const ui::ClassProperty<bool>* const kTrackDefaultFrameColors;

// A property key to store the active color on the window frame.
// `kTrackDefaultFrameColors` must be set to false for this to take effect.
COMPONENT_EXPORT(CHROMEOS_UI_BASE)
extern const ui::ClassProperty<SkColor>* const kFrameActiveColorKey;

// A property key to store the inactive color on the window frame.
// `kTrackDefaultFrameColors` must be set to false for this to take effect.
COMPONENT_EXPORT(CHROMEOS_UI_BASE)
extern const ui::ClassProperty<SkColor>* const kFrameInactiveColorKey;

// A property key that is set to true when the window frame should look like it
// is in restored state, but actually isn't. Set while dragging a maximized
// window.
COMPONENT_EXPORT(CHROMEOS_UI_BASE)
extern const ui::ClassProperty<bool>* const kFrameRestoreLookKey;

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

// A property key to indicate if the window is a game.
COMPONENT_EXPORT(CHROMEOS_UI_BASE)
extern const ui::ClassProperty<bool>* const kIsGameKey;

// If true, the window is currently showing in overview mode.
COMPONENT_EXPORT(CHROMEOS_UI_BASE)
extern const ui::ClassProperty<bool>* const kIsShowingInOverviewKey;

// A property to indicate if a window should have a highlight border overlay.
COMPONENT_EXPORT(CHROMEOS_UI_BASE)
extern const ui::ClassProperty<bool>* const kShouldHaveHighlightBorderOverlay;

// A property key to indicate if the window supports
// `WindowStateType::kFloated`. Even if true, it doesn't always mean we _can_
// float the window. See `chromeos::wm::CanFloatWindow` for details. True by
// default.
COMPONENT_EXPORT(CHROMEOS_UI_BASE)
extern const ui::ClassProperty<bool>* const kSupportsFloatedStateKey;

// A property key to tell if the window's opacity should be managed by WM.
COMPONENT_EXPORT(CHROMEOS_UI_BASE)
extern const ui::ClassProperty<bool>* const kWindowManagerManagesOpacityKey;

// A property key to indicate ash's extended window state.
COMPONENT_EXPORT(CHROMEOS_UI_BASE)
extern const ui::ClassProperty<WindowStateType>* const kWindowStateTypeKey;

// A property key whose value is shown in alt-tab/overview mode. If non-value
// is set, the window's title is used.
COMPONENT_EXPORT(CHROMEOS_UI_BASE)
extern const ui::ClassProperty<std::u16string*>* const kWindowOverviewTitleKey;

// Alphabetical sort.

}  // namespace chromeos

#endif  // CHROMEOS_UI_BASE_WINDOW_PROPERTIES_H_
