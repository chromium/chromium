// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/account_manager_core/account_manager_util.h"

#include "base/test/gtest_util.h"
#include "components/account_manager_core/account_upsertion_result.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace account_manager {

TEST(AccountManagerUtilDeathTest,
     ToMojoAccountUpsertionResultDiesForRemoteDisconnectedStatus) {
  EXPECT_DCHECK_DEATH(
      ToMojoAccountUpsertionResult(AccountUpsertionResult::FromStatus(
          AccountUpsertionResult::Status::kMojoRemoteDisconnected)));
}

TEST(AccountManagerUtilDeathTest,
     ToMojoAccountUpsertionResultDiesForIncompatibleMojoVersionsStatus) {
  EXPECT_DCHECK_DEATH(
      ToMojoAccountUpsertionResult(AccountUpsertionResult::FromStatus(
          AccountUpsertionResult::Status::kIncompatibleMojoVersions)));
}

}  // namespace account_manager
