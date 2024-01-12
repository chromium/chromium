// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/policy/manager.h"

#include "base/memory/scoped_refptr.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace updater {

TEST(PolicyManager, GetDefaultValuesPolicyManager) {
  scoped_refptr<PolicyManagerInterface> policy_manager(
      GetDefaultValuesPolicyManager());
  ASSERT_TRUE(policy_manager->HasActiveDevicePolicies());
}

TEST(PolicyManager, UpdateSuppressedTimes) {
  UpdatesSuppressedTimes suppression;
  suppression.start_hour_ = 13;
  suppression.start_minute_ = 35;
  suppression.duration_minute_ = 22;
  ASSERT_FALSE(suppression.contains(12, 45));
  ASSERT_TRUE(suppression.contains(13, 35));
  ASSERT_TRUE(suppression.contains(13, 45));
  ASSERT_TRUE(suppression.contains(13, 55));
  ASSERT_FALSE(suppression.contains(13, 59));
  ASSERT_FALSE(suppression.contains(14, 45));

  suppression.start_hour_ = 21;
  suppression.start_minute_ = 0;
  suppression.duration_minute_ = 360;
  ASSERT_FALSE(suppression.contains(20, 30));
  ASSERT_TRUE(suppression.contains(21, 30));
  ASSERT_TRUE(suppression.contains(22, 30));
  ASSERT_TRUE(suppression.contains(23, 30));
  ASSERT_TRUE(suppression.contains(0, 30));
  ASSERT_TRUE(suppression.contains(1, 30));
  ASSERT_TRUE(suppression.contains(2, 30));
  ASSERT_FALSE(suppression.contains(3, 30));
}

}  // namespace updater
