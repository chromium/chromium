// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/engine/cycle/sync_cycle.h"
#include "net/http/http_status_code.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {

class StatusControllerTest : public testing::Test {};

// This test is useful, as simple as it sounds, due to the copy-paste prone
// nature of status_controller.cc (we have had bugs in the past where a set_foo
// method was actually setting |bar_| instead!).
TEST_F(StatusControllerTest, ReadYourWrites) {
  StatusController status;

  status.set_last_download_updates_result(SyncerError::Success());
  EXPECT_EQ(status.model_neutral_state().last_download_updates_result.type(),
            SyncerError::Type::kSuccess);

  status.set_commit_result(SyncerError::HttpError(net::HTTP_UNAUTHORIZED));
  EXPECT_EQ(status.model_neutral_state().commit_result.type(),
            SyncerError::Type::kHttpError);

  for (int i = 0; i < 14; i++) {
    status.increment_num_successful_commits();
  }
  EXPECT_EQ(14, status.model_neutral_state().num_successful_commits);
}

// Test TotalNumConflictingItems
TEST_F(StatusControllerTest, TotalNumConflictingItems) {
  StatusController status;
  EXPECT_EQ(0, status.TotalNumConflictingItems());

  status.increment_num_server_conflicts();
  EXPECT_EQ(1, status.TotalNumConflictingItems());
}

}  // namespace syncer
