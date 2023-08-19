// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/update_service.h"

#include <string>

#include "base/version.h"
#include "components/update_client/update_client_errors.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace updater {

TEST(UpdateServiceTest, UpdateServiceState) {
  UpdateService::UpdateState state1;
  UpdateService::UpdateState state2 = state1;

  // Ignore version in the comparison, if both versions are not set.
  EXPECT_EQ(state1, state2);

  state1.next_version = base::Version("1.0");
  EXPECT_NE(state1, state2);

  state2.next_version = base::Version("1.0");
  EXPECT_EQ(state1, state2);

  state2.state = UpdateService::UpdateState::State::kUpdateError;
  EXPECT_NE(state1, state2);
  EXPECT_STREQ(::testing::PrintToString(state1).c_str(),
               "UpdateState {app_id: , state: unknown, next_version: 1.0, "
               "downloaded_bytes: -1, total_bytes: -1, install_progress: -1, "
               "error_category: none, error_code: 0, extra_code1: 0}");
}

}  // namespace updater
