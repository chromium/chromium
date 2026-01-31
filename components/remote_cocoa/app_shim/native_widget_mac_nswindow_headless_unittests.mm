// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/remote_cocoa/app_shim/native_widget_mac_nswindow_headless.h"

#include <AppKit/AppKit.h>

#include <memory>

#import "components/remote_cocoa/app_shim/native_widget_mac_nswindow.h"
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

  [window setIsHeadless:YES];
  ASSERT_TRUE([window isHeadless]);

  EXPECT_NE([window headlessInfo], nullptr);
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

}  // namespace
