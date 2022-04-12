// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_UI_WM_WINDOW_UTIL_H_
#define CHROMEOS_UI_WM_WINDOW_UTIL_H_

#include "base/component_export.h"

namespace aura {
class Window;
}

namespace chromeos {

// Toggles the float state of `window`.
COMPONENT_EXPORT(CHROMEOS_UI_BASE)
void ToggleFloating(aura::Window* window);

}  // namespace chromeos

#endif  // CHROMEOS_UI_WM_WINDOW_UTIL_H_
