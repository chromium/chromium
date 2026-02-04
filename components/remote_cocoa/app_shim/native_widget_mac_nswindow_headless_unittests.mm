// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/remote_cocoa/app_shim/native_widget_mac_nswindow_headless.h"

#include <AppKit/AppKit.h>

#include <memory>

#import "components/remote_cocoa/app_shim/native_widget_mac_nswindow.h"
#import "components/remote_cocoa/app_shim/native_widget_mac_overlay_nswindow.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/cocoa/window_size_constants.h"
#include "ui/display/display.h"
#import "ui/display/mac/test/test_screen_mac.h"
#include "ui/display/screen.h"
#include "ui/gfx/geometry/size.h"

namespace {

class NativeWidgetMacNSWindowHeadlessTest : public ::testing::Test {
 public:
  NativeWidgetMacNSWindowHeadlessTest() = default;
  ~NativeWidgetMacNSWindowHeadlessTest() override = default;

  NativeWidgetMacNSWindowHeadlessTest(
      const NativeWidgetMacNSWindowHeadlessTest&) = delete;
  NativeWidgetMacNSWindowHeadlessTest& operator=(
      const NativeWidgetMacNSWindowHeadlessTest&) = delete;

  void TearDown() override {
    if (display::Screen::Get() == test_screen_.get()) {
      display::Screen::SetScreenInstance(nullptr);
    }
  }

 protected:
  void CreateTestScreen(const gfx::Size& size) {
    test_screen_ = std::make_unique<display::test::TestScreenMac>(size);
    display::Screen::SetScreenInstance(test_screen_.get());
  }

  std::unique_ptr<display::test::TestScreenMac> test_screen_;
};

TEST_F(NativeWidgetMacNSWindowHeadlessTest, HeadlessInfoCreated) {
  NativeWidgetMacNSWindow* window = [[NativeWidgetMacNSWindow alloc]
      initWithContentRect:ui::kWindowSizeDeterminedLater
                styleMask:NSWindowStyleMaskBorderless
                  backing:NSBackingStoreBuffered
                    defer:NO];

  // In headless mode the NativeWidgetMacNSWindow wrapper visibility follows the
  // expectations, however, the underlying platform NSWindow is always hidden.
  [window setIsHeadless:YES];
  EXPECT_TRUE([window isHeadless]);
  EXPECT_FALSE([window isVisible]);
  EXPECT_FALSE([window invokeOriginalIsVisibleForTesting]);

  EXPECT_NE([window headlessInfo], nullptr);
}

TEST_F(NativeWidgetMacNSWindowHeadlessTest, SubclassIsAlsoSwizzled) {
  NativeWidgetMacOverlayNSWindow* window =
      [[NativeWidgetMacOverlayNSWindow alloc]
          initWithContentRect:ui::kWindowSizeDeterminedLater
                    styleMask:NSWindowStyleMaskBorderless
                      backing:NSBackingStoreBuffered
                        defer:NO];

  [window setIsHeadless:YES];
  EXPECT_TRUE([window isHeadless]);
  EXPECT_FALSE([window isVisible]);
  EXPECT_FALSE([window invokeOriginalIsVisibleForTesting]);

  EXPECT_NE([window headlessInfo], nullptr);

  [window orderFront:nil];
  EXPECT_TRUE([window isVisible]);
  EXPECT_FALSE([window invokeOriginalIsVisibleForTesting]);
}

TEST_F(NativeWidgetMacNSWindowHeadlessTest, UnrelatedNSWindowIsNotAffected) {
  NativeWidgetMacOverlayNSWindow* window =
      [[NativeWidgetMacOverlayNSWindow alloc]
          initWithContentRect:ui::kWindowSizeDeterminedLater
                    styleMask:NSWindowStyleMaskBorderless
                      backing:NSBackingStoreBuffered
                        defer:NO];

  [window setIsHeadless:YES];
  EXPECT_TRUE([window isHeadless]);
  EXPECT_FALSE([window isVisible]);
  EXPECT_FALSE([window invokeOriginalIsVisibleForTesting]);

  [window orderFront:nil];
  EXPECT_TRUE([window isVisible]);
  EXPECT_FALSE([window invokeOriginalIsVisibleForTesting]);

  NSWindow* other_window =
      [[NSWindow alloc] initWithContentRect:ui::kWindowSizeDeterminedLater
                                  styleMask:NSWindowStyleMaskBorderless
                                    backing:NSBackingStoreBuffered
                                      defer:NO];
  EXPECT_FALSE([other_window isVisible]);
  [other_window orderFront:nil];
  EXPECT_TRUE([other_window isVisible]);
}

TEST_F(NativeWidgetMacNSWindowHeadlessTest, HeadlessWindowCanBeOrdered) {
  NativeWidgetMacNSWindow* window = [[NativeWidgetMacNSWindow alloc]
      initWithContentRect:ui::kWindowSizeDeterminedLater
                styleMask:NSWindowStyleMaskBorderless
                  backing:NSBackingStoreBuffered
                    defer:NO];

  [window setIsHeadless:YES];
  ASSERT_TRUE([window isHeadless]);
  ASSERT_FALSE([window isVisible]);
  ASSERT_FALSE([window invokeOriginalIsVisibleForTesting]);

  [window orderFront:nil];
  EXPECT_TRUE([window isVisible]);
  EXPECT_FALSE([window invokeOriginalIsVisibleForTesting]);

  [window orderOut:nil];
  EXPECT_FALSE([window isVisible]);
  EXPECT_FALSE([window invokeOriginalIsVisibleForTesting]);

  [window orderBack:nil];
  EXPECT_TRUE([window isVisible]);
  EXPECT_FALSE([window invokeOriginalIsVisibleForTesting]);

  [window orderOut:nil];
  EXPECT_FALSE([window isVisible]);
  EXPECT_FALSE([window invokeOriginalIsVisibleForTesting]);

  [window orderWindow:NSWindowAbove relativeTo:0];
  EXPECT_TRUE([window isVisible]);
  EXPECT_FALSE([window invokeOriginalIsVisibleForTesting]);

  [window orderWindow:NSWindowOut relativeTo:0];
  EXPECT_FALSE([window isVisible]);
  EXPECT_FALSE([window invokeOriginalIsVisibleForTesting]);

  [window orderWindow:NSWindowBelow relativeTo:0];
  EXPECT_TRUE([window isVisible]);
  EXPECT_FALSE([window invokeOriginalIsVisibleForTesting]);

  [window orderOut:nil];
  EXPECT_FALSE([window isVisible]);
  EXPECT_FALSE([window invokeOriginalIsVisibleForTesting]);

  [window orderFrontRegardless];
  EXPECT_TRUE([window isVisible]);
  EXPECT_FALSE([window invokeOriginalIsVisibleForTesting]);
}

TEST_F(NativeWidgetMacNSWindowHeadlessTest,
       HeadlessWindowMakeKeyAndOrderFront) {
  NativeWidgetMacNSWindow* window = [[NativeWidgetMacNSWindow alloc]
      initWithContentRect:ui::kWindowSizeDeterminedLater
                styleMask:NSWindowStyleMaskBorderless
                  backing:NSBackingStoreBuffered
                    defer:NO];

  [window setIsHeadless:YES];
  ASSERT_TRUE([window isHeadless]);
  ASSERT_FALSE([window isVisible]);
  ASSERT_FALSE([window isKeyWindow]);

  [window makeKeyAndOrderFront:nil];
  EXPECT_TRUE([window isVisible]);
  EXPECT_TRUE([window isKeyWindow]);

  [window orderOut:nil];
  EXPECT_FALSE([window isVisible]);
  EXPECT_FALSE([window isKeyWindow]);
}

TEST_F(NativeWidgetMacNSWindowHeadlessTest, HeadlessWindowCanToggleFullScreen) {
  CreateTestScreen(gfx::Size(1600, 1200));

  NativeWidgetMacNSWindow* window = [[NativeWidgetMacNSWindow alloc]
      initWithContentRect:ui::kWindowSizeDeterminedLater
                styleMask:NSWindowStyleMaskResizable |
                          NSWindowStyleMaskClosable |
                          NSWindowStyleMaskMiniaturizable
                  backing:NSBackingStoreBuffered
                    defer:NO];

  [window setIsHeadless:YES];
  ASSERT_TRUE([window isHeadless]);
  ASSERT_NE([window styleMask] & NSWindowStyleMaskFullScreen,
            NSWindowStyleMaskFullScreen);

  [window toggleFullScreen:nil];
  EXPECT_EQ([window styleMask] & NSWindowStyleMaskFullScreen,
            NSWindowStyleMaskFullScreen);

  [window toggleFullScreen:nil];
  EXPECT_NE([window styleMask] & NSWindowStyleMaskFullScreen,
            NSWindowStyleMaskFullScreen);
}

TEST_F(NativeWidgetMacNSWindowHeadlessTest, HeadlessWindowCanBeZoomedUnzoomed) {
  CreateTestScreen(gfx::Size(1600, 1200));

  NativeWidgetMacNSWindow* window = [[NativeWidgetMacNSWindow alloc]
      initWithContentRect:ui::kWindowSizeDeterminedLater
                styleMask:NSWindowStyleMaskBorderless
                  backing:NSBackingStoreBuffered
                    defer:NO];

  [window setIsHeadless:YES];
  ASSERT_TRUE([window isHeadless]);
  ASSERT_FALSE([window isZoomed]);

  [window setIsZoomed:YES];
  EXPECT_TRUE([window isZoomed]);

  [window setIsZoomed:NO];
  EXPECT_FALSE([window isZoomed]);
}

TEST_F(NativeWidgetMacNSWindowHeadlessTest, HeadlessWindowCanPerformZoom) {
  CreateTestScreen(gfx::Size(1600, 1200));

  NativeWidgetMacNSWindow* window = [[NativeWidgetMacNSWindow alloc]
      initWithContentRect:ui::kWindowSizeDeterminedLater
                styleMask:NSWindowStyleMaskResizable |
                          NSWindowStyleMaskClosable |
                          NSWindowStyleMaskMiniaturizable
                  backing:NSBackingStoreBuffered
                    defer:NO];

  [window setIsHeadless:YES];
  ASSERT_TRUE([window isHeadless]);
  ASSERT_FALSE([window isZoomed]);

  [window performZoom:nil];
  EXPECT_TRUE([window isZoomed]);

  [window performZoom:nil];
  EXPECT_FALSE([window isZoomed]);
}

TEST_F(NativeWidgetMacNSWindowHeadlessTest, SwizzledWindowIsZoomable) {
  CreateTestScreen(gfx::Size(1600, 1200));

  NativeWidgetMacNSWindow* window = [[NativeWidgetMacNSWindow alloc]
      initWithContentRect:ui::kWindowSizeDeterminedLater
                styleMask:NSWindowStyleMaskResizable |
                          NSWindowStyleMaskClosable |
                          NSWindowStyleMaskMiniaturizable
                  backing:NSBackingStoreBuffered
                    defer:NO];

  // Make window headless to insall swizzlers.
  [window setIsHeadless:YES];
  ASSERT_TRUE([window isHeadless]);

  // Make window normal to enable fallback behavior in swizzlers.
  [window setIsHeadless:NO];
  ASSERT_FALSE([window isHeadless]);

  ASSERT_FALSE([window isZoomed]);

  [window setIsZoomed:YES];
  EXPECT_TRUE([window isZoomed]);
  [window setIsZoomed:NO];
  EXPECT_FALSE([window isZoomed]);
}

TEST_F(NativeWidgetMacNSWindowHeadlessTest, HeadlessWindowCanBeMiniaturized) {
  NativeWidgetMacNSWindow* window = [[NativeWidgetMacNSWindow alloc]
      initWithContentRect:ui::kWindowSizeDeterminedLater
                styleMask:NSWindowStyleMaskResizable |
                          NSWindowStyleMaskClosable |
                          NSWindowStyleMaskMiniaturizable
                  backing:NSBackingStoreBuffered
                    defer:NO];

  [window setIsHeadless:YES];
  ASSERT_TRUE([window isHeadless]);
  ASSERT_FALSE([window isMiniaturized]);

  [window miniaturize:nil];
  EXPECT_TRUE([window isMiniaturized]);

  [window deminiaturize:nil];
  EXPECT_FALSE([window isMiniaturized]);
}

TEST_F(NativeWidgetMacNSWindowHeadlessTest,
       HeadlessWindowCanPerformMiniaturize) {
  NativeWidgetMacNSWindow* window = [[NativeWidgetMacNSWindow alloc]
      initWithContentRect:ui::kWindowSizeDeterminedLater
                styleMask:NSWindowStyleMaskResizable |
                          NSWindowStyleMaskClosable |
                          NSWindowStyleMaskMiniaturizable
                  backing:NSBackingStoreBuffered
                    defer:NO];

  [window setIsHeadless:YES];
  ASSERT_TRUE([window isHeadless]);
  ASSERT_FALSE([window isMiniaturized]);

  [window performMiniaturize:nil];
  EXPECT_TRUE([window isMiniaturized]);

  [window performMiniaturize:nil];
  EXPECT_FALSE([window isMiniaturized]);
}

}  // namespace
