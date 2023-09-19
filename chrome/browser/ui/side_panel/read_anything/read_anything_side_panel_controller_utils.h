// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SIDE_PANEL_READ_ANYTHING_READ_ANYTHING_SIDE_PANEL_CONTROLLER_UTILS_H_
#define CHROME_BROWSER_UI_SIDE_PANEL_READ_ANYTHING_READ_ANYTHING_SIDE_PANEL_CONTROLLER_UTILS_H_

class Browser;

namespace content {
class WebContents;
}  // namespace content

// Used for reading mode option in context menu.
void ShowReadAnythingSidePanel(Browser* browser);
bool IsReadAnythingEntryShowing(Browser* browser);
// Create and register a read anything side panel entry for the given web
// contents.
void CreateAndRegisterReadAnythingEntry(content::WebContents* web_contents);

#endif  // CHROME_BROWSER_UI_SIDE_PANEL_READ_ANYTHING_READ_ANYTHING_SIDE_PANEL_CONTROLLER_UTILS_H_
