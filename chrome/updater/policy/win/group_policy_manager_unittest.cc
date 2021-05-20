// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/policy/win/group_policy_manager.h"

#include <memory>

#include "base/win/registry.h"
#include "base/win/win_util.h"
#include "chrome/updater/win/constants.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace updater {

TEST(GroupPolicyManager, GetPolicyManager) {
  base::win::RegKey key(HKEY_LOCAL_MACHINE, UPDATER_POLICIES_KEY, KEY_READ);

  std::unique_ptr<PolicyManagerInterface> policy_manager(
      std::make_unique<GroupPolicyManager>());
  ASSERT_EQ(key.Valid() && base::win::IsEnrolledToDomain(),
            policy_manager->IsManaged());
}

}  // namespace updater
