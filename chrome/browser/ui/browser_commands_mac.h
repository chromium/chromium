// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_BROWSER_COMMANDS_MAC_H_
#define CHROME_BROWSER_UI_BROWSER_COMMANDS_MAC_H_

class Browser;

namespace chrome {

// Toggles the "Always Show Toolbar in Full Screen".
void ToggleAlwaysShowToolbarInFullscreen(Browser* browser);

// Sets the "Always Show Toolbar in Full Screen" in tests.
void SetAlwaysShowToolbarInFullscreenForTesting(Browser* browser,
                                                bool always_show);

// Toggles the "Allow JavaScript from AppleEvents" setting.
void ToggleJavaScriptFromAppleEventsAllowed(Browser* browser);

}  // namespace chrome

#endif  // CHROME_BROWSER_UI_BROWSER_COMMANDS_MAC_H_
