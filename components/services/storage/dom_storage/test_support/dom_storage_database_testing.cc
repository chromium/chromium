// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/storage/dom_storage/test_support/dom_storage_database_testing.h"

#include <algorithm>

#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/test/bind.h"
#include "base/test/gmock_expected_support.h"
#include "base/test/test_future.h"
#include "components/services/storage/dom_storage/db_status.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace storage {
namespace {

// Returns a copy of `source` as a byte vector.
std::vector<uint8_t> ToBytes(std::string source) {
  return std::vector<uint8_t>(source.begin(), source.end());
}

// Sort by the map's ID, session ID and storage key.
bool CompareMapMetadata(const DomStorageDatabase::MapMetadata& left,
                        const DomStorageDatabase::MapMetadata& right) {
  if (left.map_locator.map_id() != right.map_locator.map_id()) {
    return left.map_locator.map_id() < right.map_locator.map_id();
  }
  if (left.map_locator.storage_key() != right.map_locator.storage_key()) {
    return left.map_locator.storage_key() < right.map_locator.storage_key();
  }
  return std::lexicographical_compare(left.map_locator.session_ids().begin(),
                                      left.map_locator.session_ids().end(),
                                      right.map_locator.session_ids().begin(),
                                      right.map_locator.session_ids().end());
}

}  // namespace

void ExpectEqualsMapLocator(const DomStorageDatabase::MapLocator& left,
                            const DomStorageDatabase::MapLocator& right) {
  EXPECT_THAT(left.session_ids(),
              testing::UnorderedElementsAreArray(right.session_ids()));
  EXPECT_EQ(left.storage_key(), right.storage_key());
  EXPECT_EQ(left.map_id(), right.map_id());
}

void ExpectEqualsMapMetadata(const DomStorageDatabase::MapMetadata& left,
                             const DomStorageDatabase::MapMetadata& right) {
  ExpectEqualsMapLocator(left.map_locator, right.map_locator);
  EXPECT_EQ(left.last_accessed, right.last_accessed);
  EXPECT_EQ(left.last_modified, right.last_modified);
  EXPECT_EQ(left.total_size, right.total_size);
}

void ExpectEqualsMapMetadataSpan(
    base::span<const DomStorageDatabase::MapMetadata> left,
    base::span<const DomStorageDatabase::MapMetadata> right) {
  ASSERT_EQ(left.size(), right.size());

  // Left and right may not have the same order of elements. Create clones to
  // sort before comparing.
  std::vector<DomStorageDatabase::MapMetadata> left_clone =
      CloneMapMetadataVector(left);
  std::sort(left_clone.begin(), left_clone.end(), CompareMapMetadata);

  std::vector<DomStorageDatabase::MapMetadata> right_clone =
      CloneMapMetadataVector(right);
  std::sort(right_clone.begin(), right_clone.end(), CompareMapMetadata);

  for (size_t i = 0u; i < left.size(); ++i) {
    ExpectEqualsMapMetadata(left_clone[i], right_clone[i]);
  }
}

DomStorageDatabase::MapMetadata CloneMapMetadata(
    const DomStorageDatabase::MapMetadata& source) {
  DomStorageDatabase::MapMetadata clone{
      .map_locator =
          source.map_locator.map_id().has_value()
              ? DomStorageDatabase::MapLocator(source.map_locator.storage_key(),
                                               *source.map_locator.map_id())
              : DomStorageDatabase::MapLocator(
                    source.map_locator.storage_key()),
      .last_accessed{source.last_accessed},
      .last_modified{source.last_modified},
      .total_size{source.total_size},
  };

  // Clone the session IDs.
  for (const std::string& session_id : source.map_locator.session_ids()) {
    clone.map_locator.AddSession(session_id);
  }
  return clone;
}

std::vector<DomStorageDatabase::MapMetadata> CloneMapMetadataVector(
    base::span<const DomStorageDatabase::MapMetadata> source_span) {
  std::vector<DomStorageDatabase::MapMetadata> results;
  for (const DomStorageDatabase::MapMetadata& source : source_span) {
    results.push_back(CloneMapMetadata(source));
  }
  return results;
}

void TestUpdateMaps(DomStorageDatabase& database,
                    const DomStorageDatabase::MapLocator& map1_locator,
                    const DomStorageDatabase::MapLocator& map2_locator) {
  // Create two maps, each with 3 key/value pairs.
  std::map<DomStorageDatabase::Key, DomStorageDatabase::Value> map1_entries{
      {ToBytes("key_1"), ToBytes("value_1")},
      {ToBytes("key_2"), ToBytes("value_2")},
      {ToBytes("key_3"), ToBytes("value_3")},
  };
  std::map<DomStorageDatabase::Key, DomStorageDatabase::Value> map2_entries{
      {ToBytes("key_4"), ToBytes("value_4")},
      {ToBytes("key_5"), ToBytes("value_5")},
      {ToBytes("key_6"), ToBytes("value_6")},
  };

  // Write the key/value pairs to the database.
  std::vector<DomStorageDatabase::MapBatchUpdate> add_update;
  add_update.emplace_back(map1_locator.Clone());
  for (const auto& [key, value] : map1_entries) {
    add_update.back().entries_to_add.emplace_back(key, value);
  }
  add_update.emplace_back(map2_locator.Clone());
  for (const auto& [key, value] : map2_entries) {
    add_update.back().entries_to_add.emplace_back(key, value);
  }
  DbStatus status = database.UpdateMaps(std::move(add_update));
  EXPECT_TRUE(status.ok()) << status.ToString();

  // Read back the key/value pairs from the database.
  ASSERT_OK_AND_ASSIGN((std::map<DomStorageDatabase::Key,
                                 DomStorageDatabase::Value> actual_entries),
                       database.ReadMapKeyValues(map1_locator.Clone()));
  EXPECT_EQ(actual_entries, map1_entries);

  ASSERT_OK_AND_ASSIGN(actual_entries,
                       database.ReadMapKeyValues(map2_locator.Clone()));
  EXPECT_EQ(actual_entries, map2_entries);

  // Delete one of the key/value pairs from the first map.
  std::vector<DomStorageDatabase::MapBatchUpdate> delete_update;
  delete_update.emplace_back(map1_locator.Clone());
  delete_update.back().keys_to_delete.push_back(ToBytes("key_2"));

  // Delete two key/value pairs from the second map.
  delete_update.emplace_back(map2_locator.Clone());
  delete_update.back().keys_to_delete.push_back(ToBytes("key_4"));
  delete_update.back().keys_to_delete.push_back(ToBytes("key_6"));

  status = database.UpdateMaps(std::move(delete_update));
  EXPECT_TRUE(status.ok()) << status.ToString();

  // Read the remaining key/value pairs from the database.
  ASSERT_OK_AND_ASSIGN(actual_entries,
                       database.ReadMapKeyValues(map1_locator.Clone()));
  map1_entries.erase(ToBytes("key_2"));
  EXPECT_EQ(actual_entries, map1_entries);

  ASSERT_OK_AND_ASSIGN(actual_entries,
                       database.ReadMapKeyValues(map2_locator.Clone()));
  map2_entries.erase(ToBytes("key_4"));
  map2_entries.erase(ToBytes("key_6"));
  EXPECT_EQ(actual_entries, map2_entries);

  // Clear all of the first map's key/value pairs in the database.
  std::vector<DomStorageDatabase::MapBatchUpdate> clear_all_update;
  clear_all_update.emplace_back(map1_locator.Clone());
  clear_all_update[0].clear_all_first = true;

  status = database.UpdateMaps({std::move(clear_all_update)});
  EXPECT_TRUE(status.ok()) << status.ToString();

  // The first map must not contain any key/value pairs.
  ASSERT_OK_AND_ASSIGN(actual_entries,
                       database.ReadMapKeyValues(map1_locator.Clone()));
  EXPECT_EQ(actual_entries.size(), 0u);

  // `clear_all_first` must not delete key/value pairs from the second map.
  ASSERT_OK_AND_ASSIGN(actual_entries,
                       database.ReadMapKeyValues(map2_locator.Clone()));
  EXPECT_EQ(actual_entries, map2_entries);
}

void InsertMapEntries(
    DomStorageDatabase& database,
    const DomStorageDatabase::MapLocator& map_locator,
    const std::map<DomStorageDatabase::Key, DomStorageDatabase::Value>& entries,
    std::optional<DomStorageDatabase::MapBatchUpdate::Usage> usage_metadata) {
  // Write the `entries` to `database`.
  DomStorageDatabase::MapBatchUpdate map_update(map_locator.Clone());
  for (const auto& entry : entries) {
    map_update.entries_to_add.emplace_back(entry.first, entry.second);
  }
  map_update.map_usage = std::move(usage_metadata);

  std::vector<DomStorageDatabase::MapBatchUpdate> map_updates;
  map_updates.push_back(std::move(map_update));

  DbStatus status = database.UpdateMaps(std::move(map_updates));
  EXPECT_TRUE(status.ok()) << status.ToString();

  // Read back the entries and verify they match what was inserted.
  ASSERT_OK_AND_ASSIGN((std::map<DomStorageDatabase::Key,
                                 DomStorageDatabase::Value> actual_entries),
                       database.ReadMapKeyValues(map_locator.Clone()));
  EXPECT_EQ(actual_entries, entries);
}

void OpenAsyncDomStorageDatabaseInMemorySync(
    StorageType storage_type,
    std::unique_ptr<AsyncDomStorageDatabase>* result) {
  base::test::TestFuture<DbStatus> status_future;

  std::unique_ptr<AsyncDomStorageDatabase> database =
      AsyncDomStorageDatabase::Open(
          storage_type, /*database_path=*/base::FilePath(),
          /*memory_dump_id=*/std::nullopt, status_future.GetCallback());

  const DbStatus& status = status_future.Get();
  ASSERT_TRUE(status.ok()) << status.ToString();
  *result = std::move(database);
}

void ReadMapKeyValuesSync(
    AsyncDomStorageDatabase& database,
    DomStorageDatabase::MapLocator map_locator,
    std::map<DomStorageDatabase::Key, DomStorageDatabase::Value>*
        key_value_results) {
  base::test::TestFuture<
      StatusOr<std::map<DomStorageDatabase::Key, DomStorageDatabase::Value>>>
      map_key_values_future;
  database.ReadMapKeyValues(std::move(map_locator),
                            map_key_values_future.GetCallback());

  StatusOr<std::map<DomStorageDatabase::Key, DomStorageDatabase::Value>>
      map_key_values = map_key_values_future.Take();
  ASSERT_TRUE(map_key_values.has_value()) << map_key_values.error().ToString();
  *key_value_results = *std::move(map_key_values);
}

void ReadAllMetadataSync(AsyncDomStorageDatabase& database,
                         DomStorageDatabase::Metadata* metadata_results) {
  base::test::TestFuture<StatusOr<DomStorageDatabase::Metadata>>
      metadata_future;
  database.ReadAllMetadata(metadata_future.GetCallback());

  StatusOr<DomStorageDatabase::Metadata> metadata = metadata_future.Take();
  ASSERT_TRUE(metadata.has_value()) << metadata.error().ToString();
  *metadata_results = *std::move(metadata);
}

void PutMetadataSync(AsyncDomStorageDatabase& database,
                     DomStorageDatabase::Metadata metadata) {
  base::test::TestFuture<DbStatus> status_future;
  database.PutMetadata(std::move(metadata), status_future.GetCallback());

  // `SessionStorageNamespaceImplTest` requires `kNestableTasksAllowed`.
  ASSERT_TRUE(status_future.Wait(base::RunLoop::Type::kNestableTasksAllowed));

  const DbStatus& status = status_future.Get();
  EXPECT_TRUE(status.ok()) << status.ToString();
}

void DeleteStorageKeysFromSessionSync(
    AsyncDomStorageDatabase& database,
    std::string session_id,
    std::vector<blink::StorageKey> metadata_to_delete,
    std::vector<DomStorageDatabase::MapLocator> maps_to_delete) {
  base::test::TestFuture<DbStatus> status_future;
  database.DeleteStorageKeysFromSession(
      std::move(session_id), std::move(metadata_to_delete),
      std::move(maps_to_delete), status_future.GetCallback());

  const DbStatus& status = status_future.Get();
  EXPECT_TRUE(status.ok()) << status.ToString();
}

void DeleteSessionsSync(
    AsyncDomStorageDatabase& database,
    std::vector<std::string> session_ids,
    std::vector<DomStorageDatabase::MapLocator> maps_to_delete) {
  base::test::TestFuture<DbStatus> status_future;
  database.DeleteSessions(std::move(session_ids), std::move(maps_to_delete),
                          status_future.GetCallback());

  const DbStatus& status = status_future.Get();
  EXPECT_TRUE(status.ok()) << status.ToString();
}

FakeCommitter::FakeCommitter(AsyncDomStorageDatabase* database,
                             DomStorageDatabase::MapLocator map_locator)
    : database_(database), map_locator_(std::move(map_locator)) {
  database_->AddCommitter(this);
}

FakeCommitter::~FakeCommitter() {
  database_->RemoveCommitter(this);
}

void FakeCommitter::PutMapKeyValueSync(DomStorageDatabase::Key key,
                                       DomStorageDatabase::Value value) {
  // Create a batch update that writes `key` and `value`.
  DomStorageDatabase::MapBatchUpdate map_update(map_locator_.Clone());
  map_update.entries_to_add.emplace_back(std::move(key), std::move(value));
  CommitSync(std::move(map_update));
}

std::optional<DomStorageDatabase::MapBatchUpdate>
FakeCommitter::CollectCommit() {
  return std::exchange(pending_commit_, std::nullopt);
}

void FakeCommitter::ClearMapSync() {
  // Create a batch update that clears all keys and values.
  DomStorageDatabase::MapBatchUpdate map_update(map_locator_.Clone());
  map_update.clear_all_first = true;
  CommitSync(std::move(map_update));
}

base::OnceCallback<void(DbStatus)> FakeCommitter::GetCommitCompleteCallback() {
  return base::BindOnce(&FakeCommitter::OnCommitCompleted,
                        base::Unretained(this));
}

void FakeCommitter::CommitSync(DomStorageDatabase::MapBatchUpdate map_update) {
  pending_commit_ = std::move(map_update);

  // Commit the update to the database.
  CHECK(!commit_complete_run_loop_);
  commit_complete_run_loop_ = std::make_unique<base::RunLoop>();
  database_->InitiateCommit();

  // Wait for the commit to complete.
  commit_complete_run_loop_->Run();
  commit_complete_run_loop_.reset();

  // Verify that the commit succeeded.
  ASSERT_NE(commit_complete_result_, std::nullopt);
  EXPECT_TRUE(commit_complete_result_->ok())
      << commit_complete_result_->ToString();
  commit_complete_result_.reset();
}

void FakeCommitter::OnCommitCompleted(DbStatus status) {
  commit_complete_result_ = status;
  commit_complete_run_loop_->Quit();
}

void PutVersionForTesting(AsyncDomStorageDatabase& async_database,
                          int64_t version) {
  base::RunLoop run_loop;
  DbStatus status;

  async_database.database().PostTaskWithThisObject(
      base::BindLambdaForTesting([&](DomStorageDatabase* dom_storage_database) {
        status = dom_storage_database->PutVersionForTesting(version);
        run_loop.Quit();
      }));

  run_loop.Run();
  EXPECT_TRUE(status.ok()) << status.ToString();
}

}  // namespace storage
