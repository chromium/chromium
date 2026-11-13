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

// Activates Chrome without bringing all browser windows forward and orders the
// OmniboxEverywhere popup window key and front so macOS CharacterPalette.app
// (Emoji & Symbols) attaches to the popup without resigning key window status.
void ActivatePopupForEmojiPanelOnMac(gfx::NativeWindow native_window);

// Returns true when the active AppKit event is a non-control left mouse down so
// WebUI elements inside `-webkit-app-region: drag` containers still receive
// hover and non-drag events on macOS.
bool ShouldTreatCurrentEventAsDraggableOnMac();
#endif

}  // namespace omnibox_everywhere

#endif  // CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_EVERYWHERE_MAC_WINDOW_UTIL_H_
