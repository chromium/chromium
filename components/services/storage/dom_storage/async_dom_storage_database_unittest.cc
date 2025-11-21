// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/storage/dom_storage/async_dom_storage_database.h"

#include <memory>
#include <optional>
#include <vector>

#include "base/barrier_closure.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "components/services/storage/dom_storage/dom_storage_constants.h"
#include "components/services/storage/dom_storage/dom_storage_database.h"
#include "components/services/storage/dom_storage/leveldb/dom_storage_batch_operation_leveldb.h"
#include "components/services/storage/dom_storage/leveldb/dom_storage_database_leveldb.h"
#include "components/services/storage/dom_storage/leveldb/local_storage_leveldb.h"
#include "components/services/storage/dom_storage/test_support/dom_storage_database_testing.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"

namespace storage {

namespace {
constexpr const char kFirstFakeUrlString[] = "https://a-fake-url.test";
constexpr const char kSecondFakeUrlString[] = "https://b-fake-url.test";
constexpr const char kThirdFakeUrlString[] = "https://c-fake-url.test";
constexpr const char kFourthFakeUrlString[] = "https://d-fake-url.test";
}  // namespace

class AsyncDomStorageDatabaseTest : public testing::Test {
 public:
  AsyncDomStorageDatabaseTest();
  ~AsyncDomStorageDatabaseTest() override = default;

 protected:
  base::test::TaskEnvironment task_environment_;
  const blink::StorageKey kFirstStorageKey;
  const blink::StorageKey kSecondStorageKey;
  const blink::StorageKey kThirdStorageKey;
  const blink::StorageKey kFourthStorageKey;
};

AsyncDomStorageDatabaseTest::AsyncDomStorageDatabaseTest()
    : kFirstStorageKey(
          blink::StorageKey::CreateFromStringForTesting(kFirstFakeUrlString)),
      kSecondStorageKey(
          blink::StorageKey::CreateFromStringForTesting(kSecondFakeUrlString)),
      kThirdStorageKey(
          blink::StorageKey::CreateFromStringForTesting(kThirdFakeUrlString)),
      kFourthStorageKey(
          blink::StorageKey::CreateFromStringForTesting(kFourthFakeUrlString)) {
}

TEST_F(AsyncDomStorageDatabaseTest,
       WriteAndReadThenDeleteLocalStorageMetadata) {
  // Define test values to write to the database.
  const DomStorageDatabase::MapMetadata kExpectedMapMetadataArray[] = {
      {
          .map_locator{kLocalStorageSessionId, kFirstStorageKey},
          .last_accessed{base::Time::Now() - base::Days(7)},
      },
      {
          .map_locator{kLocalStorageSessionId, kSecondStorageKey},
          .last_modified{base::Time::Now() - base::Seconds(12)},
          .total_size{104},
      },
      {
          .map_locator{kLocalStorageSessionId, kThirdStorageKey},
          .last_accessed{base::Time::Now() - base::Minutes(15)},
      },
      {
          .map_locator{kLocalStorageSessionId, kFourthStorageKey},
          .last_accessed{base::Time::Now() - base::Minutes(30)},
          .last_modified{base::Time::Now() - base::Seconds(47)},
          .total_size{211114},
      },
  };
  const base::span<const DomStorageDatabase::MapMetadata> kExpectedMapMetadata =
      kExpectedMapMetadataArray;

  // Open the database.
  std::unique_ptr<AsyncDomStorageDatabase> database;
  ASSERT_NO_FATAL_FAILURE(OpenAsyncDomStorageDatabaseInMemorySync(
      StorageType::kLocalStorage, &database));

  // Writing empty metadata must not update the local storage LevelDB.
  ASSERT_NO_FATAL_FAILURE(PutMetadataSync(*database, /*metadata=*/{}));

  DomStorageDatabase::Metadata read_metadata;
  ASSERT_NO_FATAL_FAILURE(ReadAllMetadataSync(*database, &read_metadata));
  EXPECT_EQ(read_metadata.map_metadata.size(), 0u);
  EXPECT_EQ(read_metadata.next_map_id, std::nullopt);

  // Writing metadata without usage must not update the local storage LevelDB.
  DomStorageDatabase::Metadata metadata_without_usage;
  metadata_without_usage.map_metadata.push_back({
      .map_locator{kLocalStorageSessionId, kSecondStorageKey},
  });
  ASSERT_NO_FATAL_FAILURE(
      PutMetadataSync(*database, std::move(metadata_without_usage)));

  ASSERT_NO_FATAL_FAILURE(ReadAllMetadataSync(*database, &read_metadata));
  EXPECT_EQ(read_metadata.map_metadata.size(), 0u);
  EXPECT_EQ(read_metadata.next_map_id, std::nullopt);

  // Write each map's metadata to the database.
  for (size_t i = 0; i < kExpectedMapMetadata.size(); ++i) {
    // Write the metadata for a single map.
    DomStorageDatabase::Metadata cloned_metadata;
    cloned_metadata.map_metadata =
        CloneMapMetadataVector(base::span_from_ref(kExpectedMapMetadata[i]));

    ASSERT_NO_FATAL_FAILURE(
        PutMetadataSync(*database, std::move(cloned_metadata)));

    // Read the metadata from the database.
    ASSERT_NO_FATAL_FAILURE(ReadAllMetadataSync(*database, &read_metadata));

    // Read back the metadata written so far.
    auto expected_read_metadata = kExpectedMapMetadata.first(/*count=*/i + 1);
    ExpectEqualsMapMetadataSpan(read_metadata.map_metadata,
                                expected_read_metadata);

    // Local storage does not store the next map id number.
    EXPECT_EQ(read_metadata.next_map_id, std::nullopt);
  }

  // Delete the first and third storage keys.
  DeleteStorageKeysFromSessionSync(*database, kLocalStorageSessionId,
                                   {kFirstStorageKey, kThirdStorageKey},
                                   /*excluded_cloned_map_ids=*/{});

  ASSERT_NO_FATAL_FAILURE(ReadAllMetadataSync(*database, &read_metadata));
  EXPECT_EQ(read_metadata.next_map_id, std::nullopt);

  // Add the second and fourth storage keys as expected.
  std::vector<DomStorageDatabase::MapMetadata> expected_metadata_after_delete;
  expected_metadata_after_delete.push_back(
      CloneMapMetadata(kExpectedMapMetadata[1]));
  expected_metadata_after_delete.push_back(
      CloneMapMetadata(kExpectedMapMetadata[3]));

  ExpectEqualsMapMetadataSpan(read_metadata.map_metadata,
                              expected_metadata_after_delete);
}

TEST_F(AsyncDomStorageDatabaseTest, EnqueuePendingTasksWhileOpening) {
  // Define test values to write to the database.
  const blink::StorageKey kStorageKey =
      blink::StorageKey::CreateFromStringForTesting("https://a-fake-url.test");

  const DomStorageDatabase::MapMetadata kExpectedMapMetadata[] = {
      {
          .map_locator{kLocalStorageSessionId, kStorageKey},
          .last_modified{base::Time::Now() - base::Seconds(12)},
          .total_size{104},
      },
  };

  // Start the database open task.
  scoped_refptr<base::SequencedTaskRunner> database_task_runner =
      base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::WithBaseSyncPrimitives(),
           base::TaskShutdownBehavior::BLOCK_SHUTDOWN});

  // Open an in-memory LevelDB.
  base::test::TestFuture<DbStatus> open_status_future;
  std::unique_ptr<AsyncDomStorageDatabase> database =
      AsyncDomStorageDatabase::Open(
          StorageType::kLocalStorage, /*directory=*/base::FilePath(),
          "TestPendingTasks",
          /*memory_dump_id=*/std::nullopt, database_task_runner,
          open_status_future.GetCallback());

  // Immediately start using the database, which will enqueue pending tasks
  // while opening.
  DomStorageDatabase::Metadata cloned_metadata(
      CloneMapMetadataVector(kExpectedMapMetadata));

  base::test::TestFuture<DbStatus> write_status_future;
  database->PutMetadata(std::move(cloned_metadata),
                        write_status_future.GetCallback());

  // Start the task to read metadata from the database.
  base::test::TestFuture<StatusOr<DomStorageDatabase::Metadata>>
      metadata_future;
  database->ReadAllMetadata(metadata_future.GetCallback());

  const DbStatus& open_status = open_status_future.Get();
  const DbStatus& write_status = write_status_future.Get();
  StatusOr<DomStorageDatabase::Metadata> metadata = metadata_future.Take();

  EXPECT_TRUE(open_status.ok()) << open_status.ToString();
  EXPECT_TRUE(write_status.ok()) << write_status.ToString();
  ASSERT_TRUE(metadata.has_value()) << metadata.error().ToString();

  ExpectEqualsMapMetadataSpan(metadata->map_metadata, kExpectedMapMetadata);
  EXPECT_EQ(metadata->next_map_id, std::nullopt);
}

}  // namespace storage
