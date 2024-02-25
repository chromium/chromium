// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/app/app_utils.h"

#include <string>
#include <vector>

#include "chrome/updater/constants.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace updater {

TEST(AppUtilsTest, BasicTests) {
  ASSERT_TRUE(ShouldUninstall({}, 50, true));
  ASSERT_FALSE(ShouldUninstall({kUpdaterAppId}, 5, false));
  ASSERT_TRUE(ShouldUninstall({kUpdaterAppId}, 50, false));
  ASSERT_TRUE(ShouldUninstall({kUpdaterAppId}, 5, true));
  ASSERT_FALSE(ShouldUninstall({kUpdaterAppId, "test1"}, 50, true));
  ASSERT_FALSE(ShouldUninstall({"test1"}, 50, true));
}

}  // namespace updater
