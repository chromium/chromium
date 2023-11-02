// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feature_engagement/internal/noop_display_lock_controller.h"

#include "components/feature_engagement/public/tracker.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace feature_engagement {

TEST(NoopDisplayLockControllerTest, ShouldNeverLetCallersAcquireLock) {
  NoopDisplayLockController controller;
  EXPECT_EQ(nullptr, controller.AcquireDisplayLock());
}

TEST(NoopDisplayLockControllerTest, DisplayShouldNeverBeLocked) {
  NoopDisplayLockController controller;
  EXPECT_FALSE(controller.IsDisplayLocked());
}

}  // namespace feature_engagement
