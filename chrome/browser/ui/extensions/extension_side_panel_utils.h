// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_EXTENSIONS_EXTENSION_SIDE_PANEL_UTILS_H_
#define CHROME_BROWSER_UI_EXTENSIONS_EXTENSION_SIDE_PANEL_UTILS_H_

#include "extensions/common/extension_id.h"

class Browser;

namespace content {
class WebContents;
}  // namespace content

namespace extensions::side_panel_util {

// Toggles the side panel for the given `extension_id` in `browser`, opening
// the panel if it is closed and closing it if it is open.
// Implemented by extension_side_panel_utils.cc in views/.
void ToggleExtensionSidePanel(Browser* browser,
                              const ExtensionId& extension_id);

// Opens the global side panel for the given `extension_id` in `browser`.
// If `web_contents` is specified, this will close any active side panel in
// `web_contents`; otherwise, this will not override any contextual side panels.
// This may not immediately show the side panel if `web_contents` is not the
// active tab and the active tab has an open contextual panel. No-op (and safe
// to call) if the panel is already open.
// Implemented by extension_side_panel_utils.cc in views/.
void OpenGlobalExtensionSidePanel(Browser& browser,
                                  content::WebContents* web_contents,
                                  const ExtensionId& extension_id);

// Opens a contextual side panel for the given `extension_id` in `browser` for
// `web_contents`. If `web_contents` is not the active tab, this will set the
// panel for that tab, but will not open the side panel until that tab is
// activated.
// Implemented by extension_side_panel_utils.cc in views/.
void OpenContextualExtensionSidePanel(Browser& browser,
                                      content::WebContents& web_contents,
                                      const ExtensionId& extension_id);

}  // namespace extensions::side_panel_util

#endif  // CHROME_BROWSER_UI_EXTENSIONS_EXTENSION_SIDE_PANEL_UTILS_H_
