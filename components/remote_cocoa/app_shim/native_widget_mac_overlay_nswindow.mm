// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/remote_cocoa/app_shim/native_widget_mac_overlay_nswindow.h"

#include <AppKit/AppKit.h>

#include "base/mac/mac_util.h"

// Overlay windows are used to render top chrome components while in immersive
// fullscreen. Those rendered views are hosted in a separate AppKit controlled
// window. The overlay windows are a transparent overlay on top of the AppKit
// hosted views, allowing for secondary UI to be aligned properly. Only allow
// `NSWindowCollectionBehaviorTransient` and
// `NSWindowCollectionBehaviorIgnoresCycle` to be set. Other collection
// behaviors can be problematic. In particular,
// `NSWindowCollectionBehaviorPrimary`, when set on an overlay window, can cause
// crashes. See https://crbug.com/348713769 for more info.
constexpr unsigned long kAllowedCollectionBehaviorMask =
    (NSWindowCollectionBehaviorTransient |
     NSWindowCollectionBehaviorIgnoresCycle);

@implementation NativeWidgetMacOverlayNSWindow

- (void)setCollectionBehavior:(NSWindowCollectionBehavior)collectionBehavior {
  [super setCollectionBehavior:collectionBehavior &
                               kAllowedCollectionBehaviorMask];
}

- (NSRect)constrainFrameRect:(NSRect)frameRect toScreen:(NSScreen*)screen {
  if (base::mac::MacOSVersion() >= 26'00'00) {
    // Since macOS 26.0, when the main window enters full screen, the
    // overlay child window’s position is automatically adjusted by the system
    // to just below the menu bar location (even though the menu bar is hidden
    // at that time). During this process, [NSWindow constrainFrameRect] is
    // called and returns the system-adjusted position. Override this
    // function to return the original value frameRect before the adjustment.
    return frameRect;
  }
  return [super constrainFrameRect:frameRect toScreen:screen];
}

// Paint the window a mostly opaque color.
- (void)debugWithColor:(NSColor*)color {
  self.backgroundColor = color;
  self.alphaValue = 0.3;
  self.contentView = [[NSView alloc] init];
}

@end
