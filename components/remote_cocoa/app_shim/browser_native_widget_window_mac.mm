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
- (void)setButtonRevealAmount:(double)amount;
@property(readonly) NSView* closeButton;
@property(readonly) NSView* minimizeButton;
@property(readonly) NSView* zoomButton;
@end

@interface BrowserWindowFrame : NativeWidgetMacNSWindowTitledFrame
@end

@implementation BrowserWindowFrame {
  BOOL _inFullScreen;
  BOOL _alwaysShowTrafficLights;
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

- (void)setButtonRevealAmount:(double)amount {
  // Don't override the reveal amount sent to `super`. `-[NSThemeFrame
  // setButtonRevealAmount:]` performs layout operations in addition to
  // adjusting the visibility of the traffic lights. The layout changes are
  // desired and should be left intact.
  [super setButtonRevealAmount:amount];
  if (amount == 1.0) {
    return;
  }

  [self maybeShowTrafficLights];
}

- (void)setAlwaysShowTrafficLights:(BOOL)alwaysShow {
  _alwaysShowTrafficLights = alwaysShow;
  [self maybeShowTrafficLights];
}

- (void)maybeShowTrafficLights {
  if (!_alwaysShowTrafficLights ||
      ![self respondsToSelector:@selector(closeButton)] ||
      ![self respondsToSelector:@selector(minimizeButton)] ||
      ![self respondsToSelector:@selector(zoomButton)]) {
    return;
  }
  self.closeButton.alphaValue = 1.0;
  self.minimizeButton.alphaValue = 1.0;
  self.zoomButton.alphaValue = 1.0;
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

- (void)setAlwaysShowTrafficLights:(BOOL)alwaysShow {
  [base::apple::ObjCCastStrict<BrowserWindowFrame>(self.contentView.superview)
      setAlwaysShowTrafficLights:alwaysShow];
}

@end
