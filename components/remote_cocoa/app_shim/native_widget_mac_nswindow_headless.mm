// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/remote_cocoa/app_shim/native_widget_mac_nswindow_headless.h"

#import <AppKit/AppKit.h>

#import "base/apple/scoped_objc_class_swizzler.h"
#include "base/check_deref.h"
#include "base/no_destructor.h"
#import "components/remote_cocoa/app_shim/native_widget_mac_nswindow.h"
#import "components/remote_cocoa/app_shim/views_nswindow_delegate.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#import "ui/gfx/mac/coordinate_conversion.h"

using enum NativeWidgetMacNSWindowHeadlessInfo::WindowState;

// A donor class to provide headless specific NSWindow methods implementation.
@interface NativeWidgetMacHeadlessNSWindow : NativeWidgetMacNSWindow

// Window visibility and Z-Order.
- (BOOL)isVisible;
- (BOOL)invokeOriginalIsVisibleForTesting;
- (void)orderFront:(id)sender;
- (void)orderBack:(id)sender;
- (void)orderOut:(id)sender;
- (void)orderWindow:(NSWindowOrderingMode)place relativeTo:(NSInteger)otherWin;
- (void)orderFrontRegardless;

// Window key state.
- (BOOL)isKeyWindow;
- (void)makeKeyAndOrderFront:(id)sender;

// Window fullscreen.
- (NSWindowStyleMask)styleMask;
- (void)setStyleMask:(NSWindowStyleMask)styleMask;
- (void)toggleFullScreen:(id)sender;

// Window zoom.
- (BOOL)isZoomed;
- (void)setIsZoomed:(BOOL)isZoomed;
- (void)performZoom:(id)sender;

// Window miniaturize.
- (BOOL)isMiniaturized;
- (void)miniaturize:(id)sender;
- (void)deminiaturize:(id)sender;
- (void)performMiniaturize:(id)sender;
@end

namespace {

#define DEFINE_SWIZZLER(name, sel)                                            \
  SEL& name##Selector() {                                                     \
    static SEL selector(@selector(sel));                                      \
    return selector;                                                          \
  }                                                                           \
  base::apple::ScopedObjCClassSwizzler& name##Swizzler() {                    \
    static base::NoDestructor<base::apple::ScopedObjCClassSwizzler> swizzler( \
        [NativeWidgetMacNSWindow class],                                      \
        [NativeWidgetMacHeadlessNSWindow class], name##Selector());           \
    return *swizzler;                                                         \
  }

DEFINE_SWIZZLER(isVisible, isVisible)
DEFINE_SWIZZLER(invokeOriginalIsVisibleForTesting,
                invokeOriginalIsVisibleForTesting)
DEFINE_SWIZZLER(orderFront, orderFront:)
DEFINE_SWIZZLER(orderBack, orderBack:)
DEFINE_SWIZZLER(orderOut, orderOut:)
DEFINE_SWIZZLER(orderWindow, orderWindow:relativeTo:)
DEFINE_SWIZZLER(orderFrontRegardless, orderFrontRegardless)

DEFINE_SWIZZLER(isKeyWindow, isKeyWindow)
DEFINE_SWIZZLER(makeKeyAndOrderFront, makeKeyAndOrderFront:)

DEFINE_SWIZZLER(styleMask, styleMask)
DEFINE_SWIZZLER(setStyleMask, setStyleMask:)
DEFINE_SWIZZLER(toggleFullScreen, toggleFullScreen:)

DEFINE_SWIZZLER(isZoomed, isZoomed)
DEFINE_SWIZZLER(setIsZoomed, setIsZoomed:)
DEFINE_SWIZZLER(performZoom, performZoom:)

DEFINE_SWIZZLER(isMiniaturized, isMiniaturized)
DEFINE_SWIZZLER(miniaturize, miniaturize:)
DEFINE_SWIZZLER(deminiaturize, deminiaturize:)
DEFINE_SWIZZLER(performMiniaturize, performMiniaturize:)

void InstallSwizzlers() {
  static dispatch_once_t once;
  dispatch_once(&once, ^{
    isVisibleSwizzler();
    invokeOriginalIsVisibleForTestingSwizzler();
    orderFrontSwizzler();
    orderBackSwizzler();
    orderOutSwizzler();
    orderWindowSwizzler();
    orderFrontRegardlessSwizzler();

    isKeyWindowSwizzler();
    makeKeyAndOrderFrontSwizzler();

    styleMaskSwizzler();
    setStyleMaskSwizzler();
    toggleFullScreenSwizzler();

    isZoomedSwizzler();
    setIsZoomedSwizzler();
    performZoomSwizzler();

    isMiniaturizedSwizzler();
    miniaturizeSwizzler();
    deminiaturizeSwizzler();
    performMiniaturizeSwizzler();
  });
}

// Since the NativeWidgetMacNSWindow we swizzle does not implement the methods
// we swizzle, the base NSWindow class methods will be swizzled instead. The
// following macro ensures that unrelated NSWindow instances behavior is not
// affected.
#define GET_HEADLESS_INFO                                                    \
  [self isKindOfClass:[NativeWidgetMacNSWindow class]] ? [self headlessInfo] \
                                                       : nil
}  // namespace

@implementation NativeWidgetMacHeadlessNSWindow

// In headless mode the platform window is never made visible, instead the
// expected window visibility state is faked providing all the related
// notifications. Note that only visibility state is maintained, the window
// Z-Order is ignored for now.

- (BOOL)isVisible {
  NativeWidgetMacNSWindowHeadlessInfo* headless_info = GET_HEADLESS_INFO;
  if (!headless_info) {
    return isVisibleSwizzler().InvokeOriginal<BOOL>(self, isVisibleSelector());
  }

  return headless_info->is_visible;
}

- (BOOL)invokeOriginalIsVisibleForTesting {
  // Return the original [isVisible] result which is supposed to be not visible
  // while in headless mode.
  return isVisibleSwizzler().InvokeOriginal<BOOL>(self, isVisibleSelector());
}

- (void)orderFront:(id)sender {
  NativeWidgetMacNSWindowHeadlessInfo* headless_info = GET_HEADLESS_INFO;
  if (!headless_info) {
    orderFrontSwizzler().InvokeOriginal<void, id>(self, orderFrontSelector(),
                                                  sender);
    return;
  }

  if (headless_info->is_visible) {
    return;
  }

  headless_info->is_visible = true;

  ViewsNSWindowDelegate* delegate =
      base::apple::ObjCCastStrict<ViewsNSWindowDelegate>([self delegate]);
  if (delegate) {
    [delegate onWindowOrderChanged:nil];
  }
}

- (void)orderBack:(id)sender {
  NativeWidgetMacNSWindowHeadlessInfo* headless_info = GET_HEADLESS_INFO;
  if (!headless_info) {
    orderBackSwizzler().InvokeOriginal<void, id>(self, orderBackSelector(),
                                                 sender);
    return;
  }

  if (headless_info->is_visible) {
    return;
  }

  headless_info->is_visible = true;

  ViewsNSWindowDelegate* delegate =
      base::apple::ObjCCastStrict<ViewsNSWindowDelegate>([self delegate]);
  if (delegate) {
    [delegate onWindowOrderChanged:nil];
  }
}

- (void)orderOut:(id)sender {
  NativeWidgetMacNSWindowHeadlessInfo* headless_info = GET_HEADLESS_INFO;
  if (!headless_info) {
    orderOutSwizzler().InvokeOriginal<void, id>(self, orderOutSelector(),
                                                sender);
    return;
  }

  if (!headless_info->is_visible) {
    return;
  }

  ViewsNSWindowDelegate* delegate =
      base::apple::ObjCCastStrict<ViewsNSWindowDelegate>([self delegate]);

  if ([self isKeyWindow]) {
    headless_info->is_key = false;
    if (delegate) {
      [delegate windowDidResignKey:nil];
    }
  }

  headless_info->is_visible = false;

  if (delegate) {
    [delegate onWindowOrderChanged:nil];
  }
}

- (void)orderWindow:(NSWindowOrderingMode)place relativeTo:(NSInteger)otherWin {
  NativeWidgetMacNSWindowHeadlessInfo* headless_info = GET_HEADLESS_INFO;
  if (!headless_info) {
    orderWindowSwizzler().InvokeOriginal<void, NSWindowOrderingMode, NSInteger>(
        self, orderWindowSelector(), place, otherWin);
    return;
  }

  switch (place) {
    case NSWindowOut:
      [self orderOut:nil];
      break;
    case NSWindowAbove:
      [self orderFront:nil];
      break;
    case NSWindowBelow:
      [self orderBack:nil];
      break;
  }
}

- (void)orderFrontRegardless {
  NativeWidgetMacNSWindowHeadlessInfo* headless_info = GET_HEADLESS_INFO;
  if (!headless_info) {
    orderFrontRegardlessSwizzler().InvokeOriginal<void>(
        self, orderFrontRegardlessSelector());
    return;
  }

  [self orderFront:nil];
}

// In headless mode window key state needs to be faked to ensure that focused
// headless widnow do not affect each other.

- (BOOL)isKeyWindow {
  NativeWidgetMacNSWindowHeadlessInfo* headless_info = GET_HEADLESS_INFO;
  if (!headless_info) {
    return isKeyWindowSwizzler().InvokeOriginal<BOOL>(self,
                                                      isKeyWindowSelector());
  }

  return headless_info->is_key;
}

- (void)makeKeyAndOrderFront:(id)sender {
  NativeWidgetMacNSWindowHeadlessInfo* headless_info = GET_HEADLESS_INFO;
  if (!headless_info) {
    makeKeyAndOrderFrontSwizzler().InvokeOriginal<void, id>(
        self, makeKeyAndOrderFrontSelector(), sender);
    return;
  }

  [self orderFront:nil];

  if ([self isKeyWindow]) {
    return;
  }

  headless_info->is_key = true;

  ViewsNSWindowDelegate* delegate =
      base::apple::ObjCCastStrict<ViewsNSWindowDelegate>([self delegate]);
  if (delegate) {
    [delegate windowDidBecomeKey:nil];
  }
}

// In headless mode the full screen functionality is overridden to use the
// headless screen configuration.

- (NSWindowStyleMask)styleMask {
  NSWindowStyleMask styleMask =
      styleMaskSwizzler().InvokeOriginal<NSWindowStyleMask>(
          self, styleMaskSelector());

  NativeWidgetMacNSWindowHeadlessInfo* headless_info = GET_HEADLESS_INFO;
  if (!headless_info) {
    return styleMask;
  }

  CHECK_EQ(styleMask & NSWindowStyleMaskFullScreen, 0ul);

  if (headless_info->window_state == kFullscreen) {
    styleMask |= NSWindowStyleMaskFullScreen;
  }

  return styleMask;
}

- (void)setStyleMask:(NSWindowStyleMask)styleMask {
  NativeWidgetMacNSWindowHeadlessInfo* headless_info = GET_HEADLESS_INFO;

  // In headlesss mode the fullscreen style is synthesized, so don't allow
  // anyone to actually set it. If we let it through to the default
  // |setStyleMask| implementation it will throw 'NSWindowStyleMaskFullScreen
  // set on a window outside of a full screen transition' exception.
  if (headless_info) {
    styleMask &= ~NSWindowStyleMaskFullScreen;
  }

  setStyleMaskSwizzler().InvokeOriginal<void, NSWindowStyleMask>(
      self, setStyleMaskSelector(), styleMask);
}

- (void)toggleFullScreen:(id)sender {
  NativeWidgetMacNSWindowHeadlessInfo* headless_info = GET_HEADLESS_INFO;
  if (!headless_info) {
    toggleFullScreenSwizzler().InvokeOriginal<void, id>(
        self, toggleFullScreenSelector(), sender);
    return;
  }

  ViewsNSWindowDelegate* delegate =
      base::apple::ObjCCastStrict<ViewsNSWindowDelegate>([self delegate]);

  if (headless_info->window_state != kFullscreen) {
    if (delegate) {
      [delegate windowWillEnterFullScreen:nil];
    }

    const gfx::Rect frame_rect = gfx::ScreenRectFromNSRect([super frame]);
    headless_info->restored_bounds = frame_rect;
    headless_info->window_state = kFullscreen;

    // Fullscreen state uses the entire screen estate.
    display::Screen& screen = CHECK_DEREF(display::Screen::Get());
    display::Display display = screen.GetDisplayMatching(frame_rect);
    NSRect zoomed_frame = gfx::ScreenRectToNSRect(display.bounds());
    [self setFrame:zoomed_frame display:NO animate:NO];

    if (delegate) {
      [delegate windowDidEnterFullScreen:nil];
    }
  } else {
    if (delegate) {
      [delegate windowWillExitFullScreen:nil];
    }

    headless_info->window_state = kNormal;

    if (headless_info->restored_bounds) {
      NSRect restored_frame =
          gfx::ScreenRectToNSRect(headless_info->restored_bounds.value());
      [self setFrame:restored_frame display:NO animate:NO];
      headless_info->restored_bounds.reset();
    }

    if (delegate) {
      [delegate windowDidExitFullScreen:nil];
    }
  }
}

// In headless mode the window zoom functionality is overridden to use the
// headless screen configuration. Note that the platform window is resized as
// required thus providing all the usual window sizing notifications.

- (BOOL)isZoomed {
  NativeWidgetMacNSWindowHeadlessInfo* headless_info = GET_HEADLESS_INFO;
  if (!headless_info) {
    return isZoomedSwizzler().InvokeOriginal<BOOL>(self, isZoomedSelector());
  }

  return headless_info->window_state == kZoomed;
}

- (void)setIsZoomed:(BOOL)isZoomed {
  NativeWidgetMacNSWindowHeadlessInfo* headless_info = GET_HEADLESS_INFO;
  if (!headless_info) {
    setIsZoomedSwizzler().InvokeOriginal<void, BOOL>(
        self, setIsZoomedSelector(), isZoomed);
    return;
  }

  if (isZoomed) {
    if ([self isZoomed]) {
      return;
    }
    const gfx::Rect frame_rect = gfx::ScreenRectFromNSRect([super frame]);
    headless_info->restored_bounds = frame_rect;
    headless_info->window_state = kZoomed;

    // Zoomed state uses the screen's work area.
    display::Screen& screen = CHECK_DEREF(display::Screen::Get());
    display::Display display = screen.GetDisplayMatching(frame_rect);
    NSRect zoomed_frame = gfx::ScreenRectToNSRect(display.work_area());
    [self setFrame:zoomed_frame display:NO animate:NO];
  } else {
    if (![self isZoomed]) {
      return;
    }
    headless_info->window_state = kNormal;

    if (headless_info->restored_bounds) {
      NSRect restored_frame =
          gfx::ScreenRectToNSRect(headless_info->restored_bounds.value());
      [self setFrame:restored_frame display:NO animate:NO];
      headless_info->restored_bounds.reset();
    }
  }
}

- (void)performZoom:(id)sender {
  NativeWidgetMacNSWindowHeadlessInfo* headless_info = GET_HEADLESS_INFO;
  if (!headless_info) {
    performZoomSwizzler().InvokeOriginal<void, id>(self, performZoomSelector(),
                                                   sender);
    return;
  }

  if (![self isZoomed]) {
    if (![self isZoomable]) {
      return;
    }
    [self setIsZoomed:YES];
  } else {
    [self setIsZoomed:NO];
  }
}

// In headless mode window miniaturize functionality is overridden so that it
// uses headless window state and visibility.

- (BOOL)isMiniaturized {
  NativeWidgetMacNSWindowHeadlessInfo* headless_info = GET_HEADLESS_INFO;
  if (!headless_info) {
    return isMiniaturizedSwizzler().InvokeOriginal<BOOL>(
        self, isMiniaturizedSelector());
  }

  return headless_info->window_state == kMiniaturized;
}

- (void)miniaturize:(id)sender {
  NativeWidgetMacNSWindowHeadlessInfo* headless_info = GET_HEADLESS_INFO;
  if (!headless_info) {
    miniaturizeSwizzler().InvokeOriginal<void, id>(self, miniaturizeSelector(),
                                                   sender);
    return;
  }

  if ([self isMiniaturized]) {
    return;
  }

  ViewsNSWindowDelegate* delegate =
      base::apple::ObjCCastStrict<ViewsNSWindowDelegate>([self delegate]);

  if ([self isKeyWindow]) {
    headless_info->is_key = false;
    if (delegate) {
      [delegate windowDidResignKey:nil];
    }
  }

  headless_info->is_visible = false;
  headless_info->window_state = kMiniaturized;

  if (delegate) {
    [delegate windowDidMiniaturize:nil];
  }
}

- (void)deminiaturize:(id)sender {
  NativeWidgetMacNSWindowHeadlessInfo* headless_info = GET_HEADLESS_INFO;
  if (!headless_info) {
    deminiaturizeSwizzler().InvokeOriginal<void, id>(
        self, deminiaturizeSelector(), sender);
    return;
  }

  if (![self isMiniaturized]) {
    return;
  }

  headless_info->window_state = kNormal;
  headless_info->is_visible = true;

  ViewsNSWindowDelegate* delegate =
      base::apple::ObjCCastStrict<ViewsNSWindowDelegate>([self delegate]);
  if (delegate) {
    [delegate windowDidDeminiaturize:nil];
  }

  if (![self isKeyWindow]) {
    headless_info->is_key = true;
    if (delegate) {
      [delegate windowDidBecomeKey:nil];
    }
  }
}

- (void)performMiniaturize:(id)sender {
  NativeWidgetMacNSWindowHeadlessInfo* headless_info = GET_HEADLESS_INFO;
  if (!headless_info) {
    performMiniaturizeSwizzler().InvokeOriginal<void, id>(
        self, performMiniaturizeSelector(), sender);
    return;
  }

  if (![self isMiniaturized]) {
    [self miniaturize:sender];
  } else {
    [self deminiaturize:sender];
  }
}

@end

// Use NativeWidgetMacNSWindowHeadlessInfo constructor to swizzle the
// NativeWidgetMacNSWindow methods.
NativeWidgetMacNSWindowHeadlessInfo::NativeWidgetMacNSWindowHeadlessInfo() {
  InstallSwizzlers();
}
