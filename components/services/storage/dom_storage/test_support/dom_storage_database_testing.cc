// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/storage/dom_storage/test_support/dom_storage_database_testing.h"

#include "base/memory/scoped_refptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/test/test_future.h"
#include "components/services/storage/dom_storage/async_dom_storage_database.h"
#include "storage/common/database/db_status.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace storage {

namespace {

// Sort by the map's session ID and storage key.
bool CompareMapMetadata(const DomStorageDatabase::MapMetadata& left,
                        const DomStorageDatabase::MapMetadata& right) {
  if (left.map_locator.session_id() == right.map_locator.session_id()) {
    return left.map_locator.storage_key() > right.map_locator.storage_key();
  }
  return left.map_locator.session_id() > right.map_locator.session_id();
}

}  // namespace

void ExpectEqualsMapLocator(const DomStorageDatabase::MapLocator& left,
                            const DomStorageDatabase::MapLocator& right) {
  EXPECT_EQ(left.session_id(), right.session_id());
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
  if (source.map_locator.map_id()) {
    return {
        .map_locator{
            source.map_locator.session_id(),
            source.map_locator.storage_key(),
            source.map_locator.map_id().value(),
        },
        .last_accessed{source.last_accessed},
        .last_modified{source.last_modified},
        .total_size{source.total_size},
    };
  }

  // `source` does not have a map ID.
  return {
      .map_locator{
          source.map_locator.session_id(),
          source.map_locator.storage_key(),
      },
      .last_accessed{source.last_accessed},
      .last_modified{source.last_modified},
      .total_size{source.total_size},
  };
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

  scoped_refptr<base::SequencedTaskRunner> database_task_runner =
      base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::WithBaseSyncPrimitives(),
           base::TaskShutdownBehavior::BLOCK_SHUTDOWN});

  std::unique_ptr<AsyncDomStorageDatabase> database =
      AsyncDomStorageDatabase::Open(
          storage_type, /*directory=*/base::FilePath(),
          "TestInMemoryDomStorageDatabase", /*memory_dump_id=*/std::nullopt,
          std::move(database_task_runner), status_future.GetCallback());

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
    std::vector<blink::StorageKey> storage_keys,
    absl::flat_hash_set<int64_t> excluded_cloned_map_ids) {
  base::test::TestFuture<DbStatus> status_future;
  database.DeleteStorageKeysFromSession(
      std::move(session_id), std::move(storage_keys),
      std::move(excluded_cloned_map_ids), status_future.GetCallback());

  const DbStatus& status = status_future.Get();
  EXPECT_TRUE(status.ok()) << status.ToString();
}

}  // namespace storage
