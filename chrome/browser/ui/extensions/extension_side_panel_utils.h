// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_EXTENSIONS_EXTENSION_SIDE_PANEL_UTILS_H_
#define CHROME_BROWSER_UI_EXTENSIONS_EXTENSION_SIDE_PANEL_UTILS_H_

#include "extensions/common/extension_id.h"

class Browser;
class Profile;

namespace content {
class WebContents;
}  // namespace content

namespace extensions::side_panel_util {

// Implemented by extension_side_panel_utils.cc in views/.
void CreateSidePanelManagerForWebContents(Profile* profile,
                                          content::WebContents* web_contents);

// Toggles the side panel for the given `extension_id` in `browser`, opening
// the panel if it is closed and closing it if it is open.
// Implemented by extension_side_panel_utils.cc in views/.
void ToggleExtensionSidePanel(Browser* browser,
                              const ExtensionId& extension_id);

// Opens the side panel for the given `extension_id` in `browser`. No-op (and
// safe to call) if the panel is already open.
// Implemented by extension_side_panel_utils.cc in views/.
void OpenExtensionSidePanel(Browser& browser, const ExtensionId& extension_id);

}  // namespace extensions::side_panel_util

#endif  // CHROME_BROWSER_UI_EXTENSIONS_EXTENSION_SIDE_PANEL_UTILS_H_
