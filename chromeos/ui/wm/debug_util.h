// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_UI_WM_DEBUG_UTIL_H_
#define CHROMEOS_UI_WM_DEBUG_UTIL_H_

#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

#include "base/component_export.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "ui/aura/window.h"

namespace chromeos::wm {

using GetChildrenCallback = base::RepeatingCallback<
    std::vector<raw_ptr<aura::Window, VectorExperimental>>(aura::Window*)>;

// Prints all windows hierarchy to `out`. If `scrub_data` is true, we
// may skip some data fields that are not very important for debugging. Returns
// a list of window titles. Window titles will be removed from `out` if
// `scrub_data` is true. `children_callback` can be provided to customize how
// child windows are structured under a window.
COMPONENT_EXPORT(CHROMEOS_UI_BASE)
std::vector<std::string> PrintWindowHierarchy(
    aura::Window::Windows roots,
    bool scrub_data,
    std::ostringstream* out,
    GetChildrenCallback children_callback = GetChildrenCallback());

}  // namespace chromeos::wm

#endif  // CHROMEOS_UI_WM_DEBUG_UTIL_H_
