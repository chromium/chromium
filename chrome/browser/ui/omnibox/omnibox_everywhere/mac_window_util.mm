// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/omnibox/omnibox_everywhere/mac_window_util.h"

#import <AppKit/AppKit.h>

#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "ui/base/base_window.h"
#include "ui/gfx/native_ui_types.h"

namespace omnibox_everywhere {

namespace {

void ActivateWindowImpl(NSWindow* window) {
  if (!window) {
    return;
  }

  // Activate Chrome without NSApplicationActivateAllWindows so only the
  // targeted window is brought to the front rather than all Chrome windows.
  NSApplicationActivationOptions options = 0;
  // NSApplicationActivateIgnoringOtherApps is deprecated and has no effect on
  // macOS 14+.
  if (!@available(macOS 14, *)) {
    options = NSApplicationActivateIgnoringOtherApps;
  }
  [[NSRunningApplication currentApplication] activateWithOptions:options];

  if ([window isMiniaturized]) {
    [window deminiaturize:nil];
  }
  [window makeKeyAndOrderFront:nil];
}

}  // namespace

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

  ActivateWindowImpl(bwi->GetWindow()->GetNativeWindow().GetNativeNSWindow());
}

void ActivatePopupForEmojiPanelOnMac(gfx::NativeWindow native_window) {
  // CharacterPalette.app (IMKInputSession) requires the client application to
  // be active, and ViewsNSWindowDelegate::windowDidResignKey: only ignores
  // key-window resignation for system status bar menu extras when
  // [NSApp isActive] is YES. Activate Chrome without
  // NSApplicationActivateAllWindows so other Chrome windows stay in the
  // background while the non-activating Loomnibox panel remains key.
  ActivateWindowImpl(native_window.GetNativeNSWindow());
}

bool ShouldTreatCurrentEventAsDraggableOnMac() {
  // TODO(b/581982251): Move away from the HTCAPTION approach for dragging.
  NSEvent* event = [NSApp currentEvent];
  if (!event) {
    return false;
  }
  return [event type] == NSEventTypeLeftMouseDown &&
         ([event modifierFlags] & NSEventModifierFlagControl) == 0;
}

}  // namespace omnibox_everywhere
