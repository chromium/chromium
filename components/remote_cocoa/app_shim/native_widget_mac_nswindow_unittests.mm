// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/remote_cocoa/app_shim/native_widget_mac_nswindow.h"

#include <AppKit/AppKit.h>

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/cocoa/window_size_constants.h"

namespace {

NativeWidgetMacNSWindow* CreateWindow() {
  NativeWidgetMacNSWindow* window = [[NativeWidgetMacNSWindow alloc]
      initWithContentRect:ui::kWindowSizeDeterminedLater
                styleMask:NSWindowStyleMaskBorderless
                  backing:NSBackingStoreBuffered
                    defer:NO];
  window.releasedWhenClosed = NO;
  return window;
}

TEST(NativeWidgetMacNSWindowTest, ImmersiveRevealLockDoesNotRequireKey) {
  NativeWidgetMacNSWindow* window = CreateWindow();
  EXPECT_FALSE([window immersiveRevealLockRequiresKey]);
  [window close];
}

// Becoming or resigning key is reported to the parent's
// `childWindowKeyChangedHandler`, if it has one.
TEST(NativeWidgetMacNSWindowTest, ChildWindowKeyChangedHandler) {
  NativeWidgetMacNSWindow* parent = CreateWindow();
  NativeWidgetMacNSWindow* child = CreateWindow();
  [parent addChildWindow:child ordered:NSWindowAbove];

  NSMutableArray<NSWindow*>* reported = [NSMutableArray array];
  parent.childWindowKeyChangedHandler = ^(NSWindow* window) {
    [reported addObject:window];
  };
  [child becomeKeyWindow];
  [child resignKeyWindow];
  EXPECT_EQ(reported.count, 2u);
  EXPECT_EQ(reported.firstObject, child);
  EXPECT_EQ(reported.lastObject, child);

  // Nothing is reported without a handler or once the window is detached.
  parent.childWindowKeyChangedHandler = nil;
  [child becomeKeyWindow];
  [child resignKeyWindow];
  parent.childWindowKeyChangedHandler = ^(NSWindow* window) {
    [reported addObject:window];
  };
  [parent removeChildWindow:child];
  [child becomeKeyWindow];
  [child resignKeyWindow];
  EXPECT_EQ(reported.count, 2u);

  [child close];
  [parent close];
}

}  // namespace
