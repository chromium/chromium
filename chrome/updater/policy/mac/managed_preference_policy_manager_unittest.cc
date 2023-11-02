// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/policy/mac/managed_preference_policy_manager.h"

#include <memory>

#include "testing/gtest/include/gtest/gtest.h"

namespace updater {

TEST(ManagedPreferencePolicyManagerTest, GetPolicyManager) {
  EXPECT_NE(CreateManagedPreferencePolicyManager().get(), nullptr);
}

}  // namespace updater
