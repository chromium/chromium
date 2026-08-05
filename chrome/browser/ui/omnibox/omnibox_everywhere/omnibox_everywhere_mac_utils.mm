// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/apple/foundation_util.h"
#import "components/remote_cocoa/app_shim/native_widget_mac_nswindow.h"
#include "ui/gfx/native_ui_types.h"
#include "ui/views/widget/widget.h"

namespace omnibox_everywhere {

void OrderOmniboxEverywhereFrontOnMac(views::Widget* widget) {
  NativeWidgetMacNSWindow* ns_window =
      base::apple::ObjCCast<NativeWidgetMacNSWindow>(
          widget->GetNativeWindow().GetNativeNSWindow());
  if (ns_window) {
    [ns_window setActivationIndependence:YES];
  }
  NSWindow* raw_window = widget->GetNativeWindow().GetNativeNSWindow();
  [raw_window
      setCollectionBehavior:NSWindowCollectionBehaviorCanJoinAllSpaces |
                            NSWindowCollectionBehaviorFullScreenAuxiliary];
  // Because Omnibox Everywhere can be invoked via a global shortcut while
  // another application is active, explicitly activate the Chromium app so that
  // key window focus is acquired and keyboard/clipboard events (e.g. Cmd+C,
  // Cmd+V) are properly routed.
  [NSApp activateIgnoringOtherApps:YES];
  [raw_window makeKeyAndOrderFront:nil];
}

}  // namespace omnibox_everywhere
