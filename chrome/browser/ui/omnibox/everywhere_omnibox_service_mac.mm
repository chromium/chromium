// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/omnibox/everywhere_omnibox_service.h"

#import <Cocoa/Cocoa.h>

#include "ui/gfx/native_ui_types.h"
#include "ui/views/widget/widget.h"

bool IsAppActiveOnMac() {
  return [NSApp isActive];
}

void HideAppOnMac() {
  [NSApp hide:nil];
}

void OrderEverywhereOmniboxFrontOnMac(views::Widget* widget) {
  NSWindow* ns_window = widget->GetNativeWindow().GetNativeNSWindow();
  [ns_window setLevel:NSFloatingWindowLevel];
  [ns_window makeKeyWindow];
  [ns_window orderFrontRegardless];
}
