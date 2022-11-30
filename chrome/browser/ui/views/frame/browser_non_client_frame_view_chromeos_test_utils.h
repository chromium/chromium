// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_NON_CLIENT_FRAME_VIEW_CHROMEOS_TEST_UTILS_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_NON_CLIENT_FRAME_VIEW_CHROMEOS_TEST_UTILS_H_

#include "build/chromeos_buildflags.h"

class Browser;
class BrowserView;
class BrowserNonClientFrameViewChromeOS;

namespace content {
class WebContents;
}

// Toggles fullscreen mode and waits for the notification.
void ToggleFullscreenModeAndWait(Browser* browser);

// Enters fullscreen mode for tab and waits for the notification.
void EnterFullscreenModeForTabAndWait(Browser* browser,
                                      content::WebContents* web_contents);

// Exits fullscreen mode for tab and waits for the notification.
void ExitFullscreenModeForTabAndWait(Browser* browser,
                                     content::WebContents* web_contents);

// Returns the non client frame view for |browser_view|.
BrowserNonClientFrameViewChromeOS* GetFrameViewChromeOS(
    BrowserView* browser_view);

#if BUILDFLAG(IS_CHROMEOS_ASH)
// Starts overview session which displays an overview of all windows.
void StartOverview();

// Ends overview session.
void EndOverview();

// Returns true if the shelf is visible (e.g. not auto-hidden).
bool IsShelfVisible();
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_NON_CLIENT_FRAME_VIEW_CHROMEOS_TEST_UTILS_H_
