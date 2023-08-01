// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/scoped_menu_bar_lock.h"

#import <AppKit/AppKit.h>

#include "testing/gtest/include/gtest/gtest.h"

// Verify that creating and tearing down a ScopedMenuBarLock doesn't crash.
TEST(ScopedMenuBarLockTest, CreateAndDestroy) {
  ScopedMenuBarLock menuBarLock;
}

// Verify that the API still exists.
TEST(ScopedMenuBarLockTest, PrivateAPIs) {
  EXPECT_TRUE([NSMenu instancesRespondToSelector:@selector(_lockMenuPosition)]);
  EXPECT_TRUE(
      [NSMenu instancesRespondToSelector:@selector(_unlockMenuPosition)]);
}
