// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SIDE_PANEL_READ_ANYTHING_READ_ANYTHING_SIDE_PANEL_CONTROLLER_UTILS_H_
#define CHROME_BROWSER_UI_SIDE_PANEL_READ_ANYTHING_READ_ANYTHING_SIDE_PANEL_CONTROLLER_UTILS_H_

#include "chrome/browser/ui/side_panel/read_anything/read_anything_tab_helper.h"
#include "chrome/browser/ui/side_panel/side_panel_enums.h"

class Browser;

// Used for reading mode option in context menu.
void ShowReadAnythingSidePanel(Browser* browser,
                               SidePanelOpenTrigger open_trigger);
bool IsReadAnythingEntryShowing(Browser* browser);

// Create a ReadAnythingTabHelper::Delegate for a given WebContents.
std::unique_ptr<ReadAnythingTabHelper::Delegate> CreateDelegate(
    content::WebContents* web_contents);

#endif  // CHROME_BROWSER_UI_SIDE_PANEL_READ_ANYTHING_READ_ANYTHING_SIDE_PANEL_CONTROLLER_UTILS_H_
