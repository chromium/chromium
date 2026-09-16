// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/omnibox/omnibox_everywhere/mac_window_util.h"

#import <AppKit/AppKit.h>

#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "ui/base/base_window.h"
#include "ui/gfx/native_ui_types.h"

namespace omnibox_everywhere {

void DisassociatePopupOnMac(gfx::NativeWindow native_window) {
  NSWindow* window = native_window.GetNativeNSWindow();
  if (window) {
    // Reset auxiliary collection behavior so macOS Window Server ceases
    // associating Chrome with the current Space. The subsequent widget Hide
    // cleanly orders out the window while maintaining Views state.
    [window setCollectionBehavior:NSWindowCollectionBehaviorDefault];
  }
}

void ActivateBrowserWindowOnMac(BrowserWindowInterface* bwi) {
  if (!bwi || !bwi->GetWindow()) {
    return;
  }

  // Activate the application before ordering the window front to ensure
  // NSWindowDidBecomeKeyNotification is properly delivered and to switch Spaces
  // if the target browser window resides on a different Space than the
  // dismissed auxiliary popup.
  [NSApp activateIgnoringOtherApps:YES];

  NSWindow* window = bwi->GetWindow()->GetNativeWindow().GetNativeNSWindow();
  if (window) {
    if ([window isMiniaturized]) {
      [window deminiaturize:nil];
    }
    [window makeKeyAndOrderFront:nil];
  }
}

}  // namespace omnibox_everywhere
