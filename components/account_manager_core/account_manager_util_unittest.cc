// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/account_manager_core/account_manager_util.h"

#include "base/test/gtest_util.h"
#include "components/account_manager_core/account_addition_result.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace account_manager {

TEST(AccountManagerUtilDeathTest,
     ToMojoAccountAdditionResultDiesForRemoteDisconnectedStatus) {
  BASE_EXPECT_DEATH(
      ToMojoAccountAdditionResult(AccountAdditionResult::FromStatus(
          AccountAdditionResult::Status::kMojoRemoteDisconnected)),
      "These statuses should not be passed over the wire");
}

TEST(AccountManagerUtilDeathTest,
     ToMojoAccountAdditionResultDiesForIncompatibleMojoVersionsStatus) {
  BASE_EXPECT_DEATH(
      ToMojoAccountAdditionResult(AccountAdditionResult::FromStatus(
          AccountAdditionResult::Status::kIncompatibleMojoVersions)),
      "These statuses should not be passed over the wire");
}

}  // namespace account_manager
