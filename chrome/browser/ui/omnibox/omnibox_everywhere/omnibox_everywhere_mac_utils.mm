// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/omnibox/omnibox_everywhere/omnibox_everywhere_mac_utils.h"

#import <Cocoa/Cocoa.h>

#include "ui/gfx/native_ui_types.h"
#include "ui/views/widget/widget.h"

namespace omnibox_everywhere {

bool IsAppActiveOnMac() {
  return [NSApp isActive];
}

void HideAppOnMac() {
  [NSApp hide:nil];
}

void OrderOmniboxEverywhereFrontOnMac(views::Widget* widget) {
  NSWindow* ns_window = widget->GetNativeWindow().GetNativeNSWindow();
  [ns_window
      setCollectionBehavior:NSWindowCollectionBehaviorCanJoinAllSpaces |
                            NSWindowCollectionBehaviorFullScreenAuxiliary];
  [NSApp activateIgnoringOtherApps:YES];
  [ns_window makeKeyWindow];
  [ns_window orderFrontRegardless];
}

}  // namespace omnibox_everywhere
