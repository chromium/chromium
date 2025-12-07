// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/remote_cocoa/app_shim/browser_native_widget_window_mac.h"

#import <AppKit/AppKit.h>

#include "base/mac/mac_util.h"
#include "components/remote_cocoa/app_shim/native_widget_ns_window_bridge.h"
#include "components/remote_cocoa/common/native_widget_ns_window_host.mojom.h"

namespace {
// Workaround for https://crbug.com/1369643
const double kThinControllerHeight = 0.5;
}  // namespace
@interface NSWindow (PrivateBrowserNativeWidgetAPI)
+ (Class)frameViewClassForStyleMask:(NSUInteger)windowStyle;
@end

@interface NSThemeFrame (PrivateBrowserNativeWidgetAPI)
- (CGFloat)_titlebarHeight;
- (CGFloat)_minXTitlebarWidgetInset;
- (CGFloat)_getCachedWindowCornerRadius;
- (void)setStyleMask:(NSUInteger)styleMask;
- (void)setButtonRevealAmount:(double)amount;
@end

@interface BrowserWindowFrame : NativeWidgetMacNSWindowTitledFrame
@end

@implementation BrowserWindowFrame {
  BOOL _inFullScreen;
  BOOL _alwaysShowTrafficLights;
}

// NSThemeFrame overrides.

// Note that while this has an effect on the location of the window control
// widgets, this is also an important part of the functioning of immersive
// fullscreen and must not be removed lest that break.
- (CGFloat)_titlebarHeight {
  auto* window = base::apple::ObjCCast<NativeWidgetMacNSWindow>(self.window);
  remote_cocoa::NativeWidgetNSWindowBridge* bridge = window.bridge;
  if (!bridge) {
    return [super _titlebarHeight];
  }

  // The titlebar will be the same size during non-fullscreen and immersive
  // fullscreen. During content fullscreen the toolbar is hidden and the
  // titlebar will be smaller default height.
  if (!_inFullScreen || bridge->ShouldUseCustomTitlebarHeightForFullscreen()) {
    bool overrideTitlebarHeight = false;
    float titlebarHeight = 0;
    bridge->host()->GetWindowFrameTitlebarHeight(&overrideTitlebarHeight,
                                                 &titlebarHeight);
    if (overrideTitlebarHeight) {
      return titlebarHeight;
    }
  }

  return [super _titlebarHeight];
}

- (CGFloat)_minXTitlebarWidgetInset {
  if (@available(macOS 26, *)) {
    // On macOS 26, position the leading window widget the same distance from
    // the leading edge of the window as it is from the top of the window. That
    // way the window corner can be adjusted to make the widget concentric.
    return 13.0;
  }
  return [super _minXTitlebarWidgetInset];
}

// Override -_getCachedWindowCornerRadius rather than -_cornerRadius because the
// latter does fullscreen checks before calling down to the former, and other
// methods (-_topCornerSize, -_bottomCornerSize) also depend on
// _getCachedWindowCornerRadius.
- (CGFloat)_getCachedWindowCornerRadius {
  if (@available(macOS 26, *)) {
    return 13.0 /* widget position from top and left */ +
           7.0 /* widget radius */;
  }
  // Don't mess with the window radius before macOS 26, as concentricity was not
  // a design element for those releases.
  return [super _getCachedWindowCornerRadius];
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
  if (!_alwaysShowTrafficLights) {
    return;
  }
  NSWindow* window = [self window];
  [[window standardWindowButton:NSWindowCloseButton] setAlphaValue:1.0];
  [[window standardWindowButton:NSWindowMiniaturizeButton] setAlphaValue:1.0];
  [[window standardWindowButton:NSWindowZoomButton] setAlphaValue:1.0];
}

@end

@implementation BrowserNativeWidgetWindow

@synthesize thinTitlebarViewController = _thinTitlebarViewController;

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
    if (base::mac::MacOSMajorVersion() >= 13) {
      _thinTitlebarViewController =
          [[NSTitlebarAccessoryViewController alloc] init];
      NSView* thinView = [[NSView alloc] init];
      thinView.wantsLayer = YES;
      thinView.layer.backgroundColor = NSColor.blackColor.CGColor;
      _thinTitlebarViewController.view = thinView;
      _thinTitlebarViewController.layoutAttribute = NSLayoutAttributeBottom;
      _thinTitlebarViewController.fullScreenMinHeight = kThinControllerHeight;
      _thinTitlebarViewController.hidden = YES;
      [self addTitlebarAccessoryViewController:_thinTitlebarViewController];
    }
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
