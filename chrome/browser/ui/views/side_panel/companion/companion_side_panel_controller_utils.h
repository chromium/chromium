// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_COMPANION_COMPANION_SIDE_PANEL_CONTROLLER_UTILS_H_
#define CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_COMPANION_COMPANION_SIDE_PANEL_CONTROLLER_UTILS_H_

#include "chrome/browser/ui/views/side_panel/companion/companion_tab_helper.h"

class Browser;

namespace companion {

// Creates a delegate implemented in c/b/ui/views to aid integration with Views
// API. This is only called on desktop platforms.
std::unique_ptr<CompanionTabHelper::Delegate> CreateDelegate(
    content::WebContents* web_contents);

// A helper function to get a Browser object from a WebContents.
// TODO(b/276490341): Move this function to browser_finder.
Browser* GetBrowserForWebContents(content::WebContents* web_contents);

}  // namespace companion

#endif  // CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_COMPANION_COMPANION_SIDE_PANEL_CONTROLLER_UTILS_H_
