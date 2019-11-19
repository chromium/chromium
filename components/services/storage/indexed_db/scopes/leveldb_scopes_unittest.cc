// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/storage/indexed_db/scopes/leveldb_scopes.h"

#include <limits>
#include <utility>

#include "base/run_loop.h"
#include "base/test/bind_test_util.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "components/services/storage/indexed_db/scopes/disjoint_range_lock_manager.h"
#include "components/services/storage/indexed_db/scopes/leveldb_scopes.h"
#include "components/services/storage/indexed_db/scopes/leveldb_scopes_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/leveldatabase/src/include/leveldb/slice.h"

namespace content {
namespace {

class LevelDBScopesStartupTest : public LevelDBScopesTestBase {
 public:
  LevelDBScopesStartupTest() = default;
  ~LevelDBScopesStartupTest() override = default;
};

TEST_F(LevelDBScopesStartupTest, CleanupOnRecovery) {
  const int64_t kScopeToCleanUp = 19;
  SetUpRealDatabase();
  DisjointRangeLockManager lock_manager(3);
  WriteScopesMetadata(kScopeToCleanUp, true);

  leveldb::Status failure_callback = leveldb::Status::OK();
  LevelDBScopes scopes(
      metadata_prefix_, kWriteBatchSizeForTesting, leveldb_, &lock_manager,
      base::BindLambdaForTesting(
          [&failure_callback](leveldb::Status s) { failure_callback = s; }));

  leveldb::Status s = scopes.Initialize();
  EXPECT_TRUE(s.ok());
  scopes.StartRecoveryAndCleanupTasks(
      LevelDBScopes::TaskRunnerMode::kNewCleanupAndRevertSequences);

  // Wait until cleanup task runs.
  base::RunLoop loop;
  scopes.CleanupRunnerForTesting()->PostTask(FROM_HERE, loop.QuitClosure());
  loop.Run();

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
  DisjointRangeLockManager lock_manager(3);

  // Tests that the revert execution on startup is performed correctly. This
  // includes:
  // * The revert task is executed & a value is written to disk,
  // * The cleanup task is executed & deletes the scope metadata, but does not
  //   execute the cleanup tasks.

  metadata_buffer_.mutable_locks()->Add();
  metadata_buffer_.mutable_locks()->Mutable(0)->set_level(0);
  metadata_buffer_.mutable_locks()->Mutable(0)->mutable_range()->set_begin(
      simple_lock_begin_);
  metadata_buffer_.mutable_locks()->Mutable(0)->mutable_range()->set_end(
      simple_lock_end_);
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
  ScopesLocksHolder locks_receiver;
  lock_manager.AcquireLocks(
      {CreateSimpleExclusiveLock()}, locks_receiver.AsWeakPtr(),
      base::BindLambdaForTesting([&]() { lock_grabbed = true; }));

  scopes.StartRecoveryAndCleanupTasks(
      LevelDBScopes::TaskRunnerMode::kNewCleanupAndRevertSequences);

  EXPECT_FALSE(lock_grabbed);

  // Wait until revert runs.
  {
    base::RunLoop loop;
    scopes.RevertRunnerForTesting()->PostTask(FROM_HERE, loop.QuitClosure());
    loop.Run();
  }
  value_buffer_.clear();
  EXPECT_TRUE(leveldb_->db()
                  ->Get(leveldb::ReadOptions(), kUndoPutKey, &value_buffer_)
                  .ok());
  EXPECT_EQ(kUndoPutValue, value_buffer_);

  // Ensure the cleanup task was posted & locks were released.
  {
    base::RunLoop loop;
    base::SequencedTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                     loop.QuitClosure());
    loop.Run();
  }
  EXPECT_TRUE(lock_grabbed);

  // Wait until cleanup runs.
  {
    base::RunLoop loop;
    scopes.CleanupRunnerForTesting()->PostTask(FROM_HERE, loop.QuitClosure());
    loop.Run();
  }
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
}  // namespace content
