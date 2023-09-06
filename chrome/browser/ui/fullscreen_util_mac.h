// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_FULLSCREEN_UTIL_MAC_H_
#define CHROME_BROWSER_UI_FULLSCREEN_UTIL_MAC_H_

class Browser;

namespace fullscreen_utils {

// Returns true iff:
// - `browser` is currently in fullscreen
// - the fullscreen mode is web or extension API initiated (as opposed to via
//   macOS affordances like traffic lights
bool IsInContentFullscreen(Browser* browser);

// Whether the "Always Show Toolbar in Full Screen" setting is enabled. Properly
// handles PWAs.
bool IsAlwaysShowToolbarEnabled(const Browser* browser);

}  // namespace fullscreen_utils

#endif  // CHROME_BROWSER_UI_FULLSCREEN_UTIL_MAC_H_
