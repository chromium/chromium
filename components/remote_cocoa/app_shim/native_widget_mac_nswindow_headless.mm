// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/remote_cocoa/app_shim/native_widget_mac_nswindow_headless.h"

#import "base/apple/scoped_objc_class_swizzler.h"
#include "base/check_deref.h"
#include "base/no_destructor.h"
#import "components/remote_cocoa/app_shim/native_widget_mac_nswindow.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#import "ui/gfx/mac/coordinate_conversion.h"

// A donor class to provide headless specific NativeWidgetMacNSWindow
// methods implementations.
@interface NativeWidgetMacHeadlessNSWindow : NativeWidgetMacNSWindow
- (BOOL)isZoomed;
- (void)setIsZoomed:(BOOL)isZoomed;
- (void)performZoom:(id)sender;
@end

namespace {

#define DEFINE_SWIZZLER(name, sel)                                            \
  base::apple::ScopedObjCClassSwizzler& name() {                              \
    static base::NoDestructor<base::apple::ScopedObjCClassSwizzler> swizzler( \
        [NativeWidgetMacNSWindow class],                                      \
        [NativeWidgetMacHeadlessNSWindow class], @selector(sel));             \
    return *swizzler;                                                         \
  }

DEFINE_SWIZZLER(isZoomedSwizzler, isZoomed)
DEFINE_SWIZZLER(setIsZoomedSwizzler, setIsZoomed:)
DEFINE_SWIZZLER(performZoomSwizzler, performZoom:)

void InstallSwizzlers() {
  static dispatch_once_t once;
  dispatch_once(&once, ^{
    isZoomedSwizzler();
    setIsZoomedSwizzler();
    performZoomSwizzler();
  });
}

}  // namespace

@implementation NativeWidgetMacHeadlessNSWindow

- (BOOL)isZoomed {
  NativeWidgetMacNSWindowHeadlessInfo* headless_info = [self headlessInfo];
  if (!headless_info) {
    return isZoomedSwizzler().InvokeOriginal<BOOL>(self, @selector(isZoomed));
  }

  return headless_info->is_zoomed;
}

- (void)setIsZoomed:(BOOL)isZoomed {
  NativeWidgetMacNSWindowHeadlessInfo* headless_info = [self headlessInfo];
  if (!headless_info) {
    setIsZoomedSwizzler().InvokeOriginal<void, BOOL>(
        self, @selector(setIsZoomed:), isZoomed);
    return;
  }

  if (isZoomed) {
    if (headless_info->is_zoomed) {
      return;
    }
    const gfx::Rect frame_rect = gfx::ScreenRectFromNSRect([super frame]);
    headless_info->restored_bounds = frame_rect;
    headless_info->is_zoomed = true;

    display::Screen& screen = CHECK_DEREF(display::Screen::Get());
    display::Display display = screen.GetDisplayMatching(frame_rect);
    NSRect zoomed_frame = gfx::ScreenRectToNSRect(display.work_area());
    [self setFrame:zoomed_frame display:NO animate:NO];
  } else {
    if (!headless_info->is_zoomed) {
      return;
    }
    headless_info->is_zoomed = false;

    if (headless_info->restored_bounds) {
      NSRect restored_frame =
          gfx::ScreenRectToNSRect(headless_info->restored_bounds.value());
      [self setFrame:restored_frame display:NO animate:NO];
      headless_info->restored_bounds.reset();
    }
  }
}

- (void)performZoom:(id)sender {
  NativeWidgetMacNSWindowHeadlessInfo* headless_info = [self headlessInfo];
  if (!headless_info) {
    performZoomSwizzler().InvokeOriginal<void, id>(
        self, @selector(performZoom:), sender);
    return;
  }

  if (!headless_info->is_zoomed) {
    if (![self isZoomable]) {
      return;
    }
    [self setIsZoomed:YES];
  } else {
    [self setIsZoomed:NO];
  }
}
@end

// Use NativeWidgetMacNSWindowHeadlessInfo constructor to swizzle the
// NativeWidgetMacNSWindow methods.
NativeWidgetMacNSWindowHeadlessInfo::NativeWidgetMacNSWindowHeadlessInfo() {
  InstallSwizzlers();
}
