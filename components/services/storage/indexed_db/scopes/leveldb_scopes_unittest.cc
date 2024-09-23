// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/storage/indexed_db/scopes/leveldb_scopes.h"

#include <limits>
#include <utility>

#include "base/run_loop.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/bind.h"
#include "components/services/storage/indexed_db/locks/partitioned_lock_manager.h"
#include "components/services/storage/indexed_db/scopes/leveldb_scopes_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/leveldatabase/src/include/leveldb/slice.h"

namespace content::indexed_db {
namespace {

class LevelDBScopesStartupTest : public LevelDBScopesTestBase {
 public:
  LevelDBScopesStartupTest() = default;
  ~LevelDBScopesStartupTest() override = default;
};

TEST_F(LevelDBScopesStartupTest, CleanupOnRecovery) {
  const int64_t kScopeToCleanUp = 19;
  SetUpRealDatabase();
  PartitionedLockManager lock_manager;
  WriteScopesMetadata(kScopeToCleanUp, true);

  leveldb::Status failure_callback = leveldb::Status::OK();
  LevelDBScopes scopes(
      metadata_prefix_, kWriteBatchSizeForTesting, leveldb_, &lock_manager,
      base::BindLambdaForTesting(
          [&failure_callback](leveldb::Status s) { failure_callback = s; }));

  leveldb::Status s = scopes.Initialize();
  EXPECT_TRUE(s.ok());
  scopes.StartRecoveryAndCleanupTasks();

  // Wait until cleanup task runs.
  task_env_.RunUntilIdle();

  EXPECT_TRUE(IsScopeCleanedUp(kScopeToCleanUp));
  EXPECT_FALSE(ScopeDataExistsOnDisk());

  EXPECT_TRUE(failure_callback.ok());
}

TEST_F(LevelDBScopesStartupTest, RevertWithLocksOnRecoveryWithNoCleanup) {
  const int64_t kScopeToResumeRevert = 19;
  const std::string kUndoPutKey = "b10";
  const std::string kUndoPutValue = "abc";
  const std::string kCleanupDeleteRangeBegin = "b1";
  const std::string kKeyWithinCleanupDeleteRange = "b2";
  const std::string kCleanupDeleteRangeEnd = "b3";
  SetUpRealDatabase();
  PartitionedLockManager lock_manager;

  // Tests that the revert execution on startup is performed correctly. This
  // includes:
  // * The revert task is executed & a value is written to disk,
  // * The cleanup task is executed & deletes the scope metadata, but does not
  //   execute the cleanup tasks.

  metadata_buffer_.mutable_locks()->Add();
  metadata_buffer_.mutable_locks()->Mutable(0)->set_partition(0);
  metadata_buffer_.mutable_locks()->Mutable(0)->mutable_key()->set_key(
      simple_lock_begin_);
  WriteScopesMetadata(kScopeToResumeRevert, false);

  // Cleanup task that will be ignored.
  cleanup_task_buffer_.mutable_delete_range()->set_begin(
      kCleanupDeleteRangeBegin);
  cleanup_task_buffer_.mutable_delete_range()->set_end(kCleanupDeleteRangeEnd);
  WriteCleanupTask(kScopeToResumeRevert, /*sequence_number=*/0);

  // Undo task that will be executed.
  int64_t undo_sequence_number = std::numeric_limits<int64_t>::max();
  undo_task_buffer_.mutable_put()->set_key(kUndoPutKey);
  undo_task_buffer_.mutable_put()->set_value(kUndoPutValue);
  WriteUndoTask(kScopeToResumeRevert, undo_sequence_number);

  // Entry which would be deleted by the cleanup task if it were run.
  WriteLargeValue(kKeyWithinCleanupDeleteRange);

  leveldb::Status failure_callback = leveldb::Status::OK();
  LevelDBScopes scopes(
      metadata_prefix_, kWriteBatchSizeForTesting, leveldb_, &lock_manager,
      base::BindLambdaForTesting(
          [&failure_callback](leveldb::Status s) { failure_callback = s; }));

  leveldb::Status s = scopes.Initialize();
  EXPECT_TRUE(s.ok());

  // Verify that the lock was grabbed.
  bool lock_grabbed = false;
  PartitionedLockHolder locks_receiver;
  lock_manager.AcquireLocks(
      {CreateSimpleExclusiveLock()}, locks_receiver,
      base::BindLambdaForTesting([&]() { lock_grabbed = true; }));

  scopes.StartRecoveryAndCleanupTasks();

  EXPECT_FALSE(lock_grabbed);

  // Wait until revert runs.
  base::RunLoop().RunUntilIdle();

  value_buffer_.clear();
  EXPECT_TRUE(leveldb_->db()
                  ->Get(leveldb::ReadOptions(), kUndoPutKey, &value_buffer_)
                  .ok());
  EXPECT_EQ(kUndoPutValue, value_buffer_);

  // Ensure the cleanup task was posted & locks were released.
  {
    base::RunLoop loop;
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, loop.QuitClosure());
    loop.Run();
  }
  EXPECT_TRUE(lock_grabbed);

  // Wait until cleanup runs.
  task_env_.RunUntilIdle();

  EXPECT_TRUE(IsScopeCleanedUp(kScopeToResumeRevert));
  EXPECT_FALSE(ScopeDataExistsOnDisk());

  value_buffer_.clear();
  EXPECT_TRUE(leveldb_->db()
                  ->Get(leveldb::ReadOptions(), kKeyWithinCleanupDeleteRange,
                        &value_buffer_)
                  .ok());

  EXPECT_TRUE(failure_callback.ok());
}

}  // namespace
}  // namespace content::indexed_db
