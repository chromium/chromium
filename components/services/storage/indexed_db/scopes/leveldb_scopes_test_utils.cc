// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/storage/indexed_db/scopes/leveldb_scopes_test_utils.h"

#include "base/memory/ptr_util.h"
#include "base/no_destructor.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/synchronization/waitable_event_watcher.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/bind.h"
#include "third_party/leveldatabase/env_chromium.h"
#include "third_party/leveldatabase/leveldb_chrome.h"
#include "third_party/leveldatabase/src/include/leveldb/comparator.h"
#include "third_party/leveldatabase/src/include/leveldb/db.h"
#include "third_party/leveldatabase/src/include/leveldb/filter_policy.h"

namespace content::indexed_db {
namespace {

constexpr size_t kWriteBufferSize = 4 * 1024 * 1024;

leveldb_env::Options GetLevelDBOptions() {
  static base::NoDestructor<leveldb_env::ChromiumEnv> gTestEnv;
  static const leveldb::FilterPolicy* kFilterPolicy =
      leveldb::NewBloomFilterPolicy(10);
  leveldb_env::Options options;
  options.comparator = leveldb::BytewiseComparator();
  options.create_if_missing = true;
  options.paranoid_checks = true;
  options.write_buffer_size = 4 * 1024 * 1024;
  options.filter_policy = kFilterPolicy;
  options.compression = leveldb::kSnappyCompression;
  options.env = gTestEnv.get();
  return options;
}

}  // namespace

constexpr const size_t LevelDBScopesTestBase::kWriteBatchSizeForTesting;

LevelDBScopesTestBase::LevelDBScopesTestBase() = default;
LevelDBScopesTestBase::~LevelDBScopesTestBase() = default;

void LevelDBScopesTestBase::SetUp() {
  large_string_.assign(kWriteBatchSizeForTesting + 1, 'e');
}

void LevelDBScopesTestBase::TearDown() {
  if (leveldb_) {
    CloseScopesAndDestroyLevelDBState();
    if (temp_directory_.IsValid()) {
      DestroyDB();
      task_env_.RunUntilIdle();
      ASSERT_TRUE(temp_directory_.Delete());
    }
  }
}

leveldb::Status LevelDBScopesTestBase::DestroyDB() {
  return leveldb::DestroyDB(temp_directory_.GetPath().AsUTF8Unsafe(),
                            GetLevelDBOptions());
}

void LevelDBScopesTestBase::CloseScopesAndDestroyLevelDBState() {
  if (leveldb_) {
    base::RunLoop loop;
    base::WaitableEvent* leveldb_close_event_ptr;
    base::WaitableEventWatcher event_watcher;
    if (leveldb_->destruction_requested()) {
      leveldb_close_event_ptr = leveldb_->destruction_event();
    } else {
      leveldb_close_event_ptr = &leveldb_close_event_;
      leveldb_->RequestDestruction(leveldb_close_event_ptr);
    }
    event_watcher.StartWatching(
        leveldb_close_event_ptr,
        base::BindLambdaForTesting([&](base::WaitableEvent*) { loop.Quit(); }),
        base::SequencedTaskRunner::GetCurrentDefault());
    leveldb_.reset();
    loop.Run();
    // There is a possible race in |leveldb_close_event| where the signaling
    // thread is still in the WaitableEvent::Signal() method. To ensure that
    // the other thread exits their Signal method, any method on the
    // WaitableEvent can be called to acquire the internal lock (which will
    // subsequently wait for the other thread to exit the Signal method).
    EXPECT_TRUE(leveldb_close_event_ptr->IsSignaled());
  }
}

void LevelDBScopesTestBase::SetUpRealDatabase() {
  if (leveldb_)
    TearDown();
  ASSERT_TRUE(temp_directory_.CreateUniqueTempDir());
  CreateAndSaveLevelDBState();
}

void LevelDBScopesTestBase::SetUpBreakableDB(
    base::OnceCallback<void(leveldb::Status)>* break_db) {
  if (leveldb_)
    TearDown();
  ASSERT_TRUE(temp_directory_.CreateUniqueTempDir());

  auto options = GetLevelDBOptions();
  std::unique_ptr<leveldb::DB> real_db;
  options.create_if_missing = true;
  options.write_buffer_size = kWriteBufferSize;
  leveldb::Status status = leveldb_env::OpenDB(
      options, temp_directory_.GetPath().AsUTF8Unsafe(), &real_db);

  ASSERT_TRUE(status.ok());
  ASSERT_TRUE(real_db);

  std::unique_ptr<leveldb::DB> breakable_db;
  std::tie(breakable_db, *break_db) =
      FakeLevelDBFactory::CreateBreakableDB(std::move(real_db));
  ASSERT_TRUE(breakable_db);

  leveldb_ = LevelDBState::CreateForDiskDB(
      options.comparator, std::move(breakable_db), temp_directory_.GetPath());
}

void LevelDBScopesTestBase::SetUpFlakyDB(
    std::queue<FakeLevelDBFactory::FlakePoint> flake_points) {
  if (leveldb_)
    TearDown();
  ASSERT_TRUE(temp_directory_.CreateUniqueTempDir());

  auto options = GetLevelDBOptions();
  std::unique_ptr<leveldb::DB> real_db;
  options.create_if_missing = true;
  options.write_buffer_size = kWriteBufferSize;
  leveldb::Status status = leveldb_env::OpenDB(
      options, temp_directory_.GetPath().AsUTF8Unsafe(), &real_db);

  ASSERT_TRUE(status.ok());
  ASSERT_TRUE(real_db);

  std::unique_ptr<leveldb::DB> flaky_db = FakeLevelDBFactory::CreateFlakyDB(
      std::move(real_db), std::move(flake_points));
  ASSERT_TRUE(flaky_db);

  leveldb_ =
      LevelDBState::CreateForDiskDB(options.comparator, std::move(flaky_db),
                                    std::move(temp_directory_.GetPath()));
}

void LevelDBScopesTestBase::WriteScopesMetadata(int64_t scope_number,
                                                bool ignore_cleanup_tasks) {
  value_buffer_.clear();
  auto key = scopes_encoder_.ScopeMetadataKey(metadata_prefix_, scope_number);
  metadata_buffer_.set_ignore_cleanup_tasks(ignore_cleanup_tasks);
  metadata_buffer_.SerializeToString(&value_buffer_);
  leveldb::Status s =
      leveldb_->db()->Put(leveldb::WriteOptions(), key, value_buffer_);
  ASSERT_TRUE(s.ok());
  metadata_buffer_.Clear();
}

void LevelDBScopesTestBase::WriteUndoTask(int64_t scope_number,
                                          int64_t sequence_number) {
  value_buffer_.clear();
  auto key = scopes_encoder_.UndoTaskKey(metadata_prefix_, scope_number,
                                         sequence_number);
  undo_task_buffer_.SerializeToString(&value_buffer_);
  leveldb::Status s =
      leveldb_->db()->Put(leveldb::WriteOptions(), key, value_buffer_);
  ASSERT_TRUE(s.ok());
  undo_task_buffer_.Clear();
}

void LevelDBScopesTestBase::WriteCleanupTask(int64_t scope_number,
                                             int64_t sequence_number) {
  value_buffer_.clear();
  auto key = scopes_encoder_.CleanupTaskKey(metadata_prefix_, scope_number,
                                            sequence_number);
  cleanup_task_buffer_.SerializeToString(&value_buffer_);
  leveldb::Status s =
      leveldb_->db()->Put(leveldb::WriteOptions(), key, value_buffer_);
  ASSERT_TRUE(s.ok());
  cleanup_task_buffer_.Clear();
}

void LevelDBScopesTestBase::WriteLargeValue(const std::string& key) {
  leveldb::Status s =
      leveldb_->db()->Put(leveldb::WriteOptions(), key, large_string_);
  ASSERT_TRUE(s.ok());
}

leveldb::Status LevelDBScopesTestBase::LoadAt(const std::string& key) {
  return leveldb_->db()->Get(leveldb::ReadOptions(), key, &value_buffer_);
}

leveldb::Status LevelDBScopesTestBase::LoadScopeMetadata(int64_t scope_number) {
  auto key = scopes_encoder_.ScopeMetadataKey(metadata_prefix_, scope_number);
  return leveldb_->db()->Get(leveldb::ReadOptions(), key, &value_buffer_);
}

leveldb::Status LevelDBScopesTestBase::LoadUndoTask(int64_t scope_number,
                                                    int64_t sequence_number) {
  auto key = scopes_encoder_.UndoTaskKey(metadata_prefix_, scope_number,
                                         sequence_number);
  return leveldb_->db()->Get(leveldb::ReadOptions(), key, &value_buffer_);
}

leveldb::Status LevelDBScopesTestBase::LoadCleanupTask(
    int64_t scope_number,
    int64_t sequence_number) {
  auto key = scopes_encoder_.CleanupTaskKey(metadata_prefix_, scope_number,
                                            sequence_number);
  return leveldb_->db()->Get(leveldb::ReadOptions(), key, &value_buffer_);
}

bool LevelDBScopesTestBase::IsPrefixedRangeEmptyInDB(leveldb::Slice key) {
  std::unique_ptr<leveldb::Iterator> it =
      base::WrapUnique(leveldb_->db()->NewIterator(leveldb::ReadOptions()));
  it->Seek(key);
  if (it->Valid() && it->key().starts_with(key))
    return false;
  return true;
}

bool LevelDBScopesTestBase::IsScopeCleanedUp(int64_t scope_number) {
  return IsPrefixedRangeEmptyInDB(
             scopes_encoder_.TasksKeyPrefix(metadata_prefix_, scope_number)) &&
         IsPrefixedRangeEmptyInDB(
             scopes_encoder_.TasksKeyPrefix(metadata_prefix_, scope_number));
}

bool LevelDBScopesTestBase::ScopeDataExistsOnDisk() {
  return !IsPrefixedRangeEmptyInDB(
             scopes_encoder_.ScopeMetadataPrefix(metadata_prefix_)) ||
         !IsPrefixedRangeEmptyInDB(
             scopes_encoder_.TasksKeyPrefix(metadata_prefix_));
}

PartitionedLockManager::PartitionedLockRequest
LevelDBScopesTestBase::CreateSimpleSharedLock() {
  return {{0, simple_lock_begin_}, PartitionedLockManager::LockType::kShared};
}

PartitionedLockManager::PartitionedLockRequest
LevelDBScopesTestBase::CreateSimpleExclusiveLock() {
  return {{0, simple_lock_begin_},
          PartitionedLockManager::LockType::kExclusive};
}

PartitionedLockManager::PartitionedLockRequest
LevelDBScopesTestBase::CreateSharedLock(int i) {
  return {{0, base::StringPrintf("%010d", i * 2)},
          PartitionedLockManager::LockType::kShared};
}

PartitionedLockManager::PartitionedLockRequest
LevelDBScopesTestBase::CreateExclusiveLock(int i) {
  return {{0, base::StringPrintf("%010d", i * 2)},
          PartitionedLockManager::LockType::kExclusive};
}

const base::FilePath& LevelDBScopesTestBase::DatabaseDirFilePath() {
  return temp_directory_.GetPath();
}

leveldb::Status LevelDBScopesTestBase::CreateAndSaveLevelDBState() {
  leveldb_env::Options options = GetLevelDBOptions();
  std::unique_ptr<leveldb::DB> db;
  leveldb::Status status =
      leveldb_env::OpenDB(options, DatabaseDirFilePath().AsUTF8Unsafe(), &db);
  if (status.ok()) {
    leveldb_ = LevelDBState::CreateForDiskDB(options.comparator, std::move(db),
                                             DatabaseDirFilePath());
  } else {
    leveldb_.reset();
  }
  return status;
}

}  // namespace content::indexed_db
