// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/storage/dom_storage/test_support/dom_storage_database_testing.h"

#include <algorithm>

#include "base/memory/scoped_refptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/test/test_future.h"
#include "components/services/storage/dom_storage/async_dom_storage_database.h"
#include "storage/common/database/db_status.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace storage {

namespace {

// Sort by the map's session ID and storage key.
bool CompareMapMetadata(const DomStorageDatabase::MapMetadata& left,
                        const DomStorageDatabase::MapMetadata& right) {
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
  const std::vector<std::string>& source_session_ids =
      source.map_locator.session_ids();
  CHECK_GT(source_session_ids.size(), 0u);

  DomStorageDatabase::MapMetadata clone{
      .map_locator =
          source.map_locator.map_id().has_value()
              ? DomStorageDatabase::MapLocator(source_session_ids[0],
                                               source.map_locator.storage_key(),
                                               *source.map_locator.map_id())
              : DomStorageDatabase::MapLocator(
                    source_session_ids[0], source.map_locator.storage_key()),
      .last_accessed{source.last_accessed},
      .last_modified{source.last_modified},
      .total_size{source.total_size},
  };

  for (size_t i = 1u; i < source_session_ids.size(); ++i) {
    clone.map_locator.AddSession(source_session_ids[i]);
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

void OpenAsyncDomStorageDatabaseInMemorySync(
    StorageType storage_type,
    std::unique_ptr<AsyncDomStorageDatabase>* result) {
  base::test::TestFuture<DbStatus> status_future;

  std::unique_ptr<AsyncDomStorageDatabase> database =
      AsyncDomStorageDatabase::Open(
          storage_type, /*directory=*/base::FilePath(),
          "TestInMemoryDomStorageDatabase", /*memory_dump_id=*/std::nullopt,
          status_future.GetCallback());

  const DbStatus& status = status_future.Get();
  ASSERT_TRUE(status.ok()) << status.ToString();
  *result = std::move(database);
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

}  // namespace storage
