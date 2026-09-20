// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_EVERYWHERE_MAC_WINDOW_UTIL_H_
#define CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_EVERYWHERE_MAC_WINDOW_UTIL_H_

#include "build/build_config.h"
#include "ui/gfx/native_ui_types.h"

class BrowserWindowInterface;

namespace omnibox_everywhere {

// TODO (b/562543594): Revisit the need for these helper methods if there
// is a better approach determined to solve the space switching bugs in
// fullscreen mode on Mac.
#if BUILDFLAG(IS_MAC)
// Clears auxiliary Space associations for the OmniboxEverywhere popup on
// dismiss so that macOS Window Server does not consider Chrome active on the
// auxiliary Space.
void DisassociatePopupOnMac(gfx::NativeWindow native_window);

// Activates the browser window and ensures macOS switches spaces to Chrome.
void ActivateBrowserWindowOnMac(BrowserWindowInterface* bwi);
#endif

}  // namespace omnibox_everywhere

#endif  // CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_EVERYWHERE_MAC_WINDOW_UTIL_H_
