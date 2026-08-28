// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/extensions/extension_side_panel_utils.h"

#include "base/notimplemented.h"

namespace extensions::side_panel_util {

void ToggleExtensionSidePanel(BrowserWindowInterface* browser_window,
                              const ExtensionId& extension_id) {
  NOTIMPLEMENTED();
}

void OpenGlobalExtensionSidePanel(BrowserWindowInterface& browser_window,
                                  content::WebContents* web_contents,
                                  const ExtensionId& extension_id) {
  NOTIMPLEMENTED();
}

void OpenContextualExtensionSidePanel(BrowserWindowInterface& browser_window,
                                      content::WebContents& web_contents,
                                      const ExtensionId& extension_id) {
  NOTIMPLEMENTED();
}

void CloseGlobalExtensionSidePanel(BrowserWindowInterface* browser_window,
                                   const ExtensionId& extension_id) {
  NOTIMPLEMENTED();
}

void CloseContextualExtensionSidePanel(BrowserWindowInterface* browser_window,
                                       content::WebContents* web_contents,
                                       const ExtensionId& extension_id) {
  NOTIMPLEMENTED();
}

}  // namespace extensions::side_panel_util
