// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/storage/indexed_db/scopes/leveldb_scopes_tasks.h"

#include <limits>

#include "base/functional/callback_helpers.h"
#include "base/strings/stringprintf.h"
#include "components/services/storage/indexed_db/scopes/leveldb_scopes_coding.h"
#include "components/services/storage/indexed_db/scopes/leveldb_scopes_test_utils.h"
#include "components/services/storage/indexed_db/scopes/scopes_metadata.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content::indexed_db {
namespace {

class LevelDBScopesTasksTest : public LevelDBScopesTestBase {
 public:
  LevelDBScopesTasksTest() = default;
  ~LevelDBScopesTasksTest() override = default;

  // Creates a basic scope scenario. It consists of:
  // * A single cleanup task deleting the range |delete_range_start_key_|
  //   (inclusive) to |delete_range_end_key_| (exclusive),
  // * Undo tasks that
  //   1. puts the value "abc" into the |outside_delete_range_key_| key,
  //   2. deletes the key |key_to_revert_by_delete_|, and
  //   3. deletes the a range that includes |key_to_revert_by_delete_range_|.
  // * Large values written to the keys |delete_range_start_key_|,
  //   |inside_delete_range_key_|, |delete_range_end_key_|,
  //   |key_to_revert_by_delete_|, and |key_to_revert_by_delete_range_|
  void CreateBasicScopeScenario(int64_t scope_number,
                                bool ignore_cleanup_tasks) {
    WriteScopesMetadata(scope_number, ignore_cleanup_tasks);

    cleanup_task_buffer_.mutable_delete_range()->set_begin(
        delete_range_start_key_);
    cleanup_task_buffer_.mutable_delete_range()->set_end(delete_range_end_key_);
    WriteCleanupTask(scope_number, /*sequence_number=*/0);

    int64_t undo_sequence_number = leveldb_scopes::kFirstSequenceNumberToWrite;
    undo_task_buffer_.mutable_put()->set_key(outside_delete_range_key_);
    undo_task_buffer_.mutable_put()->set_value(undo_value_to_write_);
    WriteUndoTask(scope_number, undo_sequence_number--);

    undo_task_buffer_.mutable_delete_()->set_key(key_to_revert_by_delete_);
    WriteUndoTask(scope_number, undo_sequence_number--);

    undo_task_buffer_.mutable_delete_range()->set_begin("b6");
    undo_task_buffer_.mutable_delete_range()->set_end("b8");
    WriteUndoTask(scope_number, undo_sequence_number--);

    WriteLargeValue(delete_range_start_key_);
    WriteLargeValue(inside_delete_range_key_);
    WriteLargeValue(delete_range_end_key_);
    WriteLargeValue(key_to_revert_by_delete_);
    WriteLargeValue(key_to_revert_by_delete_range_);
  }

 protected:
  // Keys & data used by |CreateBasicScopeScenario|.
  const std::string undo_value_to_write_ = "abc";
  const std::string delete_range_start_key_ = "b1";
  const std::string inside_delete_range_key_ = "b2";
  const std::string delete_range_end_key_ = "b3";
  const std::string outside_delete_range_key_ = "b4";
  const std::string key_to_revert_by_delete_ = "b5";
  const std::string key_to_revert_by_delete_range_ = "b7";
};

TEST_F(LevelDBScopesTasksTest, CleanupExecutesTasks) {
  const int64_t kScopeNumber = 1;
  SetUpRealDatabase();

  // This tests that the cleanup tasks are executed when the mode is
  // kExecuteCleanupTasks.
  CreateBasicScopeScenario(kScopeNumber, /*ignore_cleanup_tasks=*/false);

  CleanupScopeTask task(leveldb_, metadata_prefix_, kScopeNumber,
                        CleanupScopeTask::CleanupMode::kExecuteCleanupTasks,
                        kWriteBatchSizeForTesting);
  leveldb::Status s = task.Run();
  ASSERT_TRUE(s.ok()) << s.ToString();
  EXPECT_TRUE(LoadAt(delete_range_start_key_).IsNotFound());
  EXPECT_TRUE(LoadAt(inside_delete_range_key_).IsNotFound());
  EXPECT_TRUE(LoadAt(delete_range_end_key_).ok());
}

TEST_F(LevelDBScopesTasksTest, CleanupDeletesAllScopeKeys) {
  const int64_t kScopeNumber = 1;
  SetUpRealDatabase();

  // This tests that everything in the scope is cleaned up by the cleanup task.
  CreateBasicScopeScenario(kScopeNumber, /*ignore_cleanup_tasks=*/false);

  CleanupScopeTask task(leveldb_, metadata_prefix_, kScopeNumber,
                        CleanupScopeTask::CleanupMode::kExecuteCleanupTasks,
                        kWriteBatchSizeForTesting);
  leveldb::Status s = task.Run();
  ASSERT_TRUE(s.ok()) << s.ToString();

  EXPECT_TRUE(IsScopeCleanedUp(kScopeNumber));
  EXPECT_FALSE(ScopeDataExistsOnDisk());
}

TEST_F(LevelDBScopesTasksTest, CleanupIgnoresTasks) {
  const int64_t kScopeNumber = 1;
  SetUpRealDatabase();

  // This tests that the cleanup tasks are NOT executed when the mode is
  // kIgnoreCleanupTasks.
  CreateBasicScopeScenario(kScopeNumber, /*ignore_cleanup_tasks=*/true);

  CleanupScopeTask task(leveldb_, metadata_prefix_, kScopeNumber,
                        CleanupScopeTask::CleanupMode::kIgnoreCleanupTasks,
                        kWriteBatchSizeForTesting);
  leveldb::Status s = task.Run();
  ASSERT_TRUE(s.ok()) << s.ToString();
  EXPECT_TRUE(LoadAt(delete_range_start_key_).ok());
  EXPECT_TRUE(LoadAt(inside_delete_range_key_).ok());
  EXPECT_TRUE(LoadAt(delete_range_end_key_).ok());

  EXPECT_TRUE(IsScopeCleanedUp(kScopeNumber));
  EXPECT_FALSE(ScopeDataExistsOnDisk());
}

TEST_F(LevelDBScopesTasksTest, CleanupAbortsOnDestructionRequested) {
  const int64_t kScopeNumber = 1;
  SetUpRealDatabase();

  // This tests that the cleanup task aborts when it detects that destruction
  // was requested.
  CreateBasicScopeScenario(kScopeNumber, /*ignore_cleanup_tasks=*/false);

  CleanupScopeTask task(leveldb_, metadata_prefix_, kScopeNumber,
                        CleanupScopeTask::CleanupMode::kExecuteCleanupTasks,
                        kWriteBatchSizeForTesting);

  leveldb_->RequestDestruction(&leveldb_close_event_);
  leveldb::Status s = task.Run();
  ASSERT_TRUE(s.ok()) << s.ToString();
  EXPECT_TRUE(LoadAt(delete_range_start_key_).ok());
  EXPECT_TRUE(LoadAt(inside_delete_range_key_).ok());
  EXPECT_TRUE(LoadAt(delete_range_end_key_).ok());
  EXPECT_TRUE(LoadScopeMetadata(kScopeNumber).ok());
  EXPECT_TRUE(LoadCleanupTask(kScopeNumber, /*sequence_number=*/0).ok());
  EXPECT_TRUE(
      LoadUndoTask(kScopeNumber, leveldb_scopes::kFirstSequenceNumberToWrite)
          .ok());
  EXPECT_TRUE(LoadUndoTask(kScopeNumber,
                           leveldb_scopes::kFirstSequenceNumberToWrite - 1)
                  .ok());
  EXPECT_TRUE(LoadUndoTask(kScopeNumber,
                           leveldb_scopes::kFirstSequenceNumberToWrite - 2)
                  .ok());
}

TEST_F(LevelDBScopesTasksTest, RevertExecutesTasks) {
  const int64_t kScopeNumber = 1;
  SetUpRealDatabase();

  // This tests that the revert task aborts when it detects that destruction
  // was requested.
  CreateBasicScopeScenario(kScopeNumber, /*ignore_cleanup_tasks=*/false);

  RevertScopeTask task(leveldb_, metadata_prefix_, kScopeNumber,
                       kWriteBatchSizeForTesting);

  leveldb::Status s = task.Run();
  ASSERT_TRUE(s.ok()) << s.ToString();
  EXPECT_TRUE(LoadAt(outside_delete_range_key_).ok());
  EXPECT_EQ(value_buffer_, undo_value_to_write_);
  EXPECT_TRUE(LoadAt(key_to_revert_by_delete_).IsNotFound());
  EXPECT_TRUE(LoadAt(key_to_revert_by_delete_range_).IsNotFound());
}

TEST_F(LevelDBScopesTasksTest, RevertOnlyDeletesUndoLog) {
  const int64_t kScopeNumber = 1;
  SetUpRealDatabase();

  // This tests that the cleanup task aborts when it detects that destruction
  // was requested.
  CreateBasicScopeScenario(kScopeNumber, /*ignore_cleanup_tasks=*/false);

  RevertScopeTask task(leveldb_, metadata_prefix_, kScopeNumber,
                       kWriteBatchSizeForTesting);

  leveldb::Status s = task.Run();
  ASSERT_TRUE(s.ok()) << s.ToString();
  EXPECT_TRUE(LoadScopeMetadata(kScopeNumber).ok());
  EXPECT_TRUE(LoadCleanupTask(kScopeNumber, /*sequence_number=*/0).ok());
  EXPECT_TRUE(
      LoadUndoTask(kScopeNumber, leveldb_scopes::kFirstSequenceNumberToWrite)
          .IsNotFound());
  EXPECT_TRUE(LoadUndoTask(kScopeNumber,
                           leveldb_scopes::kFirstSequenceNumberToWrite - 1)
                  .IsNotFound());
  EXPECT_TRUE(LoadUndoTask(kScopeNumber,
                           leveldb_scopes::kFirstSequenceNumberToWrite - 2)
                  .IsNotFound());
  // Double check using ranges.s
  EXPECT_TRUE(IsPrefixedRangeEmptyInDB(
      scopes_encoder_.UndoTaskKeyPrefix(metadata_prefix_, kScopeNumber)));
  EXPECT_FALSE(IsPrefixedRangeEmptyInDB(
      scopes_encoder_.CleanupTaskKeyPrefix(metadata_prefix_, kScopeNumber)));
}

TEST_F(LevelDBScopesTasksTest, RevertAndCleanup) {
  const int64_t kScopeNumber = 1;
  SetUpRealDatabase();

  // This tests that the cleanup task aborts when it detects that destruction
  // was requested.
  CreateBasicScopeScenario(kScopeNumber, /*ignore_cleanup_tasks=*/false);

  // Undo task which will be executed and deleted.
  undo_task_buffer_.mutable_put()->set_key(outside_delete_range_key_);
  undo_task_buffer_.mutable_put()->set_value("abc");
  WriteUndoTask(kScopeNumber,
                /*sequence_number=*/std::numeric_limits<int64_t>::max());

  RevertScopeTask task(leveldb_, metadata_prefix_, kScopeNumber,
                       kWriteBatchSizeForTesting);

  leveldb::Status s = task.Run();
  ASSERT_TRUE(s.ok()) << s.ToString();

  CleanupScopeTask cleanup_task(
      leveldb_, metadata_prefix_, kScopeNumber,
      CleanupScopeTask::CleanupMode::kIgnoreCleanupTasks,
      kWriteBatchSizeForTesting);

  s = cleanup_task.Run();
  ASSERT_TRUE(s.ok()) << s.ToString();

  EXPECT_TRUE(LoadScopeMetadata(kScopeNumber).IsNotFound());
  EXPECT_TRUE(
      LoadCleanupTask(kScopeNumber, /*sequence_number=*/0).IsNotFound());
  EXPECT_TRUE(
      LoadUndoTask(kScopeNumber, leveldb_scopes::kFirstSequenceNumberToWrite)
          .IsNotFound());
  EXPECT_TRUE(LoadUndoTask(kScopeNumber,
                           leveldb_scopes::kFirstSequenceNumberToWrite - 1)
                  .IsNotFound());
  EXPECT_TRUE(LoadUndoTask(kScopeNumber,
                           leveldb_scopes::kFirstSequenceNumberToWrite - 2)
                  .IsNotFound());
  EXPECT_TRUE(LoadAt(delete_range_start_key_).ok());
  EXPECT_TRUE(LoadAt(inside_delete_range_key_).ok());
  EXPECT_TRUE(LoadAt(delete_range_end_key_).ok());
  EXPECT_TRUE(LoadAt(outside_delete_range_key_).ok());
  EXPECT_TRUE(LoadAt(key_to_revert_by_delete_).IsNotFound());
  EXPECT_TRUE(LoadAt(key_to_revert_by_delete_range_).IsNotFound());
  // Finally, double check with ranges.
  EXPECT_TRUE(IsPrefixedRangeEmptyInDB(
      scopes_encoder_.UndoTaskKeyPrefix(metadata_prefix_, kScopeNumber)));
}

TEST_F(LevelDBScopesTasksTest, ErrorsDuringCleanupArePropagated) {
  const int64_t kScopeNumber = 1;

  // This test will fail if the pattern of leveldb operations in this test
  // harness or the RevertScopeTask changes. Update the following two variables
  // to tune the number of expected operations for both.
  const int64_t kNumOpsBeforeFlakesStart = 10;
#if DCHECK_IS_ON()
  // The debug version of the CleanupScopeTask does an extra read.
  const int64_t kNumFlakeOps = 12;
#else
  const int64_t kNumFlakeOps = 11;
#endif

  const int64_t kNumOpsBeforeFlakesStop =
      kNumOpsBeforeFlakesStart + kNumFlakeOps;
  for (int i = kNumOpsBeforeFlakesStart; i < kNumOpsBeforeFlakesStop + 1; ++i) {
    FakeLevelDBFactory::FlakePoint flake = {
        i, leveldb::Status::IOError(base::StringPrintf("io error %d", i)), ""};

    SetUpFlakyDB(std::queue<FakeLevelDBFactory::FlakePoint>({flake}));
    CreateBasicScopeScenario(kScopeNumber, /*ignore_cleanup_tasks=*/false);

    {
      CleanupScopeTask task(leveldb_, metadata_prefix_, kScopeNumber,
                            CleanupScopeTask::CleanupMode::kExecuteCleanupTasks,
                            kWriteBatchSizeForTesting);
      leveldb::Status s = task.Run();
      if (i < kNumOpsBeforeFlakesStop)
        ASSERT_EQ(s.ToString(), flake.flake_status.ToString());
      else
        ASSERT_TRUE(s.ok()) << i;
    }
    TearDown();
  }
}

TEST_F(LevelDBScopesTasksTest, ErrorsDuringRevertArePropagated) {
  const int64_t kScopeNumber = 1;

  // This test will fail if the pattern of leveldb operations in this test
  // harness or the RevertScopeTask changes. Update the following two variables
  // to tune the number of expected operations for both.
  const int64_t kNumOpsBeforeFlakesStart = 10;
  const int64_t kNumFlakeOps = 8;

  const int64_t kNumOpsBeforeFlakesStop =
      kNumOpsBeforeFlakesStart + kNumFlakeOps;
  for (int i = kNumOpsBeforeFlakesStart; i < kNumOpsBeforeFlakesStop + 1; ++i) {
    FakeLevelDBFactory::FlakePoint flake = {
        i, leveldb::Status::IOError(base::StringPrintf("io error %d", i)), ""};

    SetUpFlakyDB(std::queue<FakeLevelDBFactory::FlakePoint>({flake}));
    // This tests that the cleanup tasks are executed when the mode is
    // kExecuteCleanupTasks.
    CreateBasicScopeScenario(kScopeNumber, /*ignore_cleanup_tasks=*/false);

    {
      RevertScopeTask task(leveldb_, metadata_prefix_, kScopeNumber,
                           kWriteBatchSizeForTesting);
      leveldb::Status s = task.Run();
      if (i < kNumOpsBeforeFlakesStop)
        ASSERT_EQ(s.ToString(), flake.flake_status.ToString());
      else
        ASSERT_TRUE(s.ok());
    }
    TearDown();
  }
}

}  // namespace
}  // namespace content::indexed_db
