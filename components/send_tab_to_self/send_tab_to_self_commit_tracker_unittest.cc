// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/send_tab_to_self/send_tab_to_self_commit_tracker.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "components/send_tab_to_self/send_tab_to_self_model.h"
#include "components/sync/base/client_tag_hash.h"
#include "components/sync/base/data_type.h"
#include "components/sync/model/data_type_sync_bridge.h"
#include "components/sync/test/mock_data_type_local_change_processor.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace send_tab_to_self {
namespace {

using testing::Return;

class SendTabToSelfCommitTrackerTest : public testing::Test {
 protected:
  SendTabToSelfCommitTrackerTest() : tracker_(&mock_processor_) {}

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  syncer::MockDataTypeLocalChangeProcessor mock_processor_;
  SendTabToSelfCommitTracker tracker_;
};

// Verifies that success is notified when an entity becomes synced during an
// incremental update.
TEST_F(SendTabToSelfCommitTrackerTest, NotifySuccessOnIncrementalSync) {
  base::test::TestFuture<SendTabToSelfResult> future;
  tracker_.TrackCommit("guid1", future.GetCallback());

  // Still unsynced, should not fire.
  EXPECT_CALL(mock_processor_, IsEntityUnsynced("guid1"))
      .WillOnce(Return(true));
  tracker_.OnIncrementalSyncComplete();
  EXPECT_FALSE(future.IsReady());

  // Once synced, it should fire.
  EXPECT_CALL(mock_processor_, IsEntityUnsynced("guid1"))
      .WillOnce(Return(false));
  tracker_.OnIncrementalSyncComplete();
  EXPECT_EQ(future.Get(), SendTabToSelfResult::kSuccess);
}

// Verifies that a timeout error is notified if the commit takes too long.
TEST_F(SendTabToSelfCommitTrackerTest, NotifyTimeout) {
  base::test::TestFuture<SendTabToSelfResult> future;
  tracker_.TrackCommit("guid1", future.GetCallback());

  task_environment_.FastForwardBy(base::Seconds(3));
  EXPECT_EQ(future.Get(), SendTabToSelfResult::kFailureCommitTimeout);
}

// Verifies that a commit attempt error is notified when sync reports a specific
// entity error.
TEST_F(SendTabToSelfCommitTrackerTest, NotifyCommitError) {
  base::test::TestFuture<SendTabToSelfResult> future;
  tracker_.TrackCommit("guid1", future.GetCallback());

  syncer::FailedCommitResponseData error_data;
  error_data.client_tag_hash =
      syncer::ClientTagHash::FromUnhashed(syncer::SEND_TAB_TO_SELF, "guid1");

  tracker_.OnCommitErrors({error_data});
  EXPECT_EQ(future.Get(), SendTabToSelfResult::kFailureCommitAttemptError);
}

// Verifies that a commit failure is notified when the general sync commit
// attempt fails.
TEST_F(SendTabToSelfCommitTrackerTest, NotifyCommitAttemptFailed) {
  base::test::TestFuture<SendTabToSelfResult> future;
  tracker_.TrackCommit("guid1", future.GetCallback());

  tracker_.OnCommitAttemptFailed();
  EXPECT_EQ(future.Get(), SendTabToSelfResult::kFailureCommitAttemptFailed);
}

// Verifies that a sync disabled error is notified when sync is turned off.
TEST_F(SendTabToSelfCommitTrackerTest, NotifySyncDisabled) {
  base::test::TestFuture<SendTabToSelfResult> future;
  tracker_.TrackCommit("guid1", future.GetCallback());

  tracker_.OnSyncDisabled();
  EXPECT_EQ(future.Get(), SendTabToSelfResult::kFailureSyncDisabled);
}

// Verifies that an entry removed error is notified if the entry is deleted
// before commit.
TEST_F(SendTabToSelfCommitTrackerTest, NotifyEntryRemoved) {
  base::test::TestFuture<SendTabToSelfResult> future;
  tracker_.TrackCommit("guid1", future.GetCallback());

  tracker_.OnEntryRemoved("guid1");
  EXPECT_EQ(future.Get(), SendTabToSelfResult::kFailureEntryRemoved);
}

// Verifies that multiple tracked commits are handled independently without
// cross-talk.
TEST_F(SendTabToSelfCommitTrackerTest, MultipleCommitsTrackedIndependently) {
  base::test::TestFuture<SendTabToSelfResult> future1;
  base::test::TestFuture<SendTabToSelfResult> future2;

  tracker_.TrackCommit("guid1", future1.GetCallback());
  tracker_.TrackCommit("guid2", future2.GetCallback());

  // Removing guid1 should only trigger future1's failure callback.
  tracker_.OnEntryRemoved("guid1");
  EXPECT_EQ(future1.Get(), SendTabToSelfResult::kFailureEntryRemoved);
  EXPECT_FALSE(future2.IsReady());

  // Completing sync for guid2 should trigger future2's success callback.
  EXPECT_CALL(mock_processor_, IsEntityUnsynced("guid2"))
      .WillOnce(Return(false));
  tracker_.OnIncrementalSyncComplete();
  EXPECT_EQ(future2.Get(), SendTabToSelfResult::kSuccess);
}

// Verifies that a successful commit prevents subsequent timeout callbacks from
// being invoked.
TEST_F(SendTabToSelfCommitTrackerTest, TimeoutCancelledOnSuccess) {
  base::test::TestFuture<SendTabToSelfResult> future;
  tracker_.TrackCommit("guid1", future.GetCallback());

  // Simulate immediate sync success.
  EXPECT_CALL(mock_processor_, IsEntityUnsynced("guid1"))
      .WillOnce(Return(false));
  tracker_.OnIncrementalSyncComplete();
  EXPECT_EQ(future.Get(), SendTabToSelfResult::kSuccess);

  // Fast-forward past the 3-second timeout threshold.
  // This asserts that the delayed timeout task safely ignores the erased entry
  // without crashing or attempting to invoke stale callbacks.
  task_environment_.FastForwardBy(base::Seconds(3));
}

// Verifies that all tracked commits are notified of entry removal on bulk
// clear.
TEST_F(SendTabToSelfCommitTrackerTest, NotifyAllEntriesRemoved) {
  base::test::TestFuture<SendTabToSelfResult> future1;
  base::test::TestFuture<SendTabToSelfResult> future2;

  tracker_.TrackCommit("guid1", future1.GetCallback());
  tracker_.TrackCommit("guid2", future2.GetCallback());

  tracker_.OnAllEntriesRemoved();

  EXPECT_EQ(future1.Get(), SendTabToSelfResult::kFailureEntryRemoved);
  EXPECT_EQ(future2.Get(), SendTabToSelfResult::kFailureEntryRemoved);
}

// Verifies that tracking gracefully ignores null callbacks without initiating
// timers or crashing.
TEST_F(SendTabToSelfCommitTrackerTest, NullCallbackIgnored) {
  tracker_.TrackCommit("guid1", base::NullCallback());
  // Fast-forward past the timeout threshold to ensure no phantom timeout tasks
  // execute.
  task_environment_.FastForwardBy(base::Seconds(3));
}

}  // namespace
}  // namespace send_tab_to_self
