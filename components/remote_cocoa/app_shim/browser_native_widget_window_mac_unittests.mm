// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/remote_cocoa/app_shim/browser_native_widget_window_mac.h"

#include <AppKit/AppKit.h>

#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"
#include "ui/base/cocoa/window_size_constants.h"

using NativeWidgetMacOverlayNSWindowTest = PlatformTest;

@interface NSThemeFrame (PrivateTrafficLightsAPI)
- (void)setButtonRevealAmount:(double)amount;
@property(readonly) NSView* closeButton;
@property(readonly) NSView* minimizeButton;
@property(readonly) NSView* zoomButton;
@end

// Test that private NSThemeFrame traffic light methods can be used to always
// show the traffic lights.
TEST(BrowserNativeWidgetWindowTest, AlwaysShowTrafficLights) {
  BrowserNativeWidgetWindow* browser_window = [[BrowserNativeWidgetWindow alloc]
      initWithContentRect:ui::kWindowSizeDeterminedLater
                styleMask:NSWindowStyleMaskTitled | NSWindowStyleMaskClosable |
                          NSWindowStyleMaskMiniaturizable |
                          NSWindowStyleMaskResizable
                  backing:NSBackingStoreBuffered
                    defer:NO];
  NSThemeFrame* theme_frame = base::apple::ObjCCastStrict<NSThemeFrame>(
      browser_window.contentView.superview);

  // Make sure the `NSThemeFrame` responds to the private API we are declaring.
  ASSERT_TRUE([theme_frame respondsToSelector:@selector(closeButton)]);
  ASSERT_TRUE([theme_frame respondsToSelector:@selector(minimizeButton)]);
  ASSERT_TRUE([theme_frame respondsToSelector:@selector(zoomButton)]);
  ASSERT_TRUE(
      [theme_frame respondsToSelector:@selector(setButtonRevealAmount:)]);

  // Ensure the traffic lights are visible.
  EXPECT_EQ(theme_frame.closeButton.alphaValue, 1.0);
  EXPECT_EQ(theme_frame.minimizeButton.alphaValue, 1.0);
  EXPECT_EQ(theme_frame.zoomButton.alphaValue, 1.0);

  // Hide the traffic lights.
  [theme_frame setButtonRevealAmount:0];
  EXPECT_EQ(theme_frame.closeButton.alphaValue, 0.0);
  EXPECT_EQ(theme_frame.minimizeButton.alphaValue, 0.0);
  EXPECT_EQ(theme_frame.zoomButton.alphaValue, 0.0);

  // Always show the traffic lights.
  [browser_window setAlwaysShowTrafficLights:YES];
  EXPECT_EQ(theme_frame.closeButton.alphaValue, 1.0);
  EXPECT_EQ(theme_frame.minimizeButton.alphaValue, 1.0);
  EXPECT_EQ(theme_frame.zoomButton.alphaValue, 1.0);

  // Try to hide the traffic lights again, they should remain visible.
  [theme_frame setButtonRevealAmount:0.0];
  EXPECT_EQ(theme_frame.closeButton.alphaValue, 1.0);
  EXPECT_EQ(theme_frame.minimizeButton.alphaValue, 1.0);
  EXPECT_EQ(theme_frame.zoomButton.alphaValue, 1.0);

  // Remove the always show pin, the traffic lights should be able hide again.
  [browser_window setAlwaysShowTrafficLights:NO];

  // Subsequent calls to `-setButtonRevealAmount:` with the same parameter value
  // will have no effect. Call with a temporary value, then with the desired
  // value.
  [theme_frame setButtonRevealAmount:1.0];
  [theme_frame setButtonRevealAmount:0];
  EXPECT_EQ(theme_frame.closeButton.alphaValue, 0.0);
  EXPECT_EQ(theme_frame.minimizeButton.alphaValue, 0.0);
  EXPECT_EQ(theme_frame.zoomButton.alphaValue, 0.0);
}
