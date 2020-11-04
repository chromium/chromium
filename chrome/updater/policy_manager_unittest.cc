// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/policy_manager.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace updater {

TEST(PolicyManager, GetPolicyManager) {
  std::unique_ptr<PolicyManagerInterface> policy_manager(GetPolicyManager());
  ASSERT_TRUE(policy_manager->IsManaged());
}

}  // namespace updater
