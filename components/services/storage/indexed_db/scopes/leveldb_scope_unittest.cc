// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/storage/indexed_db/scopes/leveldb_scope.h"

#include <limits>
#include <optional>
#include <utility>

#include "base/containers/flat_set.h"
#include "base/functional/callback.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "components/services/storage/indexed_db/leveldb/fake_leveldb_factory.h"
#include "components/services/storage/indexed_db/locks/partitioned_lock_manager.h"
#include "components/services/storage/indexed_db/scopes/leveldb_scopes.h"
#include "components/services/storage/indexed_db/scopes/leveldb_scopes_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/leveldatabase/src/include/leveldb/slice.h"

namespace content::indexed_db {
namespace {

class LevelDBScopeTest : public LevelDBScopesTestBase {
 public:
  LevelDBScopeTest() = default;
  ~LevelDBScopeTest() override = default;

  std::vector<PartitionedLock> AcquireLocksSync(
      PartitionedLockManager* lock_manager,
      base::flat_set<PartitionedLockManager::PartitionedLockRequest>
          lock_requests) {
    base::RunLoop loop;
    PartitionedLockHolder locks_receiver;
    lock_manager->AcquireLocks(
        lock_requests, locks_receiver,
        base::BindLambdaForTesting([&loop]() { loop.Quit(); }));
    loop.Run();
    return std::move(locks_receiver.locks);
  }

  std::string CreateKey(int key_num) {
    return base::StrCat({base::NumberToString(db_prefix_[0]),
                         base::StringPrintf("%010d", key_num), "key"});
  }

  std::string CreateLargeValue(int num) {
    return base::StrCat(
        {base::StringPrintf("%05d", num), "value", large_string_});
  }

  void CommitAndWaitForCleanup(
      LevelDBScopes& scopes,
      std::unique_ptr<LevelDBScope> scope,
      base::OnceClosure on_commit_complete = base::OnceClosure()) {
    base::RunLoop cleanup_loop;
    leveldb::Status s = scopes.Commit(
        std::move(scope), /*sync_on_commit=*/false,
        std::move(on_commit_complete),
        base::BindLambdaForTesting([&cleanup_loop]() { cleanup_loop.Quit(); }));
    EXPECT_TRUE(s.ok());

    // Wait until the cleanup task completes.
    cleanup_loop.Run();
  }
};

TEST_F(LevelDBScopeTest, BasicUsage) {
  SetUpRealDatabase();
  PartitionedLockManager lock_manager;

  std::optional<leveldb::Status> failure_status;
  LevelDBScopes scopes(
      metadata_prefix_, kWriteBatchSizeForTesting, leveldb_, &lock_manager,
      base::BindLambdaForTesting(
          [&failure_status](leveldb::Status s) { failure_status = s; }));

  leveldb::Status s = scopes.Initialize();
  EXPECT_TRUE(s.ok());
  scopes.StartRecoveryAndCleanupTasks();
  auto scope = scopes.CreateScope(
      AcquireLocksSync(&lock_manager, {CreateSimpleExclusiveLock()}));

  std::string value = "12345";
  std::string key = CreateKey(0);
  s = scope->Put(key, value);
  EXPECT_TRUE(s.ok());
  // We don't expect a cleanup task to run since this is an in-memory scope.
  s = scopes.Commit(std::move(scope), /*sync_on_commit=*/false);
  EXPECT_TRUE(s.ok());
  EXPECT_FALSE(failure_status.has_value());

  leveldb::ReadOptions options;
  options.verify_checksums = true;
  std::string out;
  leveldb_->db()->Get(options, CreateKey(0), &out);
  EXPECT_EQ(out, value);
}

TEST_F(LevelDBScopeTest, InMemoryAbort) {
  SetUpRealDatabase();
  PartitionedLockManager lock_manager;

  std::optional<leveldb::Status> failure_status;
  LevelDBScopes scopes(
      metadata_prefix_, kWriteBatchSizeForTesting, leveldb_, &lock_manager,
      base::BindLambdaForTesting(
          [&failure_status](leveldb::Status s) { failure_status = s; }));

  leveldb::Status s = scopes.Initialize();
  EXPECT_TRUE(s.ok());
  scopes.StartRecoveryAndCleanupTasks();
  auto scope = scopes.CreateScope(
      AcquireLocksSync(&lock_manager, {CreateSimpleExclusiveLock()}));

  // This change is smaller than 1024 bytes so it should be in-memory.
  std::string value = "12345";
  std::string key = CreateKey(0);
  s = scope->Put(key, value);
  EXPECT_TRUE(s.ok());
  // We don't expect a cleanup task to run since this is an in-memory scope.
  s = scopes.Commit(std::move(scope), /*sync_on_commit=*/false);
  EXPECT_TRUE(s.ok());

  // Write over the value and abort.
  scope = scopes.CreateScope(
      AcquireLocksSync(&lock_manager, {CreateSimpleExclusiveLock()}));
  value = "55555";
  s = scope->Put(key, value);
  scope.reset();
  EXPECT_TRUE(s.ok());

  // Acquire the locks, which should mean that the revert is done.
  auto locks = AcquireLocksSync(&lock_manager, {CreateSimpleExclusiveLock()});
  leveldb::ReadOptions options;
  options.verify_checksums = true;
  s = leveldb_->db()->Get(options, key, &value);
  EXPECT_TRUE(s.ok());
  EXPECT_EQ("12345", value);
  EXPECT_FALSE(failure_status.has_value());
}

TEST_F(LevelDBScopeTest, AbortWithRevertTask) {
  SetUpRealDatabase();
  PartitionedLockManager lock_manager;

  std::optional<leveldb::Status> failure_status;
  LevelDBScopes scopes(
      metadata_prefix_, kWriteBatchSizeForTesting, leveldb_, &lock_manager,
      base::BindLambdaForTesting(
          [&failure_status](leveldb::Status s) { failure_status = s; }));

  leveldb::Status s = scopes.Initialize();
  EXPECT_TRUE(s.ok());
  scopes.StartRecoveryAndCleanupTasks();

  std::string value = "12345";
  leveldb::WriteOptions woptions;
  s = leveldb_->db()->Put(woptions, CreateKey(0), leveldb::Slice(value));

  auto scope = scopes.CreateScope(
      AcquireLocksSync(&lock_manager, {CreateSimpleExclusiveLock()}));

  // This makes sure the scope goes to disk and writes an undo-log. This forces
  // it to revert the changes.
  value = CreateLargeValue(1);
  for (int i = 0; i < 10; ++i) {
    std::string key = CreateKey(0);
    s = scope->Put(key, value);
    EXPECT_TRUE(s.ok());
  }
  scope.reset();

  // Acquire the locks, which should mean that the revert is done.
  auto locks = AcquireLocksSync(&lock_manager, {CreateSimpleExclusiveLock()});
  leveldb::ReadOptions options;
  options.verify_checksums = true;
  std::string key = CreateKey(0);
  s = leveldb_->db()->Get(options, key, &value);
  EXPECT_TRUE(s.ok());
  EXPECT_EQ("12345", value);
  EXPECT_FALSE(failure_status.has_value());
}

TEST_F(LevelDBScopeTest, ManyScopes) {
  SetUpRealDatabase();
  PartitionedLockManager lock_manager;
  std::optional<leveldb::Status> failure_status;
  LevelDBScopes scopes(
      metadata_prefix_, kWriteBatchSizeForTesting, leveldb_, &lock_manager,
      base::BindLambdaForTesting(
          [&failure_status](leveldb::Status s) { failure_status = s; }));

  leveldb::Status s = scopes.Initialize();
  EXPECT_TRUE(s.ok());
  scopes.StartRecoveryAndCleanupTasks();

  std::string value;
  for (int i = 0; i < 20; ++i) {
    std::string key = CreateKey(i);
    value = CreateLargeValue(i);
    auto scope = scopes.CreateScope(
        AcquireLocksSync(&lock_manager, {CreateExclusiveLock(i)}));
    s = scope->Put(key, value);
    EXPECT_TRUE(s.ok());
    s = scopes.Commit(std::move(scope), /*sync_on_commit=*/false);
    EXPECT_TRUE(s.ok());
  }

  // Wait until all the cleanup tasks complete.
  task_env_.RunUntilIdle();

  ScopesEncoder encoder;
  for (int i = 0; i < 20; ++i) {
    EXPECT_TRUE(
        IsPrefixedRangeEmptyInDB(encoder.TasksKeyPrefix(metadata_prefix_, i)));
  }
  EXPECT_TRUE(
      IsPrefixedRangeEmptyInDB(encoder.ScopeMetadataPrefix(metadata_prefix_)));

  EXPECT_FALSE(failure_status.has_value());
}

TEST_F(LevelDBScopeTest, DeleteRangeExclusive) {
  SetUpRealDatabase();
  PartitionedLockManager lock_manager;
  std::optional<leveldb::Status> failure_status;
  LevelDBScopes scopes(
      metadata_prefix_, kWriteBatchSizeForTesting, leveldb_, &lock_manager,
      base::BindLambdaForTesting(
          [&failure_status](leveldb::Status s) { failure_status = s; }));

  leveldb::Status s = scopes.Initialize();
  EXPECT_TRUE(s.ok());
  scopes.StartRecoveryAndCleanupTasks();

  // Create values for keys 0-20, inclusive.
  std::string value;
  auto scope = scopes.CreateScope(
      AcquireLocksSync(&lock_manager, {CreateSimpleExclusiveLock()}));
  for (int i = 0; i < 21; ++i) {
    std::string key = CreateKey(i);
    value = i % 2 == 0 ? CreateLargeValue(i) : "smallvalue";
    s = scope->Put(key, value);
    EXPECT_TRUE(s.ok());
  }
  CommitAndWaitForCleanup(scopes, std::move(scope));
  EXPECT_FALSE(failure_status.has_value());

  // Do a exclusive range delete, so we should not delete 20.
  scope = scopes.CreateScope(
      AcquireLocksSync(&lock_manager, {CreateSimpleExclusiveLock()}));
  s = scope->DeleteRange(
      CreateKey(0), CreateKey(20),
      LevelDBScopeDeletionMode::kImmediateWithRangeEndExclusive);
  EXPECT_TRUE(s.ok());
  CommitAndWaitForCleanup(scopes, std::move(scope));
  EXPECT_FALSE(failure_status.has_value());

  // Check that keys 0-20 (exclusive) are gone, but 20 still exists.
  auto locks = AcquireLocksSync(&lock_manager, {CreateSimpleSharedLock()});
  leveldb::ReadOptions options;
  options.verify_checksums = true;
  for (int i = 0; i < 20; ++i) {
    std::string key = CreateKey(i);
    s = leveldb_->db()->Get(options, key, &value);
    EXPECT_TRUE(s.IsNotFound()) << i;
  }
  {
    std::string key = CreateKey(20);
    s = leveldb_->db()->Get(options, key, &value);
    EXPECT_TRUE(s.ok());
    EXPECT_EQ(value, CreateLargeValue(20));
  }
  locks.clear();
}

TEST_F(LevelDBScopeTest, DeleteRangeInclusive) {
  SetUpRealDatabase();
  PartitionedLockManager lock_manager;
  std::optional<leveldb::Status> failure_status;
  LevelDBScopes scopes(
      metadata_prefix_, kWriteBatchSizeForTesting, leveldb_, &lock_manager,
      base::BindLambdaForTesting(
          [&failure_status](leveldb::Status s) { failure_status = s; }));

  leveldb::Status s = scopes.Initialize();
  EXPECT_TRUE(s.ok());
  scopes.StartRecoveryAndCleanupTasks();

  // Create values for keys 0-20, inclusive.
  std::string value;
  auto scope = scopes.CreateScope(
      AcquireLocksSync(&lock_manager, {CreateSimpleExclusiveLock()}));
  for (int i = 0; i < 21; ++i) {
    std::string key = CreateKey(i);
    value = i % 2 == 0 ? CreateLargeValue(i) : "smallvalue";
    s = scope->Put(key, value);
    EXPECT_TRUE(s.ok());
  }
  CommitAndWaitForCleanup(scopes, std::move(scope));
  EXPECT_FALSE(failure_status.has_value());

  // Do an inclusive delete range, so key 20 should be deleted.
  scope = scopes.CreateScope(
      AcquireLocksSync(&lock_manager, {CreateSimpleExclusiveLock()}));
  s = scope->DeleteRange(
      CreateKey(0), CreateKey(20),
      LevelDBScopeDeletionMode::kImmediateWithRangeEndInclusive);
  EXPECT_TRUE(s.ok());
  CommitAndWaitForCleanup(scopes, std::move(scope));
  EXPECT_FALSE(failure_status.has_value());

  // Check that keys 0-20 (inclusive) are gone, including 20.
  auto locks = AcquireLocksSync(&lock_manager, {CreateSimpleSharedLock()});
  leveldb::ReadOptions options;
  options.verify_checksums = true;
  for (int i = 0; i < 21; ++i) {
    std::string key = CreateKey(i);
    s = leveldb_->db()->Get(options, key, &value);
    EXPECT_TRUE(s.IsNotFound()) << i;
  }
  locks.clear();
}

TEST_F(LevelDBScopeTest, DeleteRangeDeferred) {
  SetUpRealDatabase();
  PartitionedLockManager lock_manager;
  std::optional<leveldb::Status> failure_status;
  LevelDBScopes scopes(
      metadata_prefix_, kWriteBatchSizeForTesting, leveldb_, &lock_manager,
      base::BindLambdaForTesting(
          [&failure_status](leveldb::Status s) { failure_status = s; }));

  leveldb::Status s = scopes.Initialize();
  EXPECT_TRUE(s.ok());
  scopes.StartRecoveryAndCleanupTasks();

  std::string value;
  auto scope = scopes.CreateScope(
      AcquireLocksSync(&lock_manager, {CreateSimpleExclusiveLock()}));
  for (int i = 0; i < 20; ++i) {
    std::string key = CreateKey(i);
    value = i % 2 == 0 ? CreateLargeValue(i) : "smallvalue";
    s = scope->Put(key, value);
    EXPECT_TRUE(s.ok());
  }
  CommitAndWaitForCleanup(scopes, std::move(scope));
  EXPECT_FALSE(failure_status.has_value());

  scope = scopes.CreateScope(
      AcquireLocksSync(&lock_manager, {CreateSimpleExclusiveLock()}));
  s = scope->DeleteRange(CreateKey(0), CreateKey(20),
                         LevelDBScopeDeletionMode::kDeferred);
  EXPECT_TRUE(s.ok());
  CommitAndWaitForCleanup(scopes, std::move(scope));
  EXPECT_FALSE(failure_status.has_value());

  // Be a good citizen and acquire read locks.
  auto locks = AcquireLocksSync(&lock_manager, {CreateSimpleSharedLock()});
  leveldb::ReadOptions options;
  options.verify_checksums = true;
  for (int i = 0; i < 20; ++i) {
    std::string key = CreateKey(i);
    s = leveldb_->db()->Get(options, key, &value);
    EXPECT_TRUE(s.IsNotFound()) << i;
  }
  EXPECT_FALSE(failure_status.has_value());
}

TEST_F(LevelDBScopeTest, DeleteRangeCompact) {
  SetUpRealDatabase();
  PartitionedLockManager lock_manager;
  std::optional<leveldb::Status> failure_status;
  LevelDBScopes scopes(
      metadata_prefix_, kWriteBatchSizeForTesting, leveldb_, &lock_manager,
      base::BindLambdaForTesting(
          [&failure_status](leveldb::Status s) { failure_status = s; }));

  leveldb::Status s = scopes.Initialize();
  EXPECT_TRUE(s.ok());
  scopes.StartRecoveryAndCleanupTasks();

  std::string value;
  auto scope = scopes.CreateScope(
      AcquireLocksSync(&lock_manager, {CreateSimpleExclusiveLock()}));
  for (int i = 0; i < 20; ++i) {
    std::string key = CreateKey(i);
    value = i % 2 == 0 ? CreateLargeValue(i) : "smallvalue";
    s = scope->Put(key, value);
    EXPECT_TRUE(s.ok());
  }
  CommitAndWaitForCleanup(scopes, std::move(scope));
  EXPECT_FALSE(failure_status.has_value());

  scope = scopes.CreateScope(
      AcquireLocksSync(&lock_manager, {CreateSimpleExclusiveLock()}));
  s = scope->DeleteRange(CreateKey(0), CreateKey(20),
                         LevelDBScopeDeletionMode::kDeferredWithCompaction);
  CommitAndWaitForCleanup(scopes, std::move(scope));
  EXPECT_FALSE(failure_status.has_value());

  // Be a good citizen and acquire read locks.
  auto locks = AcquireLocksSync(&lock_manager, {CreateSimpleSharedLock()});
  leveldb::ReadOptions options;
  options.verify_checksums = true;
  for (int i = 0; i < 20; ++i) {
    std::string key = CreateKey(i);
    s = leveldb_->db()->Get(options, key, &value);
    EXPECT_TRUE(s.IsNotFound()) << i;
  }
  EXPECT_FALSE(failure_status.has_value());
}

TEST_F(LevelDBScopeTest, RevertWithDeferredDelete) {
  SetUpRealDatabase();
  PartitionedLockManager lock_manager;
  std::optional<leveldb::Status> failure_status;
  LevelDBScopes scopes(
      metadata_prefix_, kWriteBatchSizeForTesting, leveldb_, &lock_manager,
      base::BindLambdaForTesting(
          [&failure_status](leveldb::Status s) { failure_status = s; }));
  leveldb::Status s = scopes.Initialize();
  EXPECT_TRUE(s.ok());
  scopes.StartRecoveryAndCleanupTasks();

  // This test makes sure that the cleanup scheduled after the revert doesn't
  // execute it's cleanup tasks.

  // Populate the database.
  std::string value;
  auto scope = scopes.CreateScope(
      AcquireLocksSync(&lock_manager, {CreateSimpleExclusiveLock()}));
  for (int i = 0; i < 20; ++i) {
    std::string key = CreateKey(i);
    value = i % 2 == 0 ? CreateLargeValue(i) : "smallvalue";
    s = scope->Put(key, value);
    EXPECT_TRUE(s.ok());
  }
  CommitAndWaitForCleanup(scopes, std::move(scope));
  EXPECT_FALSE(failure_status.has_value());

  // Do a deferred delete & a write large enough to make this a log-based scope,
  // and then revert it.
  scope = scopes.CreateScope(
      AcquireLocksSync(&lock_manager, {CreateSimpleExclusiveLock()}));
  value = CreateLargeValue(20);
  s = scope->Put(CreateKey(20), value);
  s = scope->DeleteRange(CreateKey(0), CreateKey(20),
                         LevelDBScopeDeletionMode::kDeferred);
  EXPECT_TRUE(s.ok());
  scope.reset();

  // Wait until cleanup task runs.
  task_env_.RunUntilIdle();
  EXPECT_FALSE(failure_status.has_value());

  // If the cleanup correctly ignored the tasks, then the values should still
  // exist.
  auto locks = AcquireLocksSync(&lock_manager, {CreateSimpleSharedLock()});
  leveldb::ReadOptions options;
  options.verify_checksums = true;
  for (int i = 0; i < 20; ++i) {
    std::string key = CreateKey(i);
    s = leveldb_->db()->Get(options, key, &value);
    EXPECT_TRUE(s.ok()) << i;
  }
  s = leveldb_->db()->Get(options, CreateKey(20), &value);
  EXPECT_TRUE(s.IsNotFound());
  EXPECT_FALSE(failure_status.has_value());
}

TEST_F(LevelDBScopeTest, BrokenDBForInitialize) {
  leveldb::Status error = leveldb::Status::IOError("test");
  leveldb_ = FakeLevelDBFactory::GetBrokenLevelDB(error, base::FilePath());
  PartitionedLockManager lock_manager;

  std::optional<leveldb::Status> failure_status;
  LevelDBScopes scopes(
      metadata_prefix_, kWriteBatchSizeForTesting, leveldb_, &lock_manager,
      base::BindLambdaForTesting(
          [&failure_status](leveldb::Status s) { failure_status = s; }));

  leveldb::Status s = scopes.Initialize();
  EXPECT_FALSE(s.ok());
  EXPECT_EQ(s.ToString(), error.ToString());
  EXPECT_FALSE(failure_status.has_value());
}

TEST_F(LevelDBScopeTest, BrokenDBForCommit) {
  base::OnceCallback<void(leveldb::Status)> break_db;
  SetUpBreakableDB(&break_db);
  PartitionedLockManager lock_manager;

  std::optional<leveldb::Status> failure_status;
  LevelDBScopes scopes(
      metadata_prefix_, kWriteBatchSizeForTesting, leveldb_, &lock_manager,
      base::BindLambdaForTesting(
          [&failure_status](leveldb::Status s) { failure_status = s; }));

  leveldb::Status s = scopes.Initialize();
  EXPECT_TRUE(s.ok());
  scopes.StartRecoveryAndCleanupTasks();
  auto scope = scopes.CreateScope(
      AcquireLocksSync(&lock_manager, {CreateSimpleExclusiveLock()}));

  leveldb::Status error = leveldb::Status::IOError("test");
  std::move(break_db).Run(error);
  std::string value = "12345";
  std::string key = CreateKey(0);
  s = scope->Put(key, value);
  EXPECT_TRUE(s.ok());
  s = scopes.Commit(std::move(scope), /*sync_on_commit=*/false);
  EXPECT_EQ(s.ToString(), error.ToString());
  EXPECT_FALSE(failure_status.has_value());
}

TEST_F(LevelDBScopeTest, BrokenDBForCleanup) {
  base::OnceCallback<void(leveldb::Status)> break_db;
  SetUpBreakableDB(&break_db);
  PartitionedLockManager lock_manager;

  std::optional<leveldb::Status> failure_status;
  LevelDBScopes scopes(
      metadata_prefix_, kWriteBatchSizeForTesting, leveldb_, &lock_manager,
      base::BindLambdaForTesting(
          [&failure_status](leveldb::Status s) { failure_status = s; }));

  leveldb::Status s = scopes.Initialize();
  EXPECT_TRUE(s.ok());
  scopes.StartRecoveryAndCleanupTasks();
  auto scope = scopes.CreateScope(
      AcquireLocksSync(&lock_manager, {CreateSimpleExclusiveLock()}));
  std::string value = CreateLargeValue(0);
  std::string key = CreateKey(0);
  s = scope->Put(key, value);
  EXPECT_TRUE(s.ok());

  leveldb::Status error = leveldb::Status::IOError("test");
  // Commit the scopes and break the DB after the commit completes but before
  // the cleanup task is scheduled.
  CommitAndWaitForCleanup(scopes, std::move(scope),
                          base::BindLambdaForTesting([&break_db, &error]() {
                            std::move(break_db).Run(error);
                          }));

  EXPECT_TRUE(failure_status.has_value());
  EXPECT_FALSE(failure_status->ok());
  EXPECT_EQ(failure_status->ToString(), error.ToString());
}

TEST_F(LevelDBScopeTest, BrokenDBForRevert) {
  base::OnceCallback<void(leveldb::Status)> break_db;
  SetUpBreakableDB(&break_db);
  PartitionedLockManager lock_manager;

  std::optional<leveldb::Status> failure_status;
  LevelDBScopes scopes(
      metadata_prefix_, kWriteBatchSizeForTesting, leveldb_, &lock_manager,
      base::BindLambdaForTesting(
          [&failure_status](leveldb::Status s) { failure_status = s; }));

  leveldb::Status s = scopes.Initialize();
  EXPECT_TRUE(s.ok());
  scopes.StartRecoveryAndCleanupTasks();
  auto scope = scopes.CreateScope(
      AcquireLocksSync(&lock_manager, {CreateSimpleExclusiveLock()}));

  leveldb::Status error = leveldb::Status::IOError("test");
  std::string value = CreateLargeValue(0);
  std::string key = CreateKey(0);
  s = scope->Put(key, value);
  EXPECT_TRUE(s.ok());
  std::move(break_db).Run(error);
  scope.reset();

  // Wait until revert task reports failure.
  task_env_.RunUntilIdle();

  EXPECT_TRUE(failure_status.has_value());
  EXPECT_FALSE(failure_status->ok());
  EXPECT_EQ(failure_status->ToString(), error.ToString());
}

TEST_F(LevelDBScopeTest, DeleteNonExistentRangeDoesNotWrite) {
  SetUpRealDatabase();
  PartitionedLockManager lock_manager;

  std::optional<leveldb::Status> failure_status;
  LevelDBScopes scopes(
      metadata_prefix_, kWriteBatchSizeForTesting, leveldb_, &lock_manager,
      base::BindLambdaForTesting(
          [&failure_status](leveldb::Status s) { failure_status = s; }));

  leveldb::Status s = scopes.Initialize();
  EXPECT_TRUE(s.ok());
  scopes.StartRecoveryAndCleanupTasks();

  auto scope = scopes.CreateScope(
      AcquireLocksSync(&lock_manager, {CreateSimpleExclusiveLock()}));

  s = scope->DeleteRange(
      "b1", "b2", LevelDBScopeDeletionMode::kImmediateWithRangeEndInclusive);
  EXPECT_TRUE(s.ok());
  // We don't expect a cleanup task to run since this is an in-memory scope.
  s = scopes.Commit(std::move(scope), /*sync_on_commit=*/false);
  EXPECT_TRUE(s.ok());
  EXPECT_FALSE(failure_status.has_value());
}

}  // namespace
}  // namespace content::indexed_db
