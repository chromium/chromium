// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_LACROS_WINDOW_UTILITY_H_
#define CHROME_BROWSER_UI_LACROS_WINDOW_UTILITY_H_

#include <string>

namespace aura {
class Window;
}  // namespace aura

// This file contains utility functions for Lacros-related windows.
namespace lacros_window_utility {

// Returns a unique id for the root window. This unique id is passed
// asynchronously to ash via Wayland during window construction. If |window| is
// not a root window, then this function automatically finds the root window.
std::string GetRootWindowUniqueId(aura::Window* window);

}  // namespace lacros_window_utility

#endif  // CHROME_BROWSER_UI_LACROS_WINDOW_UTILITY_H_
