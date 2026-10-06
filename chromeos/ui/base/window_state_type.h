// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_UI_BASE_WINDOW_STATE_TYPE_H_
#define CHROMEOS_UI_BASE_WINDOW_STATE_TYPE_H_

#include <ostream>

#include "base/component_export.h"
#include "ui/base/mojom/window_show_state.mojom-forward.h"
#include "ui/base/ui_base_types.h"

namespace chromeos {

// A superset of ui::mojom::WindowShowState. Ash has more states than the
// general ui::mojom::WindowShowState enum. These need to be communicated back
// to Chrome. The separate enum is defined here because we don't want to leak
// these type to ui/base until they're stable and we know for sure that they'll
// persist over time. Note this should be kept in sync with `WindowStateType`
// enum in tools/metrics/histograms/enums.xml.
enum class WindowStateType {
  // States which correspond to ui.mojom.ShowState.
  kDefault,
  kNormal,
  kMinimized,
  kMaximized,
  kInactive,
  kFullscreen,

  // Additional ash states.
  kPrimarySnapped,
  kSecondarySnapped,

  // A window is pinned on top of other windows with fullscreenized.
  // Corresponding shelf should be hidden, also most of windows other than the
  // pinned one should be hidden.
  kPinned,
  kTrustedPinned,

  // A window in Picture-in-Picture mode (PIP).
  kPip,

  // A window is floated on top of other windows (except PIP). When a window is
  // floated, users are allowed to change the position and size of the window.
  // One floated window is allowed per desk.
  kFloated,

  kMaxValue = kFloated,
};

COMPONENT_EXPORT(CHROMEOS_UI_BASE)
std::ostream& operator<<(std::ostream& stream, WindowStateType state);

// Utility functions to convert WindowStateType <-> ui::mojom::WindowShowState.
// Note: LEFT/RIGHT MAXIMIZED, AUTO_POSITIONED types will be lost when
// converting to ui::mojom::WindowShowState.
COMPONENT_EXPORT(CHROMEOS_UI_BASE)
WindowStateType ToWindowStateType(ui::mojom::WindowShowState state);
COMPONENT_EXPORT(CHROMEOS_UI_BASE)
ui::mojom::WindowShowState ToWindowShowState(WindowStateType type);

// Returns true if |type| is PINNED or TRUSTED_PINNED.
COMPONENT_EXPORT(CHROMEOS_UI_BASE)
bool IsPinnedWindowStateType(WindowStateType type);

// Returns true if |type| is FULLSCREEN, PINNED, or TRUSTED_PINNED.
COMPONENT_EXPORT(CHROMEOS_UI_BASE)
bool IsFullscreenOrPinnedWindowStateType(WindowStateType type);

// Returns true if |type| is MAXIMIZED, FULLSCREEN, PINNED, or TRUSTED_PINNED.
COMPONENT_EXPORT(CHROMEOS_UI_BASE)
bool IsMaximizedOrFullscreenOrPinnedWindowStateType(WindowStateType type);

// Returns true if `type` is MAXIMIZED or FULLSCREEN.
COMPONENT_EXPORT(CHROMEOS_UI_BASE)
bool IsMaximizedOrFullscreenWindowStateType(WindowStateType type);

// Returns true if |type| is MINIMIZED.
COMPONENT_EXPORT(CHROMEOS_UI_BASE)
bool IsMinimizedWindowStateType(WindowStateType type);

// Returns true if |type| is either NORMAL or DEFAULT.
COMPONENT_EXPORT(CHROMEOS_UI_BASE)
bool IsNormalWindowStateType(WindowStateType type);

// Returns true if `type` is either kPrimarySnapped or kSecondarySnapped.
COMPONENT_EXPORT(CHROMEOS_UI_BASE)
bool IsSnappedWindowStateType(WindowStateType type);

}  // namespace chromeos

#endif  // CHROMEOS_UI_BASE_WINDOW_STATE_TYPE_H_
