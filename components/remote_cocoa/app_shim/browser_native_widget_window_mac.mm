// Copyright 2018 The Chromium Authors
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

  auto* window = base::apple::ObjCCast<NativeWidgetMacNSWindow>([self window]);
  remote_cocoa::NativeWidgetNSWindowBridge* bridge = [window bridge];
  if (!bridge) {
    return [super _titlebarHeight];
  }

  // Ignore the overridden titlebar height when in fullscreen unless
  // kImmersiveFullscreenTabs is enabled and the toolbar is visible. The
  // toolbar is hidden during content fullscreen.
  // In short the titlebar will be the same size during non-fullscreen and
  // kImmersiveFullscreenTabs fullscreen. During content fullscreen the toolbar
  // is hidden and the titlebar will be smaller default height.
  if (!_inFullScreen || bridge->ShouldUseCustomTitlebarHeightForFullscreen()) {
    bridge->host()->GetWindowFrameTitlebarHeight(&overrideTitlebarHeight,
                                                 &titlebarHeight);
  }

  if (overrideTitlebarHeight) {
    return titlebarHeight;
  }
  return [super _titlebarHeight];
}

- (void)setStyleMask:(NSUInteger)styleMask {
  _inFullScreen = (styleMask & NSWindowStyleMaskFullScreen) != 0;
  [super setStyleMask:styleMask];
}

- (BOOL)_shouldCenterTrafficLights {
  return YES;
}

@end

@implementation BrowserNativeWidgetWindow

// NSWindow (PrivateAPI) overrides.

+ (Class)frameViewClassForStyleMask:(NSUInteger)windowStyle {
  // - NSThemeFrame and its subclasses will be nil if it's missing at runtime.
  if ([BrowserWindowFrame class])
    return [BrowserWindowFrame class];
  return [super frameViewClassForStyleMask:windowStyle];
}

- (instancetype)initWithContentRect:(NSRect)contentRect
                          styleMask:(NSUInteger)windowStyle
                            backing:(NSBackingStoreType)bufferingType
                              defer:(BOOL)deferCreation {
  if ((self = [super initWithContentRect:contentRect
                               styleMask:windowStyle
                                 backing:bufferingType
                                   defer:deferCreation])) {
    [NSNotificationCenter.defaultCenter
        addObserver:self
           selector:@selector(windowDidBecomeKey:)
               name:NSWindowDidBecomeKeyNotification
             object:nil];
  }
  return self;
}

- (void)dealloc {
  [NSNotificationCenter.defaultCenter removeObserver:self];
}

- (void)windowDidBecomeKey:(NSNotification*)notify {
  // NSToolbarFullScreenWindow should never become the key window, otherwise
  // the browser window will appear inactive. Activate the browser window
  // when this happens.
  NSWindow* toolbarWindow = notify.object;
  if (toolbarWindow.parentWindow == self &&
      remote_cocoa::IsNSToolbarFullScreenWindow(toolbarWindow)) {
    [self makeKeyAndOrderFront:nil];
  }
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
