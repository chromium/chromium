// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feature_engagement/internal/display_lock_controller_impl.h"

#include <memory>

#include "testing/gtest/include/gtest/gtest.h"

namespace feature_engagement {

TEST(DisplayLockControllerImplTest, DisplayIsUnlockedByDefault) {
  DisplayLockControllerImpl controller;
  EXPECT_FALSE(controller.IsDisplayLocked());
}

TEST(DisplayLockControllerImplTest, DisplayIsLockedWhileOutstandingHandle) {
  DisplayLockControllerImpl controller;
  EXPECT_FALSE(controller.IsDisplayLocked());
  std::unique_ptr<DisplayLockHandle> lock_handle =
      controller.AcquireDisplayLock();
  EXPECT_TRUE(controller.IsDisplayLocked());
  lock_handle.reset();
  EXPECT_FALSE(controller.IsDisplayLocked());
}

TEST(DisplayLockControllerImplTest,
     DisplayIsLockedWhileMultipleOutstandingHandles) {
  DisplayLockControllerImpl controller;
  std::unique_ptr<DisplayLockHandle> lock_handle1 =
      controller.AcquireDisplayLock();
  std::unique_ptr<DisplayLockHandle> lock_handle2 =
      controller.AcquireDisplayLock();
  EXPECT_TRUE(controller.IsDisplayLocked());

  // Releasing only 1 handle should keep the display locked.
  lock_handle1.reset();
  EXPECT_TRUE(controller.IsDisplayLocked());

  // When both handles are released, display should not be locked.
  lock_handle2.reset();
  EXPECT_FALSE(controller.IsDisplayLocked());
}

}  // namespace feature_engagement
