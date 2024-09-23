// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/remote_cocoa/app_shim/native_widget_mac_overlay_nswindow.h"

#include <AppKit/AppKit.h>

#include "base/containers/flat_map.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"
#include "ui/base/cocoa/window_size_constants.h"

using NativeWidgetMacOverlayNSWindowTest = PlatformTest;

// Test that only allowed collection behaviors are able to be set.
TEST(NativeWidgetMacOverlayNSWindowTest, CollectionBehavior) {
  NativeWidgetMacOverlayNSWindow* overlay_window =
      [[NativeWidgetMacOverlayNSWindow alloc]
          initWithContentRect:ui::kWindowSizeDeterminedLater
                    styleMask:NSWindowStyleMaskBorderless
                      backing:NSBackingStoreBuffered
                        defer:NO];

  // TODO: Convert to a `base::fixed_flat_map` once macOS 13 is the minimum
  // deployment target.
  base::flat_map<NSWindowCollectionBehavior, bool> behavior_tests({
      {NSWindowCollectionBehaviorCanJoinAllSpaces, false},
      {NSWindowCollectionBehaviorMoveToActiveSpace, false},
      {NSWindowCollectionBehaviorManaged, false},
      {NSWindowCollectionBehaviorTransient, true},  // Allowed
      {NSWindowCollectionBehaviorStationary, false},
      {NSWindowCollectionBehaviorParticipatesInCycle, false},
      {NSWindowCollectionBehaviorIgnoresCycle, true},  // Allowed
      {NSWindowCollectionBehaviorFullScreenPrimary, false},
      {NSWindowCollectionBehaviorFullScreenAuxiliary, false},
      {NSWindowCollectionBehaviorFullScreenNone, false},
      {NSWindowCollectionBehaviorFullScreenAllowsTiling, false},
      {NSWindowCollectionBehaviorFullScreenDisallowsTiling, false},
  });
  if (@available(macos 13.0, *)) {
    behavior_tests.insert({NSWindowCollectionBehaviorPrimary, false});
    behavior_tests.insert({NSWindowCollectionBehaviorAuxiliary, false});
    behavior_tests.insert(
        {NSWindowCollectionBehaviorCanJoinAllApplications, false});
  }

  // Ensure only the allowed bits are able to be set.
  ASSERT_EQ(overlay_window.collectionBehavior,
            NSWindowCollectionBehaviorDefault);
  for (auto const& [behavior, allowed] : behavior_tests) {
    overlay_window.collectionBehavior |= behavior;
    EXPECT_EQ(!!(overlay_window.collectionBehavior & behavior), allowed)
        << "NSWindowCollectionBehavior: " << behavior;
  }

  // Also test setting multiple bits at once.
  overlay_window.collectionBehavior = NSWindowCollectionBehaviorDefault;
  ASSERT_EQ(overlay_window.collectionBehavior,
            NSWindowCollectionBehaviorDefault);
  overlay_window.collectionBehavior = ~NSWindowCollectionBehaviorDefault;
  for (auto const& [behavior, allowed] : behavior_tests) {
    EXPECT_EQ(!!(overlay_window.collectionBehavior & behavior), allowed)
        << "NSWindowCollectionBehavior: " << behavior;
  }
}
