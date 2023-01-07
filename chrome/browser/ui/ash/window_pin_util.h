// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_WINDOW_PIN_UTIL_H_
#define CHROME_BROWSER_UI_ASH_WINDOW_PIN_UTIL_H_

#include "chromeos/ui/base/window_pin_type.h"

namespace aura {
class Window;
}

// Sets the window state to pinned.  If |trusted| is true, sets to
// kTrustedPinned.  Otherwise, the window is set to kPinned.
void PinWindow(aura::Window* window, bool trusted);

// Revert the window state to default from a pinned state.
void UnpinWindow(aura::Window* window);

// Returns the pinned state for a window.
chromeos::WindowPinType GetWindowPinType(const aura::Window* window);

// Returns true if the window is Pinned or TrustedPinned.
bool IsWindowPinned(const aura::Window* window);

#endif  // CHROME_BROWSER_UI_ASH_WINDOW_PIN_UTIL_H_
