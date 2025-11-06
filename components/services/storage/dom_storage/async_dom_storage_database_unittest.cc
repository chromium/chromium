// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/storage/dom_storage/async_dom_storage_database.h"

#include <memory>
#include <optional>
#include <vector>

#include "base/barrier_closure.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "components/services/storage/dom_storage/dom_storage_constants.h"
#include "components/services/storage/dom_storage/dom_storage_database.h"
#include "components/services/storage/dom_storage/leveldb/dom_storage_batch_operation_leveldb.h"
#include "components/services/storage/dom_storage/leveldb/dom_storage_database_leveldb.h"
#include "components/services/storage/dom_storage/leveldb/local_storage_leveldb.h"
#include "components/services/storage/dom_storage/test_support/dom_storage_database_testing.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"

namespace storage {

class AsyncDomStorageDatabaseTest : public testing::Test {
 public:
  AsyncDomStorageDatabaseTest() = default;
  ~AsyncDomStorageDatabaseTest() override = default;

 protected:
  base::test::TaskEnvironment task_environment_;
};

TEST_F(AsyncDomStorageDatabaseTest, ReadAllMetadata) {
  // Define test values to write to the database.
  const blink::StorageKey kFirstStorageKey =
      blink::StorageKey::CreateFromStringForTesting("https://a-fake-url.test");

  const blink::StorageKey kSecondStorageKey =
      blink::StorageKey::CreateFromStringForTesting("https://b-fake-url.test");

  const blink::StorageKey kThirdStorageKey =
      blink::StorageKey::CreateFromStringForTesting("https://c-fake-url.test");

  const blink::StorageKey kFourthStorageKey =
      blink::StorageKey::CreateFromStringForTesting("https://d-fake-url.test");

  const DomStorageDatabase::MapMetadata kExpectedMapMetadata[] = {
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

  // Open the database.
  std::unique_ptr<AsyncDomStorageDatabase> database;
  ASSERT_NO_FATAL_FAILURE(OpenAsyncDomStorageDatabaseInMemorySync(
      StorageType::kLocalStorage, &database));

  // Reading an empty database must succeed without results.
  DomStorageDatabase::Metadata all_metadata;
  ASSERT_NO_FATAL_FAILURE(ReadAllMetadataSync(*database, &all_metadata));
  EXPECT_EQ(all_metadata.map_metadata.size(), 0u);
  EXPECT_EQ(all_metadata.next_map_id, std::nullopt);

  // Write the metadata to the database.
  // TODO(crbug.com/377242771): Replace this with future `DomStorageDatabase`
  // based implementation.
  base::RunLoop run_loop;
  DbStatus status;
  database->RunDatabaseTask(
      base::BindOnce(
          base::BindLambdaForTesting([&](DomStorageDatabaseLevelDB& leveldb) {
            std::unique_ptr<DomStorageBatchOperationLevelDB> batch =
                leveldb.CreateBatchOperation();
            for (const auto& metadata : kExpectedMapMetadata) {
              const blink::StorageKey& storage_key =
                  metadata.map_locator.storage_key();
              if (metadata.last_accessed) {
                // Write a "METAACCESS:" entry.
                batch->Put(
                    LocalStorageLevelDB::CreateAccessMetaDataKey(storage_key),
                    LocalStorageLevelDB::CreateAccessMetaDataValue(
                        *metadata.last_accessed));
              }
              if (metadata.last_modified && metadata.total_size) {
                // Write a "META:" entry.
                batch->Put(
                    LocalStorageLevelDB::CreateWriteMetaDataKey(storage_key),
                    LocalStorageLevelDB::CreateWriteMetaDataValue(
                        *metadata.last_modified, *metadata.total_size));
              }
            }
            return batch->Commit();
          })),
      base::BindLambdaForTesting([&](DbStatus write_status) {
        status = std::move(write_status);
        run_loop.Quit();
      }));
  run_loop.Run();
  EXPECT_TRUE(status.ok()) << status.ToString();

  // Read the metadata from the database.
  ASSERT_NO_FATAL_FAILURE(ReadAllMetadataSync(*database, &all_metadata));
  ExpectEqualsMapMetadataSpan(all_metadata.map_metadata, kExpectedMapMetadata);
  EXPECT_EQ(all_metadata.next_map_id, std::nullopt);
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

  // Create a run loop that waits for 3 database tasks to complete: open, write
  // and read.
  base::RunLoop run_loop;
  base::RepeatingClosure task_completed_callback =
      base::BarrierClosure(3u, run_loop.QuitClosure());

  // Start the database open task.
  scoped_refptr<base::SequencedTaskRunner> database_task_runner =
      base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::WithBaseSyncPrimitives(),
           base::TaskShutdownBehavior::BLOCK_SHUTDOWN});

  DbStatus open_status;
  std::unique_ptr<AsyncDomStorageDatabase> database =
      AsyncDomStorageDatabase::Open(
          StorageType::kLocalStorage, /*directory=*/base::FilePath(),
          "TestPendingTasks",
          /*memory_dump_id=*/std::nullopt, database_task_runner,
          base::BindLambdaForTesting([&](DbStatus status) {
            open_status = std::move(status);
            task_completed_callback.Run();
          }));

  // Immediately start using the database, which will enqueue pending tasks
  // while opening.
  //
  // Start the task to write metadata to the database.
  // TODO(crbug.com/377242771): Replace this with future `DomStorageDatabase`
  // based implementation.
  DbStatus write_status;
  database->RunDatabaseTask(
      base::BindOnce(
          base::BindLambdaForTesting([&](DomStorageDatabaseLevelDB& leveldb) {
            std::unique_ptr<DomStorageBatchOperationLevelDB> batch =
                leveldb.CreateBatchOperation();
            for (const auto& metadata : kExpectedMapMetadata) {
              const blink::StorageKey& storage_key =
                  metadata.map_locator.storage_key();
              if (metadata.last_accessed) {
                // Write a "METAACCESS:" entry.
                batch->Put(
                    LocalStorageLevelDB::CreateAccessMetaDataKey(storage_key),
                    LocalStorageLevelDB::CreateAccessMetaDataValue(
                        *metadata.last_accessed));
              }
              if (metadata.last_modified && metadata.total_size) {
                // Write a "META:" entry.
                batch->Put(
                    LocalStorageLevelDB::CreateWriteMetaDataKey(storage_key),
                    LocalStorageLevelDB::CreateWriteMetaDataValue(
                        *metadata.last_modified, *metadata.total_size));
              }
            }
            return batch->Commit();
          })),
      base::BindLambdaForTesting([&](DbStatus status) {
        write_status = std::move(status);
        task_completed_callback.Run();
      }));

  // Start the task to read metadata from the database.
  StatusOr<DomStorageDatabase::Metadata> metadata;
  database->ReadAllMetadata(base::BindLambdaForTesting(
      [&](StatusOr<DomStorageDatabase::Metadata> result) {
        metadata = std::move(result);
        task_completed_callback.Run();
      }));

  // Wait for the open, read and write tasks to finish.
  run_loop.Run();

  EXPECT_TRUE(open_status.ok()) << open_status.ToString();
  EXPECT_TRUE(write_status.ok()) << write_status.ToString();
  ASSERT_TRUE(metadata.has_value()) << metadata.error().ToString();

  ExpectEqualsMapMetadataSpan(metadata->map_metadata, kExpectedMapMetadata);
  EXPECT_EQ(metadata->next_map_id, std::nullopt);
}

}  // namespace storage
