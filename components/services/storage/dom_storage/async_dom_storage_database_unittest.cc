// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/storage/dom_storage/async_dom_storage_database.h"

#include <map>
#include <memory>
#include <optional>
#include <vector>

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/bind.h"
#include "base/test/gmock_expected_support.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/test/with_feature_override.h"
#include "components/services/storage/dom_storage/dom_storage_constants.h"
#include "components/services/storage/dom_storage/dom_storage_database.h"
#include "components/services/storage/dom_storage/features.h"
#include "components/services/storage/dom_storage/test_support/dom_storage_database_testing.h"
#include "components/services/storage/dom_storage/test_support/scoped_dom_storage_database_factory_for_testing.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"

namespace storage {

namespace {
constexpr const char kFirstFakeUrlString[] = "https://a-fake-url.test";
constexpr const char kSecondFakeUrlString[] = "https://b-fake-url.test";
constexpr const char kThirdFakeUrlString[] = "https://c-fake-url.test";
constexpr const char kFourthFakeUrlString[] = "https://d-fake-url.test";

constexpr const char kFakeSessionId[] = "11111111_2222_3333_4444_555555555555";

// The inactivity timeout must elapse before an idle on-disk LevelDB database
// migrates to SQLite.
constexpr base::TimeDelta kExceedMigrationInactivityTimeout =
    kDomStorageSqliteMigrationInactivityTimeout +
    kDomStorageSqliteMigrationInactivityTimeout / 2;

std::vector<uint8_t> ToBytes(std::string source) {
  return std::vector<uint8_t>(source.begin(), source.end());
}

void OpenOnDiskDatabaseSync(StorageType storage_type,
                            const base::FilePath& storage_partition_dir,
                            std::unique_ptr<AsyncDomStorageDatabase>* result) {
  base::test::TestFuture<AsyncDomStorageDatabase::OpenOutcome> open_future;
  std::unique_ptr<AsyncDomStorageDatabase> database =
      AsyncDomStorageDatabase::Open(storage_type, storage_partition_dir,
                                    /*memory_dump_id=*/std::nullopt,
                                    /*dir_to_destroy=*/base::FilePath(),
                                    open_future.GetCallback());

  const DbStatus& status = open_future.Get().open_status;
  ASSERT_TRUE(status.ok()) << status.ToString();
  *result = std::move(database);
}

void SimulateMigrationFailure(
    StorageType,
    const base::FilePath&,
    const std::optional<base::trace_event::MemoryAllocatorDumpGuid>&,
    DomStorageDatabaseFactory::OpenResultCallback callback,
    DomStorageDatabase*) {
  std::move(callback).Run(DomStorageDatabaseFactory::OpenResult::FromError(
      DbStatus::IOError("migration failed")));
}

}  // namespace

class AsyncDomStorageDatabaseTest : public base::test::WithFeatureOverride,
                                    public testing::Test {
 public:
  AsyncDomStorageDatabaseTest();
  ~AsyncDomStorageDatabaseTest() override = default;

  bool IsSqliteEnabled() const { return GetParam(); }

 protected:
  base::test::ScopedFeatureList feature_list_;
  base::test::TaskEnvironment task_environment_;
  const blink::StorageKey kFirstStorageKey;
  const blink::StorageKey kSecondStorageKey;
  const blink::StorageKey kThirdStorageKey;
  const blink::StorageKey kFourthStorageKey;
};

INSTANTIATE_TEST_SUITE_P(
    /*no prefix*/,
    AsyncDomStorageDatabaseTest,
    testing::Bool(),
    /*name_generator=*/
    [](const testing::TestParamInfo<AsyncDomStorageDatabaseTest::ParamType>&
           info) { return info.param ? "SQLite" : "LevelDB"; });

AsyncDomStorageDatabaseTest::AsyncDomStorageDatabaseTest()
    : base::test::WithFeatureOverride(kDomStorageSqlite),
      kFirstStorageKey(
          blink::StorageKey::CreateFromStringForTesting(kFirstFakeUrlString)),
      kSecondStorageKey(
          blink::StorageKey::CreateFromStringForTesting(kSecondFakeUrlString)),
      kThirdStorageKey(
          blink::StorageKey::CreateFromStringForTesting(kThirdFakeUrlString)),
      kFourthStorageKey(
          blink::StorageKey::CreateFromStringForTesting(kFourthFakeUrlString)) {
  // Match the state of `kDomStorageSqliteInMemory` to the top level
  // kDomStorageSqlite. That way in-memory databases will use the backend
  // expected by the param state.
  if (IsSqliteEnabled()) {
    feature_list_.InitAndEnableFeature(kDomStorageSqliteInMemory);
  } else {
    feature_list_.InitAndDisableFeature(kDomStorageSqliteInMemory);
  }
}

TEST_P(AsyncDomStorageDatabaseTest,
       WriteAndReadThenDeleteLocalStorageMetadata) {
  // Define test values to write to the database.
  const DomStorageDatabase::MapMetadata kInitialMapMetadataArray[] = {
      {
          .map_locator{kFirstStorageKey},
          .last_accessed{base::Time::Now() - base::Days(7)},
      },
      {
          .map_locator{kSecondStorageKey},
          .last_modified{base::Time::Now() - base::Seconds(12)},
          .total_size{104},
      },
      {
          .map_locator{kThirdStorageKey},
          .last_accessed{base::Time::Now() - base::Minutes(15)},
      },
      {
          .map_locator{kFourthStorageKey},
          .last_accessed{base::Time::Now() - base::Minutes(30)},
          .last_modified{base::Time::Now() - base::Seconds(47)},
          .total_size{211114},
      },
  };
  const base::span<const DomStorageDatabase::MapMetadata> kInitialMapMetadata =
      kInitialMapMetadataArray;

  // Open the database.
  std::unique_ptr<AsyncDomStorageDatabase> database;
  ASSERT_NO_FATAL_FAILURE(OpenAsyncDomStorageDatabaseInMemorySync(
      StorageType::kLocalStorage, &database));

  // Write each map's metadata to the database.
  for (size_t i = 0; i < kInitialMapMetadata.size(); ++i) {
    // Write the metadata for a single map.
    DomStorageDatabase::Metadata cloned_metadata;
    cloned_metadata.map_metadata =
        CloneMapMetadataVector(base::span_from_ref(kInitialMapMetadata[i]));

    ASSERT_NO_FATAL_FAILURE(
        PutMetadataSync(*database, std::move(cloned_metadata)));

    // Read the metadata from the database.
    DomStorageDatabase::Metadata read_metadata;
    ASSERT_NO_FATAL_FAILURE(ReadAllMetadataSync(*database, &read_metadata));

    // Read back the metadata written so far.
    base::span<const DomStorageDatabase::MapMetadata> written_metadata_span =
        kInitialMapMetadata.first(/*count=*/i + 1);

    std::vector<DomStorageDatabase::MapMetadata> expected_map_metadata;
    if (IsSqliteEnabled()) {
      // Copy `written_metadata_span`, inserting the expected `map_id`.
      for (size_t j = 0u; j < written_metadata_span.size(); ++j) {
        const DomStorageDatabase::MapMetadata& written_metadata =
            written_metadata_span[j];

        expected_map_metadata.push_back({
            .map_locator{written_metadata.map_locator.storage_key(),
                         /*map_id=*/static_cast<int64_t>(j + 1)},
            .last_accessed = written_metadata.last_accessed,
            .last_modified = written_metadata.last_modified,
            .total_size = written_metadata.total_size,
        });
      }
    } else {
      // LevelDB does not create map IDs, associating maps by storage key only.
      expected_map_metadata = CloneMapMetadataVector(written_metadata_span);
    }

    ExpectEqualsMapMetadataSpan(read_metadata.map_metadata,
                                expected_map_metadata);
  }

  // Delete the first and third storage keys.
  std::vector<DomStorageDatabase::MapLocator> maps_to_delete;
  maps_to_delete.emplace_back(kFirstStorageKey);
  maps_to_delete.emplace_back(kThirdStorageKey);

  DeleteStorageKeysFromSessionSync(*database, /*session_id=*/std::string(),
                                   {kFirstStorageKey, kThirdStorageKey},
                                   std::move(maps_to_delete));

  DomStorageDatabase::Metadata read_metadata;
  ASSERT_NO_FATAL_FAILURE(ReadAllMetadataSync(*database, &read_metadata));

  // Add the second and fourth storage keys as expected.
  std::vector<DomStorageDatabase::MapMetadata> expected_metadata_after_delete;
  if (IsSqliteEnabled()) {
    // Copy the second and fourth `kInitialMapMetadata`, inserting the expected
    // `map_id`.
    expected_metadata_after_delete.push_back({
        .map_locator{kInitialMapMetadata[1].map_locator.storage_key(),
                     /*map_id=*/2},
        .last_accessed = kInitialMapMetadata[1].last_accessed,
        .last_modified = kInitialMapMetadata[1].last_modified,
        .total_size = kInitialMapMetadata[1].total_size,
    });

    expected_metadata_after_delete.push_back({
        .map_locator{kInitialMapMetadata[3].map_locator.storage_key(),
                     /*map_id=*/4},
        .last_accessed = kInitialMapMetadata[3].last_accessed,
        .last_modified = kInitialMapMetadata[3].last_modified,
        .total_size = kInitialMapMetadata[3].total_size,
    });
  } else {
    // LevelDB does not create map IDs, associating maps by storage key only.
    expected_metadata_after_delete.push_back(
        CloneMapMetadata(kInitialMapMetadata[1]));
    expected_metadata_after_delete.push_back(
        CloneMapMetadata(kInitialMapMetadata[3]));
  }

  ExpectEqualsMapMetadataSpan(read_metadata.map_metadata,
                              expected_metadata_after_delete);
}

TEST_P(AsyncDomStorageDatabaseTest, MapLocatorToDebugStringWithoutSessions) {
  DomStorageDatabase::MapLocator map_locator{"session_id1", kFirstStorageKey,
                                             /*map_id=*/216};
  map_locator.RemoveSession("session_id1");
  EXPECT_EQ(map_locator.ToDebugString(),
            "sessions_ids:, storage_key:{ origin: https://a-fake-url.test, "
            "top-level site: https://a-fake-url.test, nonce: <null>, ancestor "
            "chain bit: Same-Site }, map_id:216");
}

TEST_P(AsyncDomStorageDatabaseTest,
       MapLocatorToDebugStringWithMultipleSessions) {
  DomStorageDatabase::MapLocator map_locator{"session_id1", kFirstStorageKey,
                                             /*map_id=*/216};
  map_locator.AddSession("session_id2");
  EXPECT_EQ(map_locator.ToDebugString(),
            "sessions_ids:session_id1:session_id2, storage_key:{ origin: "
            "https://a-fake-url.test, top-level site: https://a-fake-url.test, "
            "nonce: <null>, ancestor chain bit: Same-Site }, "
            "map_id:216");
}

TEST_P(AsyncDomStorageDatabaseTest, MapLocatorToDebugStringWithoutMapId) {
  DomStorageDatabase::MapLocator map_locator{"session_id1", kFirstStorageKey};
  EXPECT_EQ(map_locator.ToDebugString(),
            "sessions_ids:session_id1, storage_key:{ origin: "
            "https://a-fake-url.test, top-level site: https://a-fake-url.test, "
            "nonce: <null>, ancestor chain bit: Same-Site }, "
            "map_id:null");
}

class AsyncDomStorageDatabaseMigrationTest : public testing::Test {
 public:
  AsyncDomStorageDatabaseMigrationTest() {
    feature_list_.InitAndEnableFeature(kDomStorageSqliteMigration);
    task_environment_ = std::make_unique<base::test::TaskEnvironment>(
        base::test::TaskEnvironment::TimeSource::MOCK_TIME);
  }

 protected:
  void SetUp() override { ASSERT_TRUE(temp_dir_.CreateUniqueTempDir()); }

  // Tests may provide their own migration implementation to simulate failures.
  DomStorageDatabaseFactory::MigrationCallback GetMigrationCallback() {
    return DomStorageDatabaseFactory::GetMigrationCallback();
  }

  // Creates an on-disk LevelDB with a single map.
  void CreateLevelDbDatabase(
      StorageType storage_type,
      const DomStorageDatabase::MapMetadata& map_metadata,
      const std::map<DomStorageDatabase::Key, DomStorageDatabase::Value>&
          map_entries,
      std::unique_ptr<AsyncDomStorageDatabase>* result_leveldb) {
    // Temporarily disable SQLite features to create the source LevelDB.
    {
      base::test::ScopedFeatureList disabled_feature_list;
      disabled_feature_list.InitWithFeatures(
          /*enabled_features=*/{},
          /*disabled_features=*/{kDomStorageSqliteMigration, kDomStorageSqlite,
                                 kDomStorageSqliteInMemory,
                                 kDomStorageSqliteNewDatabases});

      // Open the on-disk LevelDB.
      std::unique_ptr<AsyncDomStorageDatabase> database;
      ASSERT_NO_FATAL_FAILURE(
          OpenOnDiskDatabaseSync(storage_type, temp_dir_.GetPath(), &database));
      ASSERT_FALSE(database->is_sqlite());

      // Write metadata for the map.
      DomStorageDatabase::Metadata metadata;
      metadata.map_metadata.push_back(CloneMapMetadata(map_metadata));
      ASSERT_NO_FATAL_FAILURE(PutMetadataSync(*database, std::move(metadata)));

      // Write key/value entries for the map.
      FakeCommitter committer(database.get(), map_metadata.map_locator.Clone());
      for (const auto& [key, value] : map_entries) {
        ASSERT_NO_FATAL_FAILURE(committer.PutMapKeyValueSync(key, value));
      }
    }

    // Verify the LevelDB exists on-disk.
    const base::FilePath leveldb_path =
        DomStorageDatabase::GetLevelDbPath(storage_type, temp_dir_.GetPath());
    ASSERT_TRUE(base::PathExists(leveldb_path));

    // Re-open the source LevelDB to migrate.
    std::unique_ptr<AsyncDomStorageDatabase> database;
    ASSERT_NO_FATAL_FAILURE(
        OpenOnDiskDatabaseSync(storage_type, temp_dir_.GetPath(), &database));
    ASSERT_FALSE(database->is_sqlite());
    *result_leveldb = std::move(database);
  }

  struct ExpectedMap {
    const base::raw_ref<const DomStorageDatabase::MapMetadata> metadata;
    const base::raw_ref<
        const std::map<DomStorageDatabase::Key, DomStorageDatabase::Value>>
        entries;
  };

  void VerifyMigrationSuccess(AsyncDomStorageDatabase& database,
                              StorageType storage_type,
                              const std::vector<ExpectedMap>& expected_maps) {
    ASSERT_TRUE(database.is_sqlite());

    // The SQLite database must exist on-disk.
    const base::FilePath sqlite_path =
        DomStorageDatabase::GetSqlitePath(storage_type, temp_dir_.GetPath());
    EXPECT_TRUE(base::PathExists(sqlite_path));

    // The LevelDB must be deleted.
    const base::FilePath leveldb_path =
        DomStorageDatabase::GetLevelDbPath(storage_type, temp_dir_.GetPath());
    EXPECT_FALSE(base::PathExists(leveldb_path));

    // The temporary SQLite migration files must be deleted.
    EXPECT_FALSE(base::PathExists(
        sqlite_path.AddExtensionASCII(kSqliteMigrationStagingExtension)));

    // Verify metadata after migration.
    DomStorageDatabase::Metadata all_metadata;
    ASSERT_NO_FATAL_FAILURE(ReadAllMetadataSync(database, &all_metadata));
    ASSERT_EQ(all_metadata.map_metadata.size(), expected_maps.size());

    for (size_t i = 0; i < expected_maps.size(); ++i) {
      ExpectEqualsMapMetadata(all_metadata.map_metadata[i],
                              *expected_maps[i].metadata);

      // Verify the key/value pairs after migration.
      std::map<DomStorageDatabase::Key, DomStorageDatabase::Value>
          actual_map_entries;
      ASSERT_NO_FATAL_FAILURE(ReadMapKeyValuesSync(
          database, expected_maps[i].metadata->map_locator.Clone(),
          &actual_map_entries));
      EXPECT_EQ(actual_map_entries, *expected_maps[i].entries);
    }
  }

  void RunMigrateAfterInactivityTest(
      StorageType storage_type,
      const DomStorageDatabase::MapMetadata& map_metadata,
      const std::map<DomStorageDatabase::Key, DomStorageDatabase::Value>&
          map_entries) {
    const DomStorageDatabase::MapLocator& kMapLocator =
        map_metadata.map_locator;

    // Create the source LevelDB to migrate.
    std::unique_ptr<AsyncDomStorageDatabase> database;
    ASSERT_NO_FATAL_FAILURE(CreateLevelDbDatabase(storage_type, map_metadata,
                                                  map_entries, &database));

    // Verify the metadata before migration.
    DomStorageDatabase::Metadata actual_metadata;
    ASSERT_NO_FATAL_FAILURE(ReadAllMetadataSync(*database, &actual_metadata));
    ASSERT_EQ(actual_metadata.map_metadata.size(), 1u);

    DomStorageDatabase::MapMetadata expected_map_metadata =
        CloneMapMetadata(map_metadata);
    if (storage_type == StorageType::kLocalStorage) {
      // Remove the map ID from the expected value.  LevelDB does not record a
      // map ID for local storage.
      expected_map_metadata = {
          .map_locator{map_metadata.map_locator.storage_key()},
          .last_accessed = map_metadata.last_accessed,
          .last_modified = map_metadata.last_modified,
          .total_size = map_metadata.total_size,
      };
    }
    ExpectEqualsMapMetadata(actual_metadata.map_metadata[0],
                            expected_map_metadata);

    // Verify the key/value pairs before migration.
    std::map<DomStorageDatabase::Key, DomStorageDatabase::Value> entries;
    ASSERT_NO_FATAL_FAILURE(
        ReadMapKeyValuesSync(*database, kMapLocator.Clone(), &entries));
    EXPECT_EQ(entries, map_entries);

    // Trigger the migration.
    task_environment_->FastForwardBy(kExceedMigrationInactivityTimeout);
    EXPECT_TRUE(base::test::RunUntil([&]() { return database->is_sqlite(); }));

    ASSERT_NO_FATAL_FAILURE(VerifyMigrationSuccess(
        *database, storage_type,
        /*expected_maps=*/
        {
            {.metadata{map_metadata}, .entries{map_entries}},
        }));
  }

  const blink::StorageKey kStorageKey =
      blink::StorageKey::CreateFromStringForTesting(kFirstFakeUrlString);

  base::test::ScopedFeatureList feature_list_;

  // TaskEnvironment initialization results in threads calling
  // `FeatureList::IsEnabled()`. On Android tests, this can race with
  // `FeatureList::InitWithFeatureState()` in the constructor. So, we hold the
  // TaskEnvironment in a `unique_ptr` which allows us to delay its
  // initialization until after the feature list is set up.
  std::unique_ptr<base::test::TaskEnvironment> task_environment_;

  base::ScopedTempDir temp_dir_;
};

TEST_F(AsyncDomStorageDatabaseMigrationTest,
       MigrateLocalStorageAfterInactivity) {
  const DomStorageDatabase::MapMetadata kMapMetadata{
      .map_locator{kStorageKey, /*map_id=*/1},
      .last_accessed{base::Time::Now()},
      .last_modified{base::Time::Now() - base::Minutes(5)},
      .total_size{543},
  };
  const std::map<DomStorageDatabase::Key, DomStorageDatabase::Value>
      kMapEntries = {
          {ToBytes("key_1"), ToBytes("value_1")},
          {ToBytes("key_2"), ToBytes("value_2")},
      };
  ASSERT_NO_FATAL_FAILURE(RunMigrateAfterInactivityTest(
      StorageType::kLocalStorage, kMapMetadata, kMapEntries));
}

TEST_F(AsyncDomStorageDatabaseMigrationTest,
       MigratesSessionStorageAfterInactivity) {
  // Create the source LevelDB to migrate.
  const DomStorageDatabase::MapMetadata kMapMetadata{
      .map_locator{kFakeSessionId, kStorageKey, /*map_id=*/10},
  };
  const std::map<DomStorageDatabase::Key, DomStorageDatabase::Value>
      kMapEntries = {
          {ToBytes("key_1"), ToBytes("value_1")},
          {ToBytes("key_2"), ToBytes("value_2")},
      };
  ASSERT_NO_FATAL_FAILURE(RunMigrateAfterInactivityTest(
      StorageType::kSessionStorage, kMapMetadata, kMapEntries));
}

TEST_F(AsyncDomStorageDatabaseMigrationTest, NewDatabasesUseSqlite) {
  std::unique_ptr<AsyncDomStorageDatabase> database;
  ASSERT_NO_FATAL_FAILURE(OpenOnDiskDatabaseSync(
      StorageType::kLocalStorage, temp_dir_.GetPath(), &database));
  ASSERT_TRUE(database->is_sqlite());

  task_environment_->FastForwardBy(kExceedMigrationInactivityTimeout);
  ASSERT_TRUE(database->is_sqlite());
}

TEST_F(AsyncDomStorageDatabaseMigrationTest,
       DatabaseUsageResetsMigrationTimer) {
  // Create the source LevelDB to migrate.
  const DomStorageDatabase::MapMetadata kMapMetadata{
      .map_locator{kStorageKey, /*map_id=*/1},
      .last_accessed{base::Time::Now()},
      .last_modified{base::Time::Now() - base::Minutes(5)},
      .total_size{543},
  };
  const std::map<DomStorageDatabase::Key, DomStorageDatabase::Value>
      kMapEntries = {
          {ToBytes("key_1"), ToBytes("value_1")},
          {ToBytes("key_2"), ToBytes("value_2")},
      };
  std::unique_ptr<AsyncDomStorageDatabase> database;
  ASSERT_NO_FATAL_FAILURE(CreateLevelDbDatabase(
      StorageType::kLocalStorage, kMapMetadata, kMapEntries, &database));

  // Advance to just before the timeout, then perform an operation that resets
  // the migration timer.
  task_environment_->FastForwardBy(kDomStorageSqliteMigrationInactivityTimeout -
                                   base::Seconds(1));
  std::map<DomStorageDatabase::Key, DomStorageDatabase::Value> actual_entries;
  ASSERT_NO_FATAL_FAILURE(ReadMapKeyValuesSync(
      *database, DomStorageDatabase::MapLocator(kStorageKey), &actual_entries));
  EXPECT_EQ(actual_entries, kMapEntries);

  // Advance again to just before the timeout, leaving the total elapsed time
  // well past a single timeout.  Verify migration did not run because of the
  // database read above.
  task_environment_->FastForwardBy(kDomStorageSqliteMigrationInactivityTimeout -
                                   base::Seconds(1));
  EXPECT_FALSE(database->is_sqlite());

  // One more advance crosses the timeout and runs migration.
  task_environment_->FastForwardBy(kExceedMigrationInactivityTimeout);
  EXPECT_TRUE(base::test::RunUntil([&]() { return database->is_sqlite(); }));

  ASSERT_NO_FATAL_FAILURE(VerifyMigrationSuccess(
      *database, StorageType::kLocalStorage,
      /*expected_maps=*/
      {
          {.metadata{kMapMetadata}, .entries{kMapEntries}},
      }));
}

TEST_F(AsyncDomStorageDatabaseMigrationTest, PendingCommitsBlockMigration) {
  // Create the source LevelDB to migrate.
  const DomStorageDatabase::MapMetadata kMapMetadata{
      .map_locator{kStorageKey, /*map_id=*/1},
      .last_accessed{base::Time::Now()},
      .last_modified{base::Time::Now() - base::Minutes(5)},
      .total_size{543},
  };
  std::map<DomStorageDatabase::Key, DomStorageDatabase::Value> map_entries = {
      {ToBytes("key_1"), ToBytes("value_1")},
      {ToBytes("key_2"), ToBytes("value_2")},
  };
  std::unique_ptr<AsyncDomStorageDatabase> database;
  ASSERT_NO_FATAL_FAILURE(CreateLevelDbDatabase(
      StorageType::kLocalStorage, kMapMetadata, map_entries, &database));

  // Queue a commit that writes a key/value pair to the database.
  base::test::TestFuture<DbStatus> commit_future;
  FakeCommitter committer(database.get(),
                          DomStorageDatabase::MapLocator(kStorageKey));

  std::pair<DomStorageDatabase::Key, DomStorageDatabase::Value> pending_entry{
      ToBytes("pending-key"), ToBytes("pending-value")};
  committer.QueuePutMapKeyValue(/*key=*/pending_entry.first,
                                /*value=*/pending_entry.second,
                                commit_future.GetCallback());

  // The first idle timeout flushes the pending commit instead of migrating.
  task_environment_->FastForwardBy(kExceedMigrationInactivityTimeout);
  ASSERT_TRUE(commit_future.Get().ok());
  map_entries.insert(pending_entry);
  EXPECT_FALSE(database->is_sqlite());

  // The next idle timeout migrates all key/value pairs to SQLite.
  task_environment_->FastForwardBy(kExceedMigrationInactivityTimeout);
  EXPECT_TRUE(base::test::RunUntil([&]() { return database->is_sqlite(); }));

  ASSERT_NO_FATAL_FAILURE(VerifyMigrationSuccess(
      *database, StorageType::kLocalStorage,
      /*expected_maps=*/
      {
          {.metadata{kMapMetadata}, .entries{map_entries}},
      }));

  // Verify that writes still work after migration.
  FakeCommitter post_migration_committer(
      database.get(), DomStorageDatabase::MapLocator(kStorageKey));
  std::pair<DomStorageDatabase::Key, DomStorageDatabase::Value>
      post_migration_entry{ToBytes("post-migration-key"),
                           ToBytes("post-migration-value")};
  ASSERT_NO_FATAL_FAILURE(post_migration_committer.PutMapKeyValueSync(
      /*key=*/post_migration_entry.first,
      /*value=*/post_migration_entry.second));
  map_entries.insert(post_migration_entry);

  std::map<DomStorageDatabase::Key, DomStorageDatabase::Value> actual_entries;
  ReadMapKeyValuesSync(*database, DomStorageDatabase::MapLocator(kStorageKey),
                       &actual_entries);
  EXPECT_EQ(actual_entries, map_entries);
}

TEST_F(AsyncDomStorageDatabaseMigrationTest, QueuesOperationsDuringMigration) {
  // Create test data for a write operation that occurs during migration.
  const std::map<DomStorageDatabase::Key, DomStorageDatabase::Value>
      kEmptyEntries;
  const blink::StorageKey kOtherStorageKey =
      blink::StorageKey::CreateFromStringForTesting(kSecondFakeUrlString);
  const DomStorageDatabase::MapMetadata kWriteMapMetadata{
      .map_locator{kOtherStorageKey, /*map_id=*/2},
      .last_accessed{base::Time::Now()},
  };

  // Create the source LevelDB to migrate.
  const DomStorageDatabase::MapMetadata kMapMetadata{
      .map_locator{kStorageKey, /*map_id=*/1},
      .last_accessed{base::Time::Now()},
      .last_modified{base::Time::Now() - base::Minutes(5)},
      .total_size{543},
  };
  std::map<DomStorageDatabase::Key, DomStorageDatabase::Value> map_entries = {
      {ToBytes("key_1"), ToBytes("value_1")},
      {ToBytes("key_2"), ToBytes("value_2")},
  };
  std::unique_ptr<AsyncDomStorageDatabase> database;
  ASSERT_NO_FATAL_FAILURE(CreateLevelDbDatabase(
      StorageType::kLocalStorage, kMapMetadata, map_entries, &database));

  // Override the migration implementation, allowing this test to read and write
  // the database during migration.
  auto default_migration_callback = GetMigrationCallback();
  scoped_refptr<base::SequencedTaskRunner> test_task_runner =
      base::SequencedTaskRunner::GetCurrentDefault();

  base::test::TestFuture<
      StatusOr<std::map<DomStorageDatabase::Key, DomStorageDatabase::Value>>>
      read_future;
  auto read_future_callback = read_future.GetCallback();

  base::test::TestFuture<DbStatus> write_future;
  auto write_future_callback = write_future.GetCallback();

  ScopedDomStorageDatabaseFactoryForTesting scoped_database_factory(
      /*migration_callback=*/base::BindLambdaForTesting(
          [&](StorageType storage_type, const base::FilePath& dir_to_open,
              const std::optional<base::trace_event::MemoryAllocatorDumpGuid>&
                  memory_dump_id,
              DomStorageDatabaseFactory::OpenResultCallback callback,
              DomStorageDatabase* source) {
            // Migrate to SQLite.
            DomStorageDatabaseFactory::OpenResult migration_result;
            base::OnceCallback<void(DomStorageDatabaseFactory::OpenResult)>
                migration_completed_callback = base::BindLambdaForTesting(
                    [&](DomStorageDatabaseFactory::OpenResult open_result) {
                      migration_result = std::move(open_result);
                    });
            default_migration_callback.Run(
                storage_type, dir_to_open, memory_dump_id,
                std::move(migration_completed_callback), source);

            // Verify `migration_completed_callback` ran synchronously above.
            ASSERT_TRUE(migration_result.is_sqlite);
            ASSERT_TRUE(migration_result.open_status.ok())
                << migration_result.open_status.ToString();

            // Queue a database read and write before completing the test
            // override's migration callback.
            test_task_runner->PostTask(
                FROM_HERE,
                base::BindOnce(&AsyncDomStorageDatabase::ReadMapKeyValues,
                               base::Unretained(database.get()),
                               kMapMetadata.map_locator.Clone(),
                               std::move(read_future_callback)));

            DomStorageDatabase::Metadata metadata;
            metadata.map_metadata.push_back(
                CloneMapMetadata(kWriteMapMetadata));

            test_task_runner->PostTask(
                FROM_HERE, base::BindOnce(&AsyncDomStorageDatabase::PutMetadata,
                                          base::Unretained(database.get()),
                                          std::move(metadata),
                                          std::move(write_future_callback)));

            // Complete migration by running the test override's callback.
            std::move(callback).Run(std::move(migration_result));
          }));

  // Wait for migration to complete.
  task_environment_->FastForwardBy(kExceedMigrationInactivityTimeout);
  EXPECT_TRUE(base::test::RunUntil([&]() { return database->is_sqlite(); }));

  // Wait for the read operation to complete.
  ASSERT_OK_AND_ASSIGN((std::map<DomStorageDatabase::Key,
                                 DomStorageDatabase::Value> actual_entries),
                       read_future.Take());
  EXPECT_EQ(actual_entries, map_entries);

  // Wait for the write operation to complete.
  DbStatus put_metadata_status = write_future.Take();
  EXPECT_TRUE(put_metadata_status.ok()) << put_metadata_status.ToString();

  ASSERT_NO_FATAL_FAILURE(VerifyMigrationSuccess(
      *database, StorageType::kLocalStorage,
      /*expected_maps=*/
      {
          {.metadata{kMapMetadata}, .entries{map_entries}},
          {.metadata{kWriteMapMetadata}, .entries{kEmptyEntries}},
      }));
}

TEST_F(AsyncDomStorageDatabaseMigrationTest, MigrationFails) {
  // Create the source LevelDB to migrate.
  const DomStorageDatabase::MapMetadata kMapMetadata{
      .map_locator{kStorageKey},
      .last_accessed{base::Time::Now()},
      .last_modified{base::Time::Now() - base::Minutes(5)},
      .total_size{543},
  };
  std::map<DomStorageDatabase::Key, DomStorageDatabase::Value> map_entries = {
      {ToBytes("key_1"), ToBytes("value_1")},
      {ToBytes("key_2"), ToBytes("value_2")},
  };
  std::unique_ptr<AsyncDomStorageDatabase> database;
  ASSERT_NO_FATAL_FAILURE(CreateLevelDbDatabase(
      StorageType::kLocalStorage, kMapMetadata, map_entries, &database));

  // Simulate a failed migration.
  ScopedDomStorageDatabaseFactoryForTesting scoped_database_factory(
      /*migration_callback=*/base::BindRepeating(&SimulateMigrationFailure));
  task_environment_->FastForwardBy(kExceedMigrationInactivityTimeout);

  // Verify that migration failed.
  EXPECT_TRUE(base::test::RunUntil([&]() {
    return database->migration_state_ ==
           AsyncDomStorageDatabase::MigrationState::kAborted;
  }));
  EXPECT_FALSE(database->is_sqlite());

  // Verify the metadata after migration failure.
  DomStorageDatabase::Metadata all_metadata;
  ASSERT_NO_FATAL_FAILURE(ReadAllMetadataSync(*database, &all_metadata));
  ASSERT_EQ(all_metadata.map_metadata.size(), 1u);
  ExpectEqualsMapMetadata(all_metadata.map_metadata[0], kMapMetadata);

  // Verify the key/value pairs after migration failure.
  std::map<DomStorageDatabase::Key, DomStorageDatabase::Value> actual_entries;
  ReadMapKeyValuesSync(*database, DomStorageDatabase::MapLocator(kStorageKey),
                       &actual_entries);
  EXPECT_EQ(actual_entries, map_entries);

  // Verify that migration remains aborted without a retry.
  task_environment_->FastForwardBy(kExceedMigrationInactivityTimeout);
  EXPECT_FALSE(database->is_sqlite());
  EXPECT_EQ(database->migration_state_,
            AsyncDomStorageDatabase::MigrationState::kAborted);

  // Verify the LevelDB exists on-disk.
  const base::FilePath leveldb_path = DomStorageDatabase::GetLevelDbPath(
      StorageType::kLocalStorage, temp_dir_.GetPath());
  EXPECT_TRUE(base::PathExists(leveldb_path));

  // The SQLite database must not exist.
  const base::FilePath sqlite_path = DomStorageDatabase::GetSqlitePath(
      StorageType::kLocalStorage, temp_dir_.GetPath());
  EXPECT_FALSE(base::PathExists(sqlite_path));

  // The temporary SQLite migration files must not exist.
  EXPECT_FALSE(base::PathExists(
      sqlite_path.AddExtensionASCII(kSqliteMigrationStagingExtension)));
}

TEST_F(AsyncDomStorageDatabaseMigrationTest,
       QueuesOperationsDuringFailedMigration) {
  // Create test data for a write operation that occurs during migration.
  const std::map<DomStorageDatabase::Key, DomStorageDatabase::Value>
      kEmptyEntries;
  const blink::StorageKey kOtherStorageKey =
      blink::StorageKey::CreateFromStringForTesting(kSecondFakeUrlString);
  const DomStorageDatabase::MapMetadata kWriteMapMetadata{
      .map_locator{kOtherStorageKey},
      .last_accessed{base::Time::Now()},
  };
  // Create the source LevelDB to migrate.
  const DomStorageDatabase::MapMetadata kMapMetadata{
      .map_locator{kStorageKey},
      .last_accessed{base::Time::Now()},
      .last_modified{base::Time::Now() - base::Minutes(5)},
      .total_size{543},
  };
  std::map<DomStorageDatabase::Key, DomStorageDatabase::Value> map_entries = {
      {ToBytes("key_1"), ToBytes("value_1")},
      {ToBytes("key_2"), ToBytes("value_2")},
  };
  std::unique_ptr<AsyncDomStorageDatabase> database;
  ASSERT_NO_FATAL_FAILURE(CreateLevelDbDatabase(
      StorageType::kLocalStorage, kMapMetadata, map_entries, &database));

  // Override the migration implementation, allowing this test to read and write
  // the database during migration.
  base::test::TestFuture<
      StatusOr<std::map<DomStorageDatabase::Key, DomStorageDatabase::Value>>>
      read_future;
  auto read_future_callback = read_future.GetCallback();

  base::test::TestFuture<DbStatus> write_future;
  auto write_future_callback = write_future.GetCallback();

  scoped_refptr<base::SequencedTaskRunner> test_task_runner =
      base::SequencedTaskRunner::GetCurrentDefault();

  ScopedDomStorageDatabaseFactoryForTesting scoped_database_factory(
      /*migration_callback_=*/base::BindLambdaForTesting(
          [&](StorageType, const base::FilePath&,
              const std::optional<base::trace_event::MemoryAllocatorDumpGuid>&,
              DomStorageDatabaseFactory::OpenResultCallback callback,
              DomStorageDatabase*) {
            // Queue a database read and write before completing the test
            // override's migration callback.
            test_task_runner->PostTask(
                FROM_HERE,
                base::BindOnce(&AsyncDomStorageDatabase::ReadMapKeyValues,
                               base::Unretained(database.get()),
                               kMapMetadata.map_locator.Clone(),
                               std::move(read_future_callback)));

            DomStorageDatabase::Metadata metadata;
            metadata.map_metadata.push_back(
                CloneMapMetadata(kWriteMapMetadata));

            test_task_runner->PostTask(
                FROM_HERE, base::BindOnce(&AsyncDomStorageDatabase::PutMetadata,
                                          base::Unretained(database.get()),
                                          std::move(metadata),
                                          std::move(write_future_callback)));

            // Simulate migration failure.
            std::move(callback).Run(
                DomStorageDatabaseFactory::OpenResult::FromError(
                    DbStatus::IOError("migration failed")));
          }));

  // Wait for migration to fail.
  task_environment_->FastForwardBy(kExceedMigrationInactivityTimeout);
  EXPECT_TRUE(base::test::RunUntil([&]() {
    return database->migration_state_ ==
           AsyncDomStorageDatabase::MigrationState::kAborted;
  }));
  EXPECT_FALSE(database->is_sqlite());

  // Wait for the read operation to complete.
  ASSERT_OK_AND_ASSIGN((std::map<DomStorageDatabase::Key,
                                 DomStorageDatabase::Value> actual_entries),
                       read_future.Take());
  EXPECT_EQ(actual_entries, map_entries);

  // Wait for the write operation to complete.
  DbStatus put_metadata_status = write_future.Take();
  EXPECT_TRUE(put_metadata_status.ok()) << put_metadata_status.ToString();

  // Verify the metadata after migration fails.
  DomStorageDatabase::Metadata all_metadata;
  ASSERT_NO_FATAL_FAILURE(ReadAllMetadataSync(*database, &all_metadata));

  std::vector<DomStorageDatabase::MapMetadata> expected_map_metadata;
  expected_map_metadata.push_back(CloneMapMetadata(kMapMetadata));
  expected_map_metadata.push_back(CloneMapMetadata(kWriteMapMetadata));
  ExpectEqualsMapMetadataSpan(all_metadata.map_metadata, expected_map_metadata);
}

TEST(AsyncDomStorageDatabaseNoMigrationTest, NoMigrationWhenFeatureDisabled) {
  // Initialize test data to write to the database.
  const blink::StorageKey kStorageKey =
      blink::StorageKey::CreateFromStringForTesting(kFirstFakeUrlString);
  const DomStorageDatabase::MapLocator kMapLocator{kStorageKey};
  const std::map<DomStorageDatabase::Key, DomStorageDatabase::Value>
      kMapEntries = {
          {ToBytes("key_1"), ToBytes("value_1")},
      };

  // Disable the migration feature.
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{},
      /*disabled_features=*/{kDomStorageSqliteMigration, kDomStorageSqlite,
                             kDomStorageSqliteInMemory,
                             kDomStorageSqliteNewDatabases});

  base::test::TaskEnvironment task_environment{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  // Create a LevelDB local storage database.
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  std::unique_ptr<AsyncDomStorageDatabase> database;
  ASSERT_NO_FATAL_FAILURE(OpenOnDiskDatabaseSync(
      StorageType::kLocalStorage, temp_dir.GetPath(), &database));
  ASSERT_FALSE(database->is_sqlite());

  // Write key value pairs to the database.
  FakeCommitter committer(database.get(), kMapLocator.Clone());
  for (const auto& [key, value] : kMapEntries) {
    committer.PutMapKeyValueSync(key, value);
  }

  // Wait for migration to trigger.   Migration must not run when the feature is
  // disabled.
  task_environment.FastForwardBy(kExceedMigrationInactivityTimeout);
  EXPECT_FALSE(database->is_sqlite());

  // Verify the key/value pairs in the database.
  std::map<DomStorageDatabase::Key, DomStorageDatabase::Value> actual_entries;
  ReadMapKeyValuesSync(*database, kMapLocator.Clone(), &actual_entries);
  EXPECT_EQ(actual_entries, kMapEntries);
}

}  // namespace storage
