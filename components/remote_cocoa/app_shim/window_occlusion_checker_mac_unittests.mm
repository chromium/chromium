// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/remote_cocoa/app_shim/window_occlusion_checker_mac.h"

#import <AppKit/AppKit.h>

#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

namespace remote_cocoa {
namespace {

class WindowOcclusionCheckerMacTest : public PlatformTest {
 public:
  void SetUp() override {
    PlatformTest::SetUp();
    [WindowOcclusionCheckerMac resetSharedInstanceForTesting];
  }

  void TearDown() override {
    [WindowOcclusionCheckerMac resetSharedInstanceForTesting];
    PlatformTest::TearDown();
  }
};

TEST_F(WindowOcclusionCheckerMacTest, ReferenceCountedWindowTracking) {
  WindowOcclusionCheckerMac* checker =
      [WindowOcclusionCheckerMac sharedInstance];
  ASSERT_TRUE(checker);

  NSWindow* window =
      [[NSWindow alloc] initWithContentRect:NSMakeRect(0, 0, 100, 100)
                                  styleMask:NSWindowStyleMaskBorderless
                                    backing:NSBackingStoreBuffered
                                      defer:YES];
  window.releasedWhenClosed = NO;

  EXPECT_FALSE([checker isTrackingWindow:window]);

  // Register twice (e.g. NativeWidgetNSWindowBridge + WebContentsViewCocoa).
  [checker addTrackedWindow:window];
  EXPECT_TRUE([checker isTrackingWindow:window]);
  [checker addTrackedWindow:window];
  EXPECT_TRUE([checker isTrackingWindow:window]);

  // Removing one registration keeps the window tracked.
  [checker removeTrackedWindow:window];
  EXPECT_TRUE([checker isTrackingWindow:window]);

  // Removing the last registration stops tracking the window.
  [checker removeTrackedWindow:window];
  EXPECT_FALSE([checker isTrackingWindow:window]);

  // Extra removals and nil arguments are safe no-ops.
  [checker removeTrackedWindow:window];
  [checker addTrackedWindow:nil];
  [checker removeTrackedWindow:nil];
  EXPECT_FALSE([checker isTrackingWindow:nil]);
  EXPECT_FALSE([checker isTrackingWindow:window]);
}

TEST_F(WindowOcclusionCheckerMacTest, TrackedWindowHeldWeakly) {
  WindowOcclusionCheckerMac* checker =
      [WindowOcclusionCheckerMac sharedInstance];
  ASSERT_TRUE(checker);

  __weak NSWindow* weak_window = nil;
  @autoreleasepool {
    NSWindow* window =
        [[NSWindow alloc] initWithContentRect:NSMakeRect(0, 0, 100, 100)
                                    styleMask:NSWindowStyleMaskBorderless
                                      backing:NSBackingStoreBuffered
                                        defer:YES];
    window.releasedWhenClosed = NO;
    weak_window = window;

    [checker addTrackedWindow:window];
    EXPECT_TRUE([checker isTrackingWindow:window]);
  }

  // `NSMapTable weakToStrongObjectsMapTable` does not retain `window`.
  EXPECT_EQ(nil, weak_window);
}

TEST_F(WindowOcclusionCheckerMacTest, ResetCancelsScheduledUpdates) {
  EXPECT_EQ(nil, [WindowOcclusionCheckerMac sharedInstanceIfCreated]);
  WindowOcclusionCheckerMac* checker =
      [WindowOcclusionCheckerMac sharedInstance];
  ASSERT_TRUE(checker);
  EXPECT_EQ(checker, [WindowOcclusionCheckerMac sharedInstanceIfCreated]);
  [checker scheduleOcclusionStateUpdates];
  EXPECT_TRUE([checker occlusionStateUpdatesAreScheduledForTesting]);

  [WindowOcclusionCheckerMac resetSharedInstanceForTesting];
  EXPECT_EQ(nil, [WindowOcclusionCheckerMac sharedInstanceIfCreated]);
  // `-shutdown` cancelled the pending performSelector: request on the old
  // instance.
  EXPECT_FALSE([checker occlusionStateUpdatesAreScheduledForTesting]);

  WindowOcclusionCheckerMac* new_checker =
      [WindowOcclusionCheckerMac sharedInstance];
  ASSERT_TRUE(new_checker);
  EXPECT_NE(checker, new_checker);
  EXPECT_FALSE([new_checker occlusionStateUpdatesAreScheduledForTesting]);
}

}  // namespace
}  // namespace remote_cocoa
