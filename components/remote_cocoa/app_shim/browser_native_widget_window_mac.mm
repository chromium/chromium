// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/remote_cocoa/app_shim/browser_native_widget_window_mac.h"

#import <AppKit/AppKit.h>

#include "components/remote_cocoa/app_shim/native_widget_ns_window_bridge.h"
#include "components/remote_cocoa/common/native_widget_ns_window_host.mojom.h"

@interface NSWindow (PrivateBrowserNativeWidgetAPI)
+ (Class)frameViewClassForStyleMask:(NSUInteger)windowStyle;
@end

@interface NSThemeFrame (PrivateBrowserNativeWidgetAPI)
- (CGFloat)_titlebarHeight;
- (void)setStyleMask:(NSUInteger)styleMask;
@end

@interface BrowserWindowFrame : NativeWidgetMacNSWindowTitledFrame
@end

@implementation BrowserWindowFrame {
  BOOL _inFullScreen;
}

// NSThemeFrame overrides.

- (CGFloat)_titlebarHeight {
  bool overrideTitlebarHeight = false;
  float titlebarHeight = 0;

  if (!_inFullScreen) {
    auto* window = base::mac::ObjCCast<NativeWidgetMacNSWindow>([self window]);
    remote_cocoa::NativeWidgetNSWindowBridge* bridge = [window bridge];
    if (bridge) {
      bridge->host()->GetWindowFrameTitlebarHeight(&overrideTitlebarHeight,
                                                   &titlebarHeight);
    }
  }
  if (overrideTitlebarHeight)
    return titlebarHeight;
  return [super _titlebarHeight];
}

- (void)setStyleMask:(NSUInteger)styleMask {
  _inFullScreen = (styleMask & NSWindowStyleMaskFullScreen) != 0;
  [super setStyleMask:styleMask];
}

- (BOOL)_shouldCenterTrafficLights {
  return YES;
}

// On 10.10, this prevents the window server from treating the title bar as an
// unconditionally-draggable region, and allows -[BridgedContentView hitTest:]
// to choose case-by-case whether to take a mouse event or let it turn into a
// window drag. Not needed for newer macOS. See r549802 for details.
- (NSRect)_draggableFrame NS_DEPRECATED_MAC(10_10, 10_11) {
  return NSZeroRect;
}

@end

@implementation BrowserNativeWidgetWindow

// Prevent detached tabs from glitching when the window is partially offscreen.
// See https://crbug.com/1095717 for details.
// This is easy to get wrong so scope very tightly to only disallow large
// vertical jumps.
- (NSRect)constrainFrameRect:(NSRect)rect toScreen:(NSScreen*)screen {
  NSRect proposed = [super constrainFrameRect:rect toScreen:screen];
  // This boils down to: use the small threshold when we're not avoiding a
  // Dock on the bottom, and the big threshold otherwise.
  static constexpr CGFloat kBigThreshold = 200;
  static constexpr CGFloat kSmallThreshold = 50;
  const CGFloat yDelta = NSMaxY(proposed) - NSMaxY(rect);
  if (yDelta > kBigThreshold ||
      (yDelta > kSmallThreshold && NSMinY(proposed) == 0))
    return rect;
  return proposed;
}

// NSWindow (PrivateAPI) overrides.

+ (Class)frameViewClassForStyleMask:(NSUInteger)windowStyle {
  // - NSThemeFrame and its subclasses will be nil if it's missing at runtime.
  if ([BrowserWindowFrame class])
    return [BrowserWindowFrame class];
  return [super frameViewClassForStyleMask:windowStyle];
}

// The base implementation returns YES if the window's frame view is a custom
// class, which causes undesirable changes in behavior. AppKit NSWindow
// subclasses are known to override it and return NO.
- (BOOL)_usesCustomDrawing {
  return NO;
}

// Handle "Move focus to the window toolbar" configured in System Preferences ->
// Keyboard -> Shortcuts -> Keyboard. Usually Ctrl+F5. The argument (|unknown|)
// tends to just be nil.
- (void)_handleFocusToolbarHotKey:(id)unknown {
  remote_cocoa::NativeWidgetNSWindowBridge* bridge = [self bridge];
  if (bridge)
    bridge->host()->OnFocusWindowToolbar();
}

@end
