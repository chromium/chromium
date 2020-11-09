// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/storage/indexed_db/scopes/leveldb_scopes_test_utils.h"

#include "base/memory/ptr_util.h"
#include "base/no_destructor.h"
#include "base/run_loop.h"
#include "base/sequenced_task_runner.h"
#include "base/strings/stringprintf.h"
#include "base/synchronization/waitable_event_watcher.h"
#include "base/system/sys_info.h"
#include "base/test/bind_test_util.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "components/services/storage/indexed_db/leveldb/leveldb_factory.h"
#include "third_party/leveldatabase/env_chromium.h"
#include "third_party/leveldatabase/leveldb_chrome.h"
#include "third_party/leveldatabase/src/include/leveldb/comparator.h"
#include "third_party/leveldatabase/src/include/leveldb/db.h"
#include "third_party/leveldatabase/src/include/leveldb/filter_policy.h"

namespace content {
namespace {
constexpr size_t kWriteBufferSize = 4 * 1024 * 1024;

leveldb_env::Options GetLevelDBOptions() {
  static base::NoDestructor<leveldb_env::ChromiumEnv> gTestEnv;
  static const leveldb::FilterPolicy* kFilterPolicy =
      leveldb::NewBloomFilterPolicy(10);
  leveldb_env::Options options;
  options.comparator = leveldb::BytewiseComparator();
  options.paranoid_checks = true;
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
  if (!leveldb_factory_)
    leveldb_factory_ = std::make_unique<FakeLevelDBFactory>(GetLevelDBOptions(),
                                                            "scopes-test-db");
}

void LevelDBScopesTestBase::TearDown() {
  if (leveldb_) {
    CloseScopesAndDestroyLevelDBState();
    if (temp_directory_.IsValid()) {
      leveldb_factory_->DestroyLevelDB(temp_directory_.GetPath());
      task_env_.RunUntilIdle();
      ASSERT_TRUE(temp_directory_.Delete());
    }
  }
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
        base::SequencedTaskRunnerHandle::Get());
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
    base::OnceCallback<void(leveldb::Status)>* callback) {
  if (leveldb_)
    TearDown();
  ASSERT_TRUE(temp_directory_.CreateUniqueTempDir());

  leveldb::Status status;
  std::unique_ptr<leveldb::DB> temp_real_db;
  std::tie(temp_real_db, status) =
      leveldb_factory_->OpenDB(temp_directory_.GetPath().AsUTF8Unsafe(),
                               /*create_if_missing=*/true, kWriteBufferSize);
  ASSERT_TRUE(status.ok());
  ASSERT_TRUE(temp_real_db);

  std::unique_ptr<leveldb::DB> breakable_db;
  std::tie(breakable_db, *callback) =
      FakeLevelDBFactory::CreateBreakableDB(std::move(temp_real_db));
  ASSERT_TRUE(breakable_db);

  leveldb_factory_->EnqueueNextOpenDBResult(std::move(breakable_db),
                                            leveldb::Status::OK());
  CreateAndSaveLevelDBState();
}

void LevelDBScopesTestBase::SetUpFlakyDB(
    std::queue<FakeLevelDBFactory::FlakePoint> flake_points) {
  if (leveldb_)
    TearDown();
  ASSERT_TRUE(temp_directory_.CreateUniqueTempDir());
  leveldb::Status status;

  std::unique_ptr<leveldb::DB> temp_db;
  std::tie(temp_db, status) =
      leveldb_factory_->OpenDB(temp_directory_.GetPath().AsUTF8Unsafe(),
                               /*create_if_missing=*/true, kWriteBufferSize);
  ASSERT_TRUE(status.ok());
  ASSERT_TRUE(temp_db);

  std::unique_ptr<leveldb::DB> flaky_db = FakeLevelDBFactory::CreateFlakyDB(
      std::move(temp_db), std::move(flake_points));

  leveldb_factory_->EnqueueNextOpenDBResult(std::move(flaky_db),
                                            leveldb::Status::OK());
  CreateAndSaveLevelDBState();
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

ScopesLockManager::ScopeLockRequest
LevelDBScopesTestBase::CreateSimpleSharedLock() {
  return {0,
          {simple_lock_begin_, simple_lock_end_},
          ScopesLockManager::LockType::kShared};
}

ScopesLockManager::ScopeLockRequest
LevelDBScopesTestBase::CreateSimpleExclusiveLock() {
  return {0,
          {simple_lock_begin_, simple_lock_end_},
          ScopesLockManager::LockType::kExclusive};
}

ScopesLockManager::ScopeLockRequest LevelDBScopesTestBase::CreateSharedLock(
    int i) {
  return {0,
          {base::StringPrintf("%010d", i * 2),
           base::StringPrintf("%010d", i * 2 + 1)},
          ScopesLockManager::LockType::kShared};
}

ScopesLockManager::ScopeLockRequest LevelDBScopesTestBase::CreateExclusiveLock(
    int i) {
  return {0,
          {base::StringPrintf("%010d", i * 2),
           base::StringPrintf("%010d", i * 2 + 1)},
          ScopesLockManager::LockType::kExclusive};
}

const base::FilePath& LevelDBScopesTestBase::DatabaseDirFilePath() {
  return temp_directory_.GetPath();
}

void LevelDBScopesTestBase::CreateAndSaveLevelDBState() {
  leveldb::Status status;
  std::tie(leveldb_, status, std::ignore) = leveldb_factory_->OpenLevelDBState(
      temp_directory_.GetPath(), true, kWriteBufferSize);
  ASSERT_TRUE(status.ok()) << status.ToString();
  ASSERT_TRUE(leveldb_);
}

}  // namespace content
