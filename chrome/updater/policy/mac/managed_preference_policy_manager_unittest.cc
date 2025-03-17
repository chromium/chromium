// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <optional>

#include "chrome/updater/policy/platform_policy_manager.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace updater {

TEST(ManagedPreferencePolicyManagerTest, GetPolicyManager) {
  EXPECT_NE(CreatePlatformPolicyManager(std::nullopt).get(), nullptr);
}

}  // namespace updater
