// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/remote_cocoa/app_shim/immersive_mode_transparent_titlebar_controller_cocoa.h"

#import <AppKit/AppKit.h>

#include <memory>
#include <vector>

#include "base/functional/bind.h"
#import "components/remote_cocoa/app_shim/appkit_titlebar_private.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"
#include "ui/gfx/geometry/rect.h"

namespace remote_cocoa {
namespace {

NSView* GetTitlebarContainer(NSWindow* window) {
  NSView* view = [window standardWindowButton:NSWindowCloseButton];
  Class container_class = NSClassFromString(@"NSTitlebarContainerView");
  while (view && ![view isKindOfClass:container_class]) {
    view = view.superview;
  }
  return view;
}

class ImmersiveModeTransparentTitlebarControllerCocoaTest
    : public PlatformTest {
 public:
  void SetUp() override {
    PlatformTest::SetUp();
    // Configured like a browser window: content extends under a transparent
    // titlebar.
    window_ = [[NSWindow alloc]
        initWithContentRect:NSMakeRect(100, 100, 800, 600)
                  styleMask:NSWindowStyleMaskTitled |
                            NSWindowStyleMaskClosable |
                            NSWindowStyleMaskMiniaturizable |
                            NSWindowStyleMaskResizable |
                            NSWindowStyleMaskFullSizeContentView
                    backing:NSBackingStoreBuffered
                      defer:NO];
    window_.releasedWhenClosed = NO;
    window_.titlebarAppearsTransparent = YES;
    window_.titleVisibility = NSWindowTitleHidden;
  }

  void TearDown() override {
    controller_.reset();
    [window_ close];
    window_ = nil;
    PlatformTest::TearDown();
  }

  void CreateController() {
    controller_ =
        std::make_unique<ImmersiveModeTransparentTitlebarControllerCocoa>(
            window_, base::BindRepeating(
                         &ImmersiveModeTransparentTitlebarControllerCocoaTest::
                             OnTitlebarFrameChanged,
                         base::Unretained(this)));
    controller_->Init();
  }

  NSTitlebarAccessoryViewController* accessory() {
    return (NSTitlebarAccessoryViewController*)
        controller_->accessory_view_controller_for_testing();
  }

  CGFloat GetTitlebarHeight() {
    return NSHeight(
        [window_ standardWindowButton:NSWindowCloseButton].superview.frame);
  }

 protected:
  void OnTitlebarFrameChanged(const gfx::Rect& frame) {
    reported_frames_.push_back(frame);
  }

  NSWindow* __strong window_;
  std::unique_ptr<ImmersiveModeTransparentTitlebarControllerCocoa> controller_;
  std::vector<gfx::Rect> reported_frames_;
};

// The accessory is added below the titlebar without a separator, and both are
// undone when the controller goes away.
TEST_F(ImmersiveModeTransparentTitlebarControllerCocoaTest, InitAndTearDown) {
  window_.titlebarSeparatorStyle = NSTitlebarSeparatorStyleLine;
  CreateController();
  EXPECT_TRUE(controller_->is_initialized());

  NSTitlebarAccessoryViewController* accessory_controller = accessory();
  EXPECT_TRUE([window_.titlebarAccessoryViewControllers
      containsObject:accessory_controller]);
  EXPECT_EQ(NSLayoutAttributeBottom, accessory_controller.layoutAttribute);
  EXPECT_EQ(0, accessory_controller.fullScreenMinHeight);
  EXPECT_EQ(NSTitlebarSeparatorStyleNone, window_.titlebarSeparatorStyle);

  controller_.reset();
  EXPECT_FALSE([window_.titlebarAccessoryViewControllers
      containsObject:accessory_controller]);
  EXPECT_EQ(NSTitlebarSeparatorStyleLine, window_.titlebarSeparatorStyle);
}

// The accessory makes the titlebar as tall as top chrome.
TEST_F(ImmersiveModeTransparentTitlebarControllerCocoaTest, AccessoryHeight) {
  CreateController();
  NSView* accessory_view = accessory().view;
  const CGFloat titlebar_height = GetTitlebarHeight();
  ASSERT_GT(titlebar_height, 0);

  controller_->OnTopViewBoundsChanged(gfx::Rect(0, 0, 800, 100));
  EXPECT_EQ(100 - titlebar_height, NSHeight(accessory_view.frame));

  // Empty bounds (e.g. top chrome not laid out yet) are ignored.
  controller_->OnTopViewBoundsChanged(gfx::Rect());
  EXPECT_EQ(100 - titlebar_height, NSHeight(accessory_view.frame));

  // Top chrome shorter than the titlebar needs no accessory.
  controller_->OnTopViewBoundsChanged(gfx::Rect(0, 0, 800, 10));
  EXPECT_EQ(0, NSHeight(accessory_view.frame));
}

// Content fullscreen has no top chrome, so the accessory is hidden.
TEST_F(ImmersiveModeTransparentTitlebarControllerCocoaTest,
       ContentFullscreenHidesAccessory) {
  CreateController();
  controller_->UpdateToolbarVisibility(mojom::ToolbarVisibilityStyle::kNone);
  EXPECT_TRUE(controller_->IsContentFullscreen());
  EXPECT_TRUE(accessory().hidden);

  controller_->UpdateToolbarVisibility(
      mojom::ToolbarVisibilityStyle::kAutohide);
  EXPECT_FALSE(controller_->IsContentFullscreen());
  EXPECT_FALSE(accessory().hidden);
}

// The accessory never handles events, so that clicks reach top chrome in the
// browser window underneath the NSToolbarFullScreenWindow.
TEST_F(ImmersiveModeTransparentTitlebarControllerCocoaTest,
       AccessoryIsClickThrough) {
  CreateController();
  controller_->OnTopViewBoundsChanged(gfx::Rect(0, 0, 800, 100));
  NSView* accessory_view = accessory().view;
  EXPECT_EQ(nil, [accessory_view hitTest:NSMakePoint(10, 10)]);
}

// Outside of fullscreen there is no NSToolbarFullScreenWindow: the titlebar
// must not be made transparent and no titlebar frame is reported.
TEST_F(ImmersiveModeTransparentTitlebarControllerCocoaTest,
       NoTransparencyOutsideOfFullscreen) {
  NSView* container = GetTitlebarContainer(window_);
  if (![container
          respondsToSelector:@selector(transparencyAllowedInFullScreen)]) {
    GTEST_SKIP() << "No -[NSTitlebarContainerView "
                    "transparencyAllowedInFullScreen] on this macOS version.";
  }
  CreateController();
  controller_->FullscreenTransitionCompleted();
  EXPECT_FALSE(container.transparencyAllowedInFullScreen);
  EXPECT_EQ(nil, controller_->toolbar_window_for_testing());
  EXPECT_FALSE(controller_->last_reported_titlebar_frame_for_testing());
  EXPECT_TRUE(reported_frames_.empty());
}

// ResetTitlebarTransparency() clears a flag left on by a previous fullscreen
// session. Init() does the same.
TEST_F(ImmersiveModeTransparentTitlebarControllerCocoaTest,
       ResetTitlebarTransparency) {
  NSView* container = GetTitlebarContainer(window_);
  if (![container
          respondsToSelector:@selector(setTransparencyAllowedInFullScreen:)] ||
      ![container
          respondsToSelector:@selector(transparencyAllowedInFullScreen)]) {
    GTEST_SKIP() << "No -[NSTitlebarContainerView "
                    "setTransparencyAllowedInFullScreen:] on this macOS "
                    "version.";
  }

  container.transparencyAllowedInFullScreen = YES;
  ImmersiveModeTransparentTitlebarControllerCocoa::ResetTitlebarTransparency(
      window_);
  EXPECT_FALSE(container.transparencyAllowedInFullScreen);

  container.transparencyAllowedInFullScreen = YES;
  CreateController();
  EXPECT_FALSE(container.transparencyAllowedInFullScreen);
}

}  // namespace
}  // namespace remote_cocoa
