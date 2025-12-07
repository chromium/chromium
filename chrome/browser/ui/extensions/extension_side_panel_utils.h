// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_EXTENSIONS_EXTENSION_SIDE_PANEL_UTILS_H_
#define CHROME_BROWSER_UI_EXTENSIONS_EXTENSION_SIDE_PANEL_UTILS_H_

#include <optional>

#include "extensions/common/extension_id.h"

class BrowserWindowInterface;

namespace content {
class WebContents;
}  // namespace content

namespace extensions::side_panel_util {

// Toggles the side panel for the given `extension_id` in `browser_window`,
// opening the panel if it is closed and closing it if it is open. Implemented
// by extension_side_panel_utils.cc in views/.
void ToggleExtensionSidePanel(BrowserWindowInterface* browser_window,
                              const ExtensionId& extension_id);

// Opens the global side panel for the given `extension_id` in `browser_window`.
// If `web_contents` is specified, this will close any active side panel in
// `web_contents`; otherwise, this will not override any contextual side panels.
// This may not immediately show the side panel if `web_contents` is not the
// active tab and the active tab has an open contextual panel. No-op (and safe
// to call) if the panel is already open.
// Implemented by extension_side_panel_utils.cc in views/.
void OpenGlobalExtensionSidePanel(BrowserWindowInterface& browser_window,
                                  content::WebContents* web_contents,
                                  const ExtensionId& extension_id);

// Opens a contextual side panel for the given `extension_id` in
// `browser_window` for `web_contents`. If `web_contents` is not the active tab,
// this will set the panel for that tab, but will not open the side panel until
// that tab is activated. Implemented by extension_side_panel_utils.cc in
// views/.
void OpenContextualExtensionSidePanel(BrowserWindowInterface& browser_window,
                                      content::WebContents& web_contents,
                                      const ExtensionId& extension_id);

// Closes the global side panel for the given `extension_id` in
// `browser_window`. This will close the global side panel across all tabs where
// no contextual panel is active. No-op (and safe to call) if the panel is
// already closed. This function provides a programmatic way to close global
// side panels without user interaction. Implemented by
// extension_side_panel_utils.cc in views/.
void CloseGlobalExtensionSidePanel(BrowserWindowInterface* browser_window,
                                   const ExtensionId& extension_id);

// Closes the contextual side panel for the specified `extension_id` in
// `browser_window` associated with `web_contents`. If no contextual panel
// exists on this tab and `window_id` is not `nullopt`, checks for a global side
// panel for the same extension in `browser_window` and closes it across all
// tabs if found. No-op (and safe to call) if the panel is already closed.
// Implemented in extension_side_panel_utils.cc in views/.
void CloseContextualExtensionSidePanel(BrowserWindowInterface* browser_window,
                                       content::WebContents* web_contents,
                                       const ExtensionId& extension_id,
                                       std::optional<int> window_id);

}  // namespace extensions::side_panel_util

#endif  // CHROME_BROWSER_UI_EXTENSIONS_EXTENSION_SIDE_PANEL_UTILS_H_
