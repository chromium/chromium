// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/storage/indexed_db/scopes/leveldb_scopes.h"

#include <limits>
#include <utility>

#include "base/containers/flat_set.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind_test_util.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "components/services/storage/indexed_db/leveldb/fake_leveldb_factory.h"
#include "components/services/storage/indexed_db/scopes/disjoint_range_lock_manager.h"
#include "components/services/storage/indexed_db/scopes/leveldb_scope.h"
#include "components/services/storage/indexed_db/scopes/leveldb_scopes.h"
#include "components/services/storage/indexed_db/scopes/leveldb_scopes_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/leveldatabase/src/include/leveldb/slice.h"

namespace content {
namespace {

class LevelDBScopeTest : public LevelDBScopesTestBase {
 public:
  LevelDBScopeTest() = default;
  ~LevelDBScopeTest() override = default;

  std::vector<ScopeLock> AcquireLocksSync(
      ScopesLockManager* lock_manager,
      base::flat_set<ScopesLockManager::ScopeLockRequest> lock_requests) {
    base::RunLoop loop;
    ScopesLocksHolder locks_receiver;
    bool success = lock_manager->AcquireLocks(
        lock_requests, locks_receiver.AsWeakPtr(),
        base::BindLambdaForTesting([&loop]() { loop.Quit(); }));
    EXPECT_TRUE(success);
    if (success)
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
};

TEST_F(LevelDBScopeTest, BasicUsage) {
  SetUpRealDatabase();
  DisjointRangeLockManager lock_manager(3);

  leveldb::Status failure_status = leveldb::Status::OK();
  LevelDBScopes scopes(
      metadata_prefix_, kWriteBatchSizeForTesting, leveldb_, &lock_manager,
      base::BindLambdaForTesting(
          [&failure_status](leveldb::Status s) { failure_status = s; }));

  leveldb::Status s = scopes.Initialize();
  EXPECT_TRUE(s.ok());
  scopes.StartRecoveryAndCleanupTasks(
      LevelDBScopes::TaskRunnerMode::kNewCleanupAndRevertSequences);
  auto scope = scopes.CreateScope(
      AcquireLocksSync(&lock_manager, {CreateSimpleExclusiveLock()}), {});

  std::string value = "12345";
  std::string key = CreateKey(0);
  s = scope->Put(key, value);
  EXPECT_TRUE(s.ok());
  s = scopes.Commit(std::move(scope), /*sync_on_commit=*/false);
  EXPECT_TRUE(s.ok());
  EXPECT_TRUE(failure_status.ok());

  leveldb::ReadOptions options;
  options.verify_checksums = true;
  std::string out;
  leveldb_->db()->Get(options, CreateKey(0), &out);
  EXPECT_EQ(out, value);
}

TEST_F(LevelDBScopeTest, InMemoryAbort) {
  SetUpRealDatabase();
  DisjointRangeLockManager lock_manager(3);

  leveldb::Status failure_status = leveldb::Status::OK();
  LevelDBScopes scopes(
      metadata_prefix_, kWriteBatchSizeForTesting, leveldb_, &lock_manager,
      base::BindLambdaForTesting(
          [&failure_status](leveldb::Status s) { failure_status = s; }));

  leveldb::Status s = scopes.Initialize();
  EXPECT_TRUE(s.ok());
  scopes.StartRecoveryAndCleanupTasks(
      LevelDBScopes::TaskRunnerMode::kNewCleanupAndRevertSequences);
  auto scope = scopes.CreateScope(
      AcquireLocksSync(&lock_manager, {CreateSimpleExclusiveLock()}), {});

  // This change is smaller than 1024 bytes so it should be in-memory.
  std::string value = "12345";
  std::string key = CreateKey(0);
  s = scope->Put(key, value);
  EXPECT_TRUE(s.ok());
  s = scopes.Commit(std::move(scope), /*sync_on_commit=*/false);
  EXPECT_TRUE(s.ok());

  // Write over the value and abort.
  scope = scopes.CreateScope(
      AcquireLocksSync(&lock_manager, {CreateSimpleExclusiveLock()}), {});
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
  EXPECT_TRUE(failure_status.ok());
}

TEST_F(LevelDBScopeTest, AbortWithRevertTask) {
  SetUpRealDatabase();
  DisjointRangeLockManager lock_manager(3);

  leveldb::Status failure_status = leveldb::Status::OK();
  LevelDBScopes scopes(
      metadata_prefix_, kWriteBatchSizeForTesting, leveldb_, &lock_manager,
      base::BindLambdaForTesting(
          [&failure_status](leveldb::Status s) { failure_status = s; }));

  leveldb::Status s = scopes.Initialize();
  EXPECT_TRUE(s.ok());
  scopes.StartRecoveryAndCleanupTasks(
      LevelDBScopes::TaskRunnerMode::kNewCleanupAndRevertSequences);

  std::string value = "12345";
  leveldb::WriteOptions woptions;
  s = leveldb_->db()->Put(woptions, CreateKey(0), leveldb::Slice(value));

  auto scope = scopes.CreateScope(
      AcquireLocksSync(&lock_manager, {CreateSimpleExclusiveLock()}), {});

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
  EXPECT_TRUE(failure_status.ok());
}

TEST_F(LevelDBScopeTest, ManyScopes) {
  SetUpRealDatabase();
  DisjointRangeLockManager lock_manager(3);
  leveldb::Status failure_status;
  LevelDBScopes scopes(
      metadata_prefix_, kWriteBatchSizeForTesting, leveldb_, &lock_manager,
      base::BindLambdaForTesting(
          [&failure_status](leveldb::Status s) { failure_status = s; }));

  leveldb::Status s = scopes.Initialize();
  EXPECT_TRUE(s.ok());
  scopes.StartRecoveryAndCleanupTasks(
      LevelDBScopes::TaskRunnerMode::kNewCleanupAndRevertSequences);

  std::string value;
  for (int i = 0; i < 20; ++i) {
    std::string key = CreateKey(i);
    value = CreateLargeValue(i);
    auto scope = scopes.CreateScope(
        AcquireLocksSync(&lock_manager, {CreateExclusiveLock(i)}), {});
    s = scope->Put(key, value);
    EXPECT_TRUE(s.ok());
    s = scopes.Commit(std::move(scope), /*sync_on_commit=*/false);
    EXPECT_TRUE(s.ok());
  }

  // Wait until cleanup task runs.
  base::RunLoop loop;
  scopes.CleanupRunnerForTesting()->PostTask(FROM_HERE, loop.QuitClosure());
  loop.Run();

  ScopesEncoder encoder;
  for (int i = 0; i < 20; ++i) {
    EXPECT_TRUE(
        IsPrefixedRangeEmptyInDB(encoder.TasksKeyPrefix(metadata_prefix_, i)));
  }
  EXPECT_TRUE(
      IsPrefixedRangeEmptyInDB(encoder.ScopeMetadataPrefix(metadata_prefix_)));

  EXPECT_TRUE(failure_status.ok());
}

TEST_F(LevelDBScopeTest, DeleteRangeExclusive) {
  SetUpRealDatabase();
  DisjointRangeLockManager lock_manager(3);
  leveldb::Status failure_status;
  LevelDBScopes scopes(
      metadata_prefix_, kWriteBatchSizeForTesting, leveldb_, &lock_manager,
      base::BindLambdaForTesting(
          [&failure_status](leveldb::Status s) { failure_status = s; }));

  leveldb::Status s = scopes.Initialize();
  EXPECT_TRUE(s.ok());
  scopes.StartRecoveryAndCleanupTasks(
      LevelDBScopes::TaskRunnerMode::kNewCleanupAndRevertSequences);

  // Create values for keys 0-20, inclusive.
  std::string value;
  auto scope = scopes.CreateScope(
      AcquireLocksSync(&lock_manager, {CreateSimpleExclusiveLock()}), {});
  for (int i = 0; i < 21; ++i) {
    std::string key = CreateKey(i);
    value = i % 2 == 0 ? CreateLargeValue(i) : "smallvalue";
    s = scope->Put(key, value);
    EXPECT_TRUE(s.ok());
  }
  s = scopes.Commit(std::move(scope), /*sync_on_commit=*/false);
  EXPECT_TRUE(s.ok());

  // Do a exclusive range delete, so we should not delete 20.
  scope = scopes.CreateScope(
      AcquireLocksSync(&lock_manager, {CreateSimpleExclusiveLock()}), {});
  s = scope->DeleteRange(
      CreateKey(0), CreateKey(20),
      LevelDBScopeDeletionMode::kImmediateWithRangeEndExclusive);
  EXPECT_TRUE(s.ok());
  s = scopes.Commit(std::move(scope), /*sync_on_commit=*/false);
  EXPECT_TRUE(s.ok());

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
  DisjointRangeLockManager lock_manager(3);
  leveldb::Status failure_status;
  LevelDBScopes scopes(
      metadata_prefix_, kWriteBatchSizeForTesting, leveldb_, &lock_manager,
      base::BindLambdaForTesting(
          [&failure_status](leveldb::Status s) { failure_status = s; }));

  leveldb::Status s = scopes.Initialize();
  EXPECT_TRUE(s.ok());
  scopes.StartRecoveryAndCleanupTasks(
      LevelDBScopes::TaskRunnerMode::kNewCleanupAndRevertSequences);

  // Create values for keys 0-20, inclusive.
  std::string value;
  auto scope = scopes.CreateScope(
      AcquireLocksSync(&lock_manager, {CreateSimpleExclusiveLock()}), {});
  for (int i = 0; i < 21; ++i) {
    std::string key = CreateKey(i);
    value = i % 2 == 0 ? CreateLargeValue(i) : "smallvalue";
    s = scope->Put(key, value);
    EXPECT_TRUE(s.ok());
  }
  s = scopes.Commit(std::move(scope), /*sync_on_commit=*/false);
  EXPECT_TRUE(s.ok());

  // Do an inclusive delete range, so key 20 should be deleted.
  scope = scopes.CreateScope(
      AcquireLocksSync(&lock_manager, {CreateSimpleExclusiveLock()}), {});
  s = scope->DeleteRange(
      CreateKey(0), CreateKey(20),
      LevelDBScopeDeletionMode::kImmediateWithRangeEndInclusive);
  EXPECT_TRUE(s.ok());
  s = scopes.Commit(std::move(scope), /*sync_on_commit=*/false);
  EXPECT_TRUE(s.ok());

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
  DisjointRangeLockManager lock_manager(3);
  leveldb::Status failure_status;
  LevelDBScopes scopes(
      metadata_prefix_, kWriteBatchSizeForTesting, leveldb_, &lock_manager,
      base::BindLambdaForTesting(
          [&failure_status](leveldb::Status s) { failure_status = s; }));

  leveldb::Status s = scopes.Initialize();
  EXPECT_TRUE(s.ok());
  scopes.StartRecoveryAndCleanupTasks(
      LevelDBScopes::TaskRunnerMode::kNewCleanupAndRevertSequences);

  std::string value;
  auto scope = scopes.CreateScope(
      AcquireLocksSync(&lock_manager, {CreateSimpleExclusiveLock()}), {});
  for (int i = 0; i < 20; ++i) {
    std::string key = CreateKey(i);
    value = i % 2 == 0 ? CreateLargeValue(i) : "smallvalue";
    s = scope->Put(key, value);
    EXPECT_TRUE(s.ok());
  }
  s = scopes.Commit(std::move(scope), /*sync_on_commit=*/false);
  EXPECT_TRUE(s.ok());

  scope = scopes.CreateScope(
      AcquireLocksSync(&lock_manager, {CreateSimpleExclusiveLock()}), {});
  s = scope->DeleteRange(CreateKey(0), CreateKey(20),
                         LevelDBScopeDeletionMode::kDeferred);
  EXPECT_TRUE(s.ok());
  s = scopes.Commit(std::move(scope), /*sync_on_commit=*/false);
  EXPECT_TRUE(s.ok());

  // Wait until cleanup task runs.
  base::RunLoop loop;
  scopes.CleanupRunnerForTesting()->PostTask(FROM_HERE, loop.QuitClosure());
  loop.Run();

  // Be a good citizen and acquire read locks.
  auto locks = AcquireLocksSync(&lock_manager, {CreateSimpleSharedLock()});
  leveldb::ReadOptions options;
  options.verify_checksums = true;
  for (int i = 0; i < 20; ++i) {
    std::string key = CreateKey(i);
    s = leveldb_->db()->Get(options, key, &value);
    EXPECT_TRUE(s.IsNotFound()) << i;
  }
  EXPECT_TRUE(failure_status.ok());
}

TEST_F(LevelDBScopeTest, DeleteRangeCompact) {
  SetUpRealDatabase();
  DisjointRangeLockManager lock_manager(3);
  leveldb::Status failure_status;
  LevelDBScopes scopes(
      metadata_prefix_, kWriteBatchSizeForTesting, leveldb_, &lock_manager,
      base::BindLambdaForTesting(
          [&failure_status](leveldb::Status s) { failure_status = s; }));

  leveldb::Status s = scopes.Initialize();
  EXPECT_TRUE(s.ok());
  scopes.StartRecoveryAndCleanupTasks(
      LevelDBScopes::TaskRunnerMode::kNewCleanupAndRevertSequences);

  std::string value;
  auto scope = scopes.CreateScope(
      AcquireLocksSync(&lock_manager, {CreateSimpleExclusiveLock()}), {});
  for (int i = 0; i < 20; ++i) {
    std::string key = CreateKey(i);
    value = i % 2 == 0 ? CreateLargeValue(i) : "smallvalue";
    s = scope->Put(key, value);
    EXPECT_TRUE(s.ok());
  }
  s = scopes.Commit(std::move(scope), /*sync_on_commit=*/false);
  EXPECT_TRUE(s.ok());

  scope = scopes.CreateScope(
      AcquireLocksSync(&lock_manager, {CreateSimpleExclusiveLock()}), {});
  s = scope->DeleteRange(CreateKey(0), CreateKey(20),
                         LevelDBScopeDeletionMode::kDeferredWithCompaction);
  EXPECT_TRUE(s.ok());
  s = scopes.Commit(std::move(scope), /*sync_on_commit=*/false);
  EXPECT_TRUE(s.ok());

  // Wait until cleanup task runs.
  base::RunLoop loop;
  scopes.CleanupRunnerForTesting()->PostTask(FROM_HERE, loop.QuitClosure());
  loop.Run();

  // Be a good citizen and acquire read locks.
  auto locks = AcquireLocksSync(&lock_manager, {CreateSimpleSharedLock()});
  leveldb::ReadOptions options;
  options.verify_checksums = true;
  for (int i = 0; i < 20; ++i) {
    std::string key = CreateKey(i);
    s = leveldb_->db()->Get(options, key, &value);
    EXPECT_TRUE(s.IsNotFound()) << i;
  }
  EXPECT_TRUE(failure_status.ok());
}

TEST_F(LevelDBScopeTest, RevertWithDeferredDelete) {
  SetUpRealDatabase();
  DisjointRangeLockManager lock_manager(3);
  leveldb::Status failure_status;
  LevelDBScopes scopes(
      metadata_prefix_, kWriteBatchSizeForTesting, leveldb_, &lock_manager,
      base::BindLambdaForTesting(
          [&failure_status](leveldb::Status s) { failure_status = s; }));
  leveldb::Status s = scopes.Initialize();
  EXPECT_TRUE(s.ok());
  scopes.StartRecoveryAndCleanupTasks(
      LevelDBScopes::TaskRunnerMode::kNewCleanupAndRevertSequences);

  // This test makes sure that the cleanup scheduled after the revert doesn't
  // execute it's cleanup tasks.

  // Populate the database.
  std::string value;
  auto scope = scopes.CreateScope(
      AcquireLocksSync(&lock_manager, {CreateSimpleExclusiveLock()}), {});
  for (int i = 0; i < 20; ++i) {
    std::string key = CreateKey(i);
    value = i % 2 == 0 ? CreateLargeValue(i) : "smallvalue";
    s = scope->Put(key, value);
    EXPECT_TRUE(s.ok());
  }
  s = scopes.Commit(std::move(scope), /*sync_on_commit=*/false);
  EXPECT_TRUE(s.ok());

  // Do a deferred delete & a write large enough to make this a log-based scope,
  // and then revert it.
  scope = scopes.CreateScope(
      AcquireLocksSync(&lock_manager, {CreateSimpleExclusiveLock()}), {});
  value = CreateLargeValue(20);
  s = scope->Put(CreateKey(20), value);
  s = scope->DeleteRange(CreateKey(0), CreateKey(20),
                         LevelDBScopeDeletionMode::kDeferred);
  EXPECT_TRUE(s.ok());
  scope.reset();

  // Wait until revert runner runs.
  {
    base::RunLoop loop;
    scopes.RevertRunnerForTesting()->PostTask(FROM_HERE, loop.QuitClosure());
    loop.Run();
  }

  // Wait until cleanup runner runs.
  {
    base::RunLoop loop;
    scopes.CleanupRunnerForTesting()->PostTask(FROM_HERE, loop.QuitClosure());
    loop.Run();
  }

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
  EXPECT_TRUE(failure_status.ok());
}

TEST_F(LevelDBScopeTest, EmptyRangeRevert) {
  SetUpRealDatabase();
  DisjointRangeLockManager lock_manager(3);
  leveldb::Status failure_status = leveldb::Status::OK();
  LevelDBScopes scopes(
      metadata_prefix_, kWriteBatchSizeForTesting, leveldb_, &lock_manager,
      base::BindLambdaForTesting(
          [&failure_status](leveldb::Status s) { failure_status = s; }));

  std::vector<std::pair<std::string, std::string>> empty_ranges = {
      {CreateKey(0), CreateKey(10)}, {CreateKey(30), CreateKey(50)}};
  leveldb::Status s = scopes.Initialize();
  EXPECT_TRUE(s.ok());
  scopes.StartRecoveryAndCleanupTasks(
      LevelDBScopes::TaskRunnerMode::kNewCleanupAndRevertSequences);
  auto scope = scopes.CreateScope(
      AcquireLocksSync(&lock_manager, {CreateSimpleExclusiveLock()}),
      std::move(empty_ranges));

  // Use a large value to ensure we are in a undo log state.
  std::string value = CreateLargeValue(0);
  s = scope->Put(CreateKey(0), value);
  EXPECT_TRUE(s.ok());
  s = scope->Put(CreateKey(1), value);
  EXPECT_TRUE(s.ok());
  s = scope->Put(CreateKey(11), value);
  EXPECT_TRUE(s.ok());
  scope.reset();
  EXPECT_TRUE(s.ok());
  EXPECT_TRUE(failure_status.ok());

  auto locks = AcquireLocksSync(&lock_manager, {CreateSimpleSharedLock()});
  leveldb::ReadOptions options;
  options.verify_checksums = true;
  std::string out;
  s = leveldb_->db()->Get(options, CreateKey(0), &out);
  EXPECT_TRUE(s.IsNotFound());
}

TEST_F(LevelDBScopeTest, BrokenDBForInitialize) {
  leveldb::Status error = leveldb::Status::IOError("test");
  leveldb_ = FakeLevelDBFactory::GetBrokenLevelDB(error, base::FilePath());
  DisjointRangeLockManager lock_manager(3);

  leveldb::Status failure_status = leveldb::Status::OK();
  LevelDBScopes scopes(
      metadata_prefix_, kWriteBatchSizeForTesting, leveldb_, &lock_manager,
      base::BindLambdaForTesting(
          [&failure_status](leveldb::Status s) { failure_status = s; }));

  leveldb::Status s = scopes.Initialize();
  EXPECT_FALSE(s.ok());
  EXPECT_EQ(s.ToString(), error.ToString());
}

TEST_F(LevelDBScopeTest, BrokenDBForCommit) {
  base::OnceCallback<void(leveldb::Status)> break_db;
  SetUpBreakableDB(&break_db);
  DisjointRangeLockManager lock_manager(3);

  leveldb::Status failure_status = leveldb::Status::OK();
  LevelDBScopes scopes(
      metadata_prefix_, kWriteBatchSizeForTesting, leveldb_, &lock_manager,
      base::BindLambdaForTesting(
          [&failure_status](leveldb::Status s) { failure_status = s; }));

  leveldb::Status s = scopes.Initialize();
  EXPECT_TRUE(s.ok());
  scopes.StartRecoveryAndCleanupTasks(
      LevelDBScopes::TaskRunnerMode::kNewCleanupAndRevertSequences);
  auto scope = scopes.CreateScope(
      AcquireLocksSync(&lock_manager, {CreateSimpleExclusiveLock()}), {});

  leveldb::Status error = leveldb::Status::IOError("test");
  std::move(break_db).Run(error);
  std::string value = "12345";
  std::string key = CreateKey(0);
  s = scope->Put(key, value);
  EXPECT_TRUE(s.ok());
  s = scopes.Commit(std::move(scope), /*sync_on_commit=*/false);
  EXPECT_EQ(s.ToString(), error.ToString());
  EXPECT_TRUE(failure_status.ok());
}

TEST_F(LevelDBScopeTest, BrokenDBForCleanup) {
  base::OnceCallback<void(leveldb::Status)> break_db;
  SetUpBreakableDB(&break_db);
  DisjointRangeLockManager lock_manager(3);

  leveldb::Status failure_status = leveldb::Status::OK();
  LevelDBScopes scopes(
      metadata_prefix_, kWriteBatchSizeForTesting, leveldb_, &lock_manager,
      base::BindLambdaForTesting(
          [&failure_status](leveldb::Status s) { failure_status = s; }));

  leveldb::Status s = scopes.Initialize();
  EXPECT_TRUE(s.ok());
  scopes.StartRecoveryAndCleanupTasks(
      LevelDBScopes::TaskRunnerMode::kNewCleanupAndRevertSequences);
  auto scope = scopes.CreateScope(
      AcquireLocksSync(&lock_manager, {CreateSimpleExclusiveLock()}), {});

  leveldb::Status error = leveldb::Status::IOError("test");
  std::string value = CreateLargeValue(0);
  std::string key = CreateKey(0);
  s = scope->Put(key, value);
  EXPECT_TRUE(s.ok());
  s = scopes.Commit(std::move(scope), /*sync_on_commit=*/false);
  // Break the database, which should hopefully effect the cleanup task.
  std::move(break_db).Run(error);
  EXPECT_TRUE(s.ok());

  // Wait until cleanup task runs.
  base::RunLoop loop;
  scopes.CleanupRunnerForTesting()->PostTask(FROM_HERE, loop.QuitClosure());
  loop.Run();

  EXPECT_FALSE(failure_status.ok());
  EXPECT_EQ(failure_status.ToString(), error.ToString());
}

TEST_F(LevelDBScopeTest, BrokenDBForRevert) {
  base::OnceCallback<void(leveldb::Status)> break_db;
  SetUpBreakableDB(&break_db);
  DisjointRangeLockManager lock_manager(3);

  leveldb::Status failure_status = leveldb::Status::OK();
  LevelDBScopes scopes(
      metadata_prefix_, kWriteBatchSizeForTesting, leveldb_, &lock_manager,
      base::BindLambdaForTesting(
          [&failure_status](leveldb::Status s) { failure_status = s; }));

  leveldb::Status s = scopes.Initialize();
  EXPECT_TRUE(s.ok());
  scopes.StartRecoveryAndCleanupTasks(
      LevelDBScopes::TaskRunnerMode::kNewCleanupAndRevertSequences);
  auto scope = scopes.CreateScope(
      AcquireLocksSync(&lock_manager, {CreateSimpleExclusiveLock()}), {});

  leveldb::Status error = leveldb::Status::IOError("test");
  std::string value = CreateLargeValue(0);
  std::string key = CreateKey(0);
  s = scope->Put(key, value);
  EXPECT_TRUE(s.ok());
  std::move(break_db).Run(error);
  scope.reset();

  // Wait until revert task runs.
  base::RunLoop loop;
  scopes.RevertRunnerForTesting()->PostTask(FROM_HERE, loop.QuitClosure());
  loop.Run();

  EXPECT_FALSE(failure_status.ok());
  EXPECT_EQ(failure_status.ToString(), error.ToString());
}

TEST_F(LevelDBScopeTest, DeleteNonExistentRangeDoesNotWrite) {
  SetUpRealDatabase();
  DisjointRangeLockManager lock_manager(3);

  leveldb::Status failure_status = leveldb::Status::OK();
  LevelDBScopes scopes(
      metadata_prefix_, kWriteBatchSizeForTesting, leveldb_, &lock_manager,
      base::BindLambdaForTesting(
          [&failure_status](leveldb::Status s) { failure_status = s; }));

  leveldb::Status s = scopes.Initialize();
  EXPECT_TRUE(s.ok());
  scopes.StartRecoveryAndCleanupTasks(
      LevelDBScopes::TaskRunnerMode::kNewCleanupAndRevertSequences);

  auto scope = scopes.CreateScope(
      AcquireLocksSync(&lock_manager, {CreateSimpleExclusiveLock()}), {});

  s = scope->DeleteRange(
      "b1", "b2", LevelDBScopeDeletionMode::kImmediateWithRangeEndInclusive);
  EXPECT_TRUE(s.ok());
  s = scopes.Commit(std::move(scope), /*sync_on_commit=*/false);
  EXPECT_TRUE(s.ok());
  EXPECT_TRUE(failure_status.ok());
}

}  // namespace
}  // namespace content
