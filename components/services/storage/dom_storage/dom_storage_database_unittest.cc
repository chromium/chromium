// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/storage/dom_storage/dom_storage_database.h"

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/byte_size.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/task/bind_post_task.h"
#include "base/test/gmock_expected_support.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/threading/sequence_bound.h"
#include "components/services/storage/dom_storage/dom_storage_constants.h"
#include "components/services/storage/dom_storage/leveldb/dom_storage_database_leveldb.h"
#include "components/services/storage/dom_storage/leveldb/local_storage_leveldb.h"
#include "components/services/storage/dom_storage/leveldb/session_storage_leveldb.h"
#include "components/services/storage/dom_storage/sqlite/local_storage_sqlite.h"
#include "components/services/storage/dom_storage/sqlite/session_storage_sqlite.h"
#include "components/services/storage/dom_storage/test_support/dom_storage_database_testing.h"
#include "components/services/storage/filesystem_proxy_factory.h"
#include "components/services/storage/public/cpp/filesystem/filesystem_proxy.h"
#include "sql/database.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"

namespace storage {
namespace {

constexpr const char kFirstUrlString[] = "https://a-fake.test/";
constexpr const char kSecondUrlString[] = "https://b-fake.test/";

constexpr const char kFirstSessionId[] = "ce8c7dc5_73b4_4320_a506_ce1f4fd3356f";
constexpr const char kSecondSessionId[] =
    "36356e0b_1627_4492_a474_db76a8996bed";

constexpr int64_t kFirstMapId = 10;
constexpr int64_t kSecondMapId = 11;

constexpr base::ByteSize kMapTotalSize{312};

std::vector<uint8_t> ToBytes(std::string_view str) {
  return std::vector<uint8_t>(str.begin(), str.end());
}

}  // namespace

// Tests migrating DOM storage from LevelDB to SQLite for both local storage and
// session storage. Local storage uses a single global session ID and includes
// usage metadata. Session storage uses per-session IDs, includes cloned maps,
// and does not include usage metadata.
class DomStorageDatabaseTest : public testing::Test {
 protected:
  base::PassKey<DomStorageDatabaseFactory> GetPassKey() {
    return DomStorageDatabaseFactory::CreatePassKeyForTesting();
  }

  void OpenLocalStorageLevelDB(std::unique_ptr<LocalStorageLevelDB>* result) {
    auto instance = std::make_unique<LocalStorageLevelDB>(
        GetPassKey(), /*write_exp_tag=*/false);
    DbStatus status = instance->Open(/*directory=*/base::FilePath(),
                                     /*memory_dump_id=*/std::nullopt);
    ASSERT_TRUE(status.ok()) << status.ToString();
    *result = std::move(instance);
  }

  void OpenLocalStorageSqlite(std::unique_ptr<LocalStorageSqlite>* result) {
    auto instance = std::make_unique<LocalStorageSqlite>(GetPassKey());
    DbStatus status = instance->Open(
        /*database_path=*/base::FilePath(),
        /*memory_dump_id=*/std::nullopt);
    ASSERT_TRUE(status.ok()) << status.ToString();
    *result = std::move(instance);
  }

  void OpenSessionStorageLevelDB(
      std::unique_ptr<SessionStorageLevelDB>* result) {
    auto instance = std::make_unique<SessionStorageLevelDB>(
        GetPassKey(), /*write_exp_tag=*/false);
    DbStatus status = instance->Open(
        /*directory=*/base::FilePath(),
        /*memory_dump_id=*/std::nullopt);
    ASSERT_TRUE(status.ok()) << status.ToString();
    *result = std::move(instance);
  }

  // Expose private `DomStorageDatabaseFactory` migration helpers for testing.
  DbStatus MigrateDatabase(DomStorageDatabase& source,
                           DomStorageDatabase& destination) {
    return DomStorageDatabaseFactory::MigrateDatabase(source, destination);
  }

  StatusOr<std::unique_ptr<DomStorageDatabase>>
  CreateAndPopulateSqliteDatabaseForMigration(
      StorageType storage_type,
      DomStorageDatabase& source,
      const base::FilePath& destination_path) {
    return DomStorageDatabaseFactory::
        CreateAndPopulateSqliteDatabaseForMigration(storage_type, source,
                                                    destination_path);
  }

  DbStatus MoveTempSqliteToFinalLocation(
      const base::FilePath& sqlite_staging_path,
      const base::FilePath& sqlite_path) {
    return DomStorageDatabaseFactory::MoveTempSqliteToFinalLocation(
        sqlite_staging_path, sqlite_path);
  }

  DbStatus VerifyMigration(DomStorageDatabase& source,
                           DomStorageDatabase& destination) {
    return DomStorageDatabaseFactory::VerifyMigration(source, destination);
  }

  void OpenSessionStorageSqlite(std::unique_ptr<SessionStorageSqlite>* result) {
    auto instance = std::make_unique<SessionStorageSqlite>(GetPassKey());
    DbStatus status = instance->Open(
        /*database_path=*/base::FilePath(),
        /*memory_dump_id=*/std::nullopt);
    ASSERT_TRUE(status.ok()) << status.ToString();
    *result = std::move(instance);
  }

  const blink::StorageKey kFirstStorageKey =
      blink::StorageKey::CreateFromStringForTesting(kFirstUrlString);

  const blink::StorageKey kSecondStorageKey =
      blink::StorageKey::CreateFromStringForTesting(kSecondUrlString);

  const base::Time kMapLastAccessed = base::Time::Now() - base::Minutes(10);
  const base::Time kMapLastModified = base::Time::Now();

  base::test::TaskEnvironment task_environment_;
};

TEST_F(DomStorageDatabaseTest, MigrateLocalStorageWithEmptyDatabase) {
  std::unique_ptr<LocalStorageLevelDB> source;
  ASSERT_NO_FATAL_FAILURE(OpenLocalStorageLevelDB(&source));

  std::unique_ptr<LocalStorageSqlite> destination;
  ASSERT_NO_FATAL_FAILURE(OpenLocalStorageSqlite(&destination));

  DbStatus status = MigrateDatabase(*source, *destination);
  EXPECT_TRUE(status.ok()) << status.ToString();

  ASSERT_OK_AND_ASSIGN(DomStorageDatabase::Metadata metadata,
                       destination->ReadAllMetadata());
  EXPECT_TRUE(metadata.map_metadata.empty());
}

TEST_F(DomStorageDatabaseTest, MigrateLocalStorageWithSingleMap) {
  std::unique_ptr<LocalStorageLevelDB> source;
  ASSERT_NO_FATAL_FAILURE(OpenLocalStorageLevelDB(&source));

  // Write a map with key/value pairs and usage metadata to `source`.
  DomStorageDatabase::MapLocator map_locator{kFirstStorageKey};
  const std::map<DomStorageDatabase::Key, DomStorageDatabase::Value>
      kExpectedEntries = {
          {ToBytes("key_1"), ToBytes("value_1")},
          {ToBytes("key_2"), ToBytes("value_2")},
      };

  DomStorageDatabase::MapBatchUpdate::Usage usage;
  usage.SetLastAccessed(kMapLastAccessed);
  usage.SetLastModifiedAndTotalSize(kMapLastModified, kMapTotalSize);

  ASSERT_NO_FATAL_FAILURE(InsertMapEntries(*source, map_locator,
                                           kExpectedEntries, std::move(usage)));

  // Migrate from LevelDB to SQLite.
  std::unique_ptr<LocalStorageSqlite> destination;
  ASSERT_NO_FATAL_FAILURE(OpenLocalStorageSqlite(&destination));

  DbStatus status = MigrateDatabase(*source, *destination);
  EXPECT_TRUE(status.ok()) << status.ToString();

  // Verify key/value pairs were migrated.
  ASSERT_OK_AND_ASSIGN(
      (std::map<DomStorageDatabase::Key, DomStorageDatabase::Value> entries),
      destination->ReadMapKeyValues(map_locator.Clone()));
  EXPECT_EQ(entries, kExpectedEntries);

  // Verify usage metadata was migrated.
  ASSERT_OK_AND_ASSIGN(DomStorageDatabase::Metadata metadata,
                       destination->ReadAllMetadata());

  const DomStorageDatabase::MapMetadata kExpectedMapMetadata[] = {
      {
          .map_locator{kFirstStorageKey, /*map_id=*/1},
          .last_accessed{kMapLastAccessed},
          .last_modified{kMapLastModified},
          .total_size{kMapTotalSize},
      },
  };
  ExpectEqualsMapMetadataSpan(metadata.map_metadata, kExpectedMapMetadata);
}

TEST_F(DomStorageDatabaseTest, MigrateLocalStorageWithMultipleMaps) {
  std::unique_ptr<LocalStorageLevelDB> source;
  ASSERT_NO_FATAL_FAILURE(OpenLocalStorageLevelDB(&source));

  // Write key/value pairs and usage metadata for the first map.
  DomStorageDatabase::MapLocator first_locator{kFirstStorageKey};
  const std::map<DomStorageDatabase::Key, DomStorageDatabase::Value>
      kFirstEntries = {
          {ToBytes("key_a"), ToBytes("value_a")},
      };

  DomStorageDatabase::MapBatchUpdate::Usage first_usage;
  first_usage.SetLastModifiedAndTotalSize(kMapLastModified, kMapTotalSize);

  ASSERT_NO_FATAL_FAILURE(InsertMapEntries(
      *source, first_locator, kFirstEntries, std::move(first_usage)));

  // Write key/value pairs and usage metadata for the second map.
  DomStorageDatabase::MapLocator second_locator{kSecondStorageKey};
  const std::map<DomStorageDatabase::Key, DomStorageDatabase::Value>
      kSecondEntries = {
          {ToBytes("key_b"), ToBytes("value_b")},
          {ToBytes("key_c"), ToBytes("value_c")},
      };

  DomStorageDatabase::MapBatchUpdate::Usage second_usage;
  const base::Time kSecondLastAccessed = base::Time::Now() - base::Hours(1);
  second_usage.SetLastAccessed(kSecondLastAccessed);

  ASSERT_NO_FATAL_FAILURE(InsertMapEntries(
      *source, second_locator, kSecondEntries, std::move(second_usage)));

  // Migrate from LevelDB to SQLite.
  std::unique_ptr<LocalStorageSqlite> destination;
  ASSERT_NO_FATAL_FAILURE(OpenLocalStorageSqlite(&destination));

  DbStatus status = MigrateDatabase(*source, *destination);
  EXPECT_TRUE(status.ok()) << status.ToString();

  // Verify first map's entries.
  ASSERT_OK_AND_ASSIGN((std::map<DomStorageDatabase::Key,
                                 DomStorageDatabase::Value> first_entries),
                       destination->ReadMapKeyValues(first_locator.Clone()));
  EXPECT_EQ(first_entries, kFirstEntries);

  // Verify second map's entries.
  ASSERT_OK_AND_ASSIGN((std::map<DomStorageDatabase::Key,
                                 DomStorageDatabase::Value> second_entries),
                       destination->ReadMapKeyValues(second_locator.Clone()));
  EXPECT_EQ(second_entries, kSecondEntries);

  // Verify metadata for both maps.
  ASSERT_OK_AND_ASSIGN(DomStorageDatabase::Metadata metadata,
                       destination->ReadAllMetadata());
  ASSERT_EQ(metadata.map_metadata.size(), 2u);

  // Each map must have a unique ID.
  ASSERT_TRUE(metadata.map_metadata[0].map_locator.map_id().has_value());
  ASSERT_TRUE(metadata.map_metadata[1].map_locator.map_id().has_value());

  EXPECT_NE(metadata.map_metadata[0].map_locator.map_id().value(),
            metadata.map_metadata[1].map_locator.map_id().value());

  // Exclude map ID metadata verification because map IDs are not stable for
  // this migration test.
  std::vector<DomStorageDatabase::MapMetadata> actual_metadata_without_map_id;

  const DomStorageDatabase::MapMetadata& first_actual =
      metadata.map_metadata[0];

  actual_metadata_without_map_id.push_back({
      .map_locator{first_actual.map_locator.storage_key()},
      .last_accessed = first_actual.last_accessed,
      .last_modified = first_actual.last_modified,
      .total_size = first_actual.total_size,
  });

  const DomStorageDatabase::MapMetadata& second_actual =
      metadata.map_metadata[1];

  actual_metadata_without_map_id.push_back({
      .map_locator{second_actual.map_locator.storage_key()},
      .last_accessed = second_actual.last_accessed,
      .last_modified = second_actual.last_modified,
      .total_size = second_actual.total_size,
  });

  const DomStorageDatabase::MapMetadata kExpectedMapMetadata[] = {
      {
          .map_locator{kFirstStorageKey},
          .last_modified{kMapLastModified},
          .total_size{kMapTotalSize},
      },
      {
          .map_locator{kSecondStorageKey},
          .last_accessed{kSecondLastAccessed},
      },
  };
  ExpectEqualsMapMetadataSpan(actual_metadata_without_map_id,
                              kExpectedMapMetadata);
}

TEST_F(DomStorageDatabaseTest, MigrateSessionStorageWithEmptyDatabase) {
  std::unique_ptr<SessionStorageLevelDB> source;
  ASSERT_NO_FATAL_FAILURE(OpenSessionStorageLevelDB(&source));

  std::unique_ptr<SessionStorageSqlite> destination;
  ASSERT_NO_FATAL_FAILURE(OpenSessionStorageSqlite(&destination));

  DbStatus status = MigrateDatabase(*source, *destination);
  EXPECT_TRUE(status.ok()) << status.ToString();

  ASSERT_OK_AND_ASSIGN(DomStorageDatabase::Metadata metadata,
                       destination->ReadAllMetadata());
  EXPECT_TRUE(metadata.map_metadata.empty());
}

TEST_F(DomStorageDatabaseTest, MigrateSessionStorageWithSingleMap) {
  std::unique_ptr<SessionStorageLevelDB> source;
  ASSERT_NO_FATAL_FAILURE(OpenSessionStorageLevelDB(&source));

  // Write metadata and key/value pairs to `source`.
  const DomStorageDatabase::MapLocator kMapLocator{
      kFirstSessionId, kFirstStorageKey, kFirstMapId};
  const DomStorageDatabase::MapMetadata kExpectedMapMetadata[] = {
      {.map_locator = kMapLocator.Clone()},
  };

  DomStorageDatabase::Metadata metadata;
  metadata.map_metadata = CloneMapMetadataVector(kExpectedMapMetadata);

  DbStatus status = source->PutMetadata(std::move(metadata));
  ASSERT_TRUE(status.ok()) << status.ToString();

  const std::map<DomStorageDatabase::Key, DomStorageDatabase::Value>
      kExpectedEntries = {
          {ToBytes("key_1"), ToBytes("value_1")},
          {ToBytes("key_2"), ToBytes("value_2")},
      };
  ASSERT_NO_FATAL_FAILURE(
      InsertMapEntries(*source, kMapLocator.Clone(), kExpectedEntries));

  // Migrate from LevelDB to SQLite.
  std::unique_ptr<SessionStorageSqlite> destination;
  ASSERT_NO_FATAL_FAILURE(OpenSessionStorageSqlite(&destination));

  status = MigrateDatabase(*source, *destination);
  EXPECT_TRUE(status.ok()) << status.ToString();

  // Verify key/value pairs were migrated.
  ASSERT_OK_AND_ASSIGN(
      (std::map<DomStorageDatabase::Key, DomStorageDatabase::Value> entries),
      destination->ReadMapKeyValues(kMapLocator.Clone()));
  EXPECT_EQ(entries, kExpectedEntries);

  // Verify metadata was migrated.
  ASSERT_OK_AND_ASSIGN(DomStorageDatabase::Metadata dest_metadata,
                       destination->ReadAllMetadata());
  ExpectEqualsMapMetadataSpan(dest_metadata.map_metadata, kExpectedMapMetadata);
}

TEST_F(DomStorageDatabaseTest, MigrateSessionStorageWithClonedMap) {
  std::unique_ptr<SessionStorageLevelDB> source;
  ASSERT_NO_FATAL_FAILURE(OpenSessionStorageLevelDB(&source));

  // Create a map shared by two sessions (a cloned map).
  std::vector<DomStorageDatabase::MapMetadata> expected_map_metadata;
  expected_map_metadata.push_back({
      .map_locator{kFirstSessionId, kFirstStorageKey, kFirstMapId},
  });
  expected_map_metadata[0].map_locator.AddSession(kSecondSessionId);

  DomStorageDatabase::Metadata metadata;
  metadata.map_metadata = CloneMapMetadataVector(expected_map_metadata);

  DbStatus status = source->PutMetadata(std::move(metadata));
  ASSERT_TRUE(status.ok()) << status.ToString();

  // Write key/value pairs to the cloned map.
  const std::map<DomStorageDatabase::Key, DomStorageDatabase::Value>
      kExpectedEntries = {
          {ToBytes("shared_key"), ToBytes("shared_value")},
      };
  ASSERT_NO_FATAL_FAILURE(InsertMapEntries(
      *source, expected_map_metadata[0].map_locator.Clone(), kExpectedEntries));

  // Migrate from LevelDB to SQLite.
  std::unique_ptr<SessionStorageSqlite> destination;
  ASSERT_NO_FATAL_FAILURE(OpenSessionStorageSqlite(&destination));

  status = MigrateDatabase(*source, *destination);
  EXPECT_TRUE(status.ok()) << status.ToString();

  // Verify key/value pairs migrated.
  ASSERT_OK_AND_ASSIGN(
      (std::map<DomStorageDatabase::Key, DomStorageDatabase::Value> entries),
      destination->ReadMapKeyValues(
          expected_map_metadata[0].map_locator.Clone()));
  EXPECT_EQ(entries, kExpectedEntries);

  // Verify the cloned map's metadata migrated.
  ASSERT_OK_AND_ASSIGN(DomStorageDatabase::Metadata dest_metadata,
                       destination->ReadAllMetadata());
  ExpectEqualsMapMetadataSpan(dest_metadata.map_metadata,
                              expected_map_metadata);
}

TEST_F(DomStorageDatabaseTest, MigrateSessionStorageWithMultipleMaps) {
  std::unique_ptr<SessionStorageLevelDB> source;
  ASSERT_NO_FATAL_FAILURE(OpenSessionStorageLevelDB(&source));

  // Create two separate maps in different sessions.
  const DomStorageDatabase::MapLocator kFirstMapLocator{
      kFirstSessionId, kFirstStorageKey, kFirstMapId};

  const DomStorageDatabase::MapLocator kSecondMapLocator{
      kSecondSessionId, kSecondStorageKey, kSecondMapId};

  const DomStorageDatabase::MapMetadata kExpectedMapMetadata[] = {
      {.map_locator = kFirstMapLocator.Clone()},
      {.map_locator = kSecondMapLocator.Clone()},
  };

  DomStorageDatabase::Metadata metadata;
  metadata.map_metadata = CloneMapMetadataVector(kExpectedMapMetadata);

  DbStatus status = source->PutMetadata(std::move(metadata));
  ASSERT_TRUE(status.ok()) << status.ToString();

  // Write key/value pairs to each map.
  const std::map<DomStorageDatabase::Key, DomStorageDatabase::Value>
      kFirstEntries = {
          {ToBytes("key_a"), ToBytes("value_a")},
      };
  ASSERT_NO_FATAL_FAILURE(
      InsertMapEntries(*source, kFirstMapLocator.Clone(), kFirstEntries));

  const std::map<DomStorageDatabase::Key, DomStorageDatabase::Value>
      kSecondEntries = {
          {ToBytes("key_b"), ToBytes("value_b")},
          {ToBytes("key_c"), ToBytes("value_c")},
      };
  ASSERT_NO_FATAL_FAILURE(
      InsertMapEntries(*source, kSecondMapLocator.Clone(), kSecondEntries));

  // Migrate from LevelDB to SQLite.
  std::unique_ptr<SessionStorageSqlite> destination;
  ASSERT_NO_FATAL_FAILURE(OpenSessionStorageSqlite(&destination));

  status = MigrateDatabase(*source, *destination);
  EXPECT_TRUE(status.ok()) << status.ToString();

  // Verify first map's key/value pairs.
  ASSERT_OK_AND_ASSIGN((std::map<DomStorageDatabase::Key,
                                 DomStorageDatabase::Value> first_entries),
                       destination->ReadMapKeyValues(kFirstMapLocator.Clone()));
  EXPECT_EQ(first_entries, kFirstEntries);

  // Verify second map's key/value pairs.
  ASSERT_OK_AND_ASSIGN(
      (std::map<DomStorageDatabase::Key, DomStorageDatabase::Value>
           second_entries),
      destination->ReadMapKeyValues(kSecondMapLocator.Clone()));
  EXPECT_EQ(second_entries, kSecondEntries);

  // Verify metadata for both maps.
  ASSERT_OK_AND_ASSIGN(DomStorageDatabase::Metadata dest_metadata,
                       destination->ReadAllMetadata());
  ExpectEqualsMapMetadataSpan(dest_metadata.map_metadata, kExpectedMapMetadata);
}

TEST_F(DomStorageDatabaseTest, CreateAndPopulateSqliteDatabaseForMigration) {
  std::unique_ptr<SessionStorageLevelDB> source;
  ASSERT_NO_FATAL_FAILURE(OpenSessionStorageLevelDB(&source));

  // Write a key/value pair to the database.
  const DomStorageDatabase::MapLocator kMapLocator{
      kFirstSessionId, kFirstStorageKey, kFirstMapId};
  const std::map<DomStorageDatabase::Key, DomStorageDatabase::Value>
      kExpectedEntries = {
          {ToBytes("key"), ToBytes("value")},
      };
  ASSERT_NO_FATAL_FAILURE(
      InsertMapEntries(*source, kMapLocator, kExpectedEntries));

  // Migrate to a SQLite database.
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  const base::FilePath destination_path =
      temp_dir.GetPath().AppendASCII("migration.sqlite");

  ASSERT_OK_AND_ASSIGN(
      std::unique_ptr<DomStorageDatabase> destination,
      CreateAndPopulateSqliteDatabaseForMigration(StorageType::kSessionStorage,
                                                  *source, destination_path));

  // Verify the key/value pairs in the SQLite database.
  ASSERT_OK_AND_ASSIGN((std::map<DomStorageDatabase::Key,
                                 DomStorageDatabase::Value> actual_entries),
                       destination->ReadMapKeyValues(kMapLocator.Clone()));
  EXPECT_EQ(actual_entries, kExpectedEntries);
}

TEST_F(DomStorageDatabaseTest, MoveTempSqliteToFinalLocation) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  // Create a file.
  const base::FilePath sqlite_temp_path =
      temp_dir.GetPath().AppendASCII("migration.sqlite");
  ASSERT_TRUE(base::WriteFile(sqlite_temp_path, "migrated-main"));

  // Move the file.
  const base::FilePath sqlite_path =
      temp_dir.GetPath().AppendASCII("LocalStorage");
  DbStatus status =
      MoveTempSqliteToFinalLocation(sqlite_temp_path, sqlite_path);
  EXPECT_TRUE(status.ok()) << status.ToString();

  // Verify the contents of the moved file.
  std::string contents;
  ASSERT_TRUE(base::ReadFileToString(sqlite_path, &contents));
  EXPECT_EQ(contents, "migrated-main");
  EXPECT_FALSE(base::PathExists(sqlite_temp_path));
}

TEST_F(DomStorageDatabaseTest,
       MoveTempSqliteToFinalLocationReplacesExistingDatabase) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  // Write two files.
  const base::FilePath sqlite_path =
      temp_dir.GetPath().AppendASCII("LocalStorage");
  ASSERT_TRUE(base::WriteFile(sqlite_path, "stale-existing"));

  const base::FilePath sqlite_temp_path =
      temp_dir.GetPath().AppendASCII("migration.sqlite");
  ASSERT_TRUE(base::WriteFile(sqlite_temp_path, "migrated-main"));

  // Move one file to replace the other file.
  DbStatus status =
      MoveTempSqliteToFinalLocation(sqlite_temp_path, sqlite_path);
  ASSERT_TRUE(status.ok()) << status.ToString();

  // Verify the contents of the removed file.
  std::string contents;
  ASSERT_TRUE(base::ReadFileToString(sqlite_path, &contents));
  EXPECT_EQ(contents, "migrated-main");
  EXPECT_FALSE(base::PathExists(sqlite_temp_path));
}

TEST_F(DomStorageDatabaseTest,
       MoveTempSqliteToFinalLocationFailsWithoutTempFile) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  std::unique_ptr<FilesystemProxy> filesystem = CreateFilesystemProxy();
  DbStatus status = MoveTempSqliteToFinalLocation(
      temp_dir.GetPath().AppendASCII("migration.sqlite"),
      temp_dir.GetPath().AppendASCII("LocalStorage"));
  EXPECT_FALSE(status.ok());
}

TEST_F(DomStorageDatabaseTest, MigrationFailsOnMetadataMismatch) {
  // Use local storage because session storage does not persist usage metadata.
  std::unique_ptr<LocalStorageLevelDB> source;
  ASSERT_NO_FATAL_FAILURE(OpenLocalStorageLevelDB(&source));

  std::unique_ptr<LocalStorageSqlite> destination;
  ASSERT_NO_FATAL_FAILURE(OpenLocalStorageSqlite(&destination));

  // Write a key/value pair while updating the last accessed metadata.
  const DomStorageDatabase::MapLocator kMapLocator{kFirstStorageKey};
  const std::map<DomStorageDatabase::Key, DomStorageDatabase::Value> kEntries =
      {{ToBytes("key"), ToBytes("value")}};

  DomStorageDatabase::MapBatchUpdate::Usage source_usage;
  source_usage.SetLastAccessed(kMapLastAccessed);
  ASSERT_NO_FATAL_FAILURE(InsertMapEntries(*source, kMapLocator, kEntries,
                                           std::move(source_usage)));

  // Write the same key/value pair without the last accessed metadata.
  DomStorageDatabase::MapBatchUpdate::Usage destination_usage;
  destination_usage.SetLastAccessed(kMapLastAccessed - base::Days(1));
  ASSERT_NO_FATAL_FAILURE(InsertMapEntries(*destination, kMapLocator, kEntries,
                                           std::move(destination_usage)));

  // Verification must fail with missing metadata.
  DbStatus status = VerifyMigration(*source, *destination);
  EXPECT_FALSE(status.ok());
}

TEST_F(DomStorageDatabaseTest, VerifyMigrationFailsOnMapCountMismatch) {
  std::unique_ptr<SessionStorageLevelDB> source;
  ASSERT_NO_FATAL_FAILURE(OpenSessionStorageLevelDB(&source));

  std::unique_ptr<SessionStorageSqlite> destination;
  ASSERT_NO_FATAL_FAILURE(OpenSessionStorageSqlite(&destination));

  // Add a key/value pair to the `source` database but not the `destination`.
  DomStorageDatabase::Metadata metadata;
  metadata.map_metadata.push_back({
      .map_locator = DomStorageDatabase::MapLocator(
          kFirstSessionId, kFirstStorageKey, kFirstMapId),
  });
  DbStatus status = source->PutMetadata(std::move(metadata));
  ASSERT_TRUE(status.ok()) << status.ToString();

  // Migration must fail with missing key/value pairs.
  status = VerifyMigration(*source, *destination);
  EXPECT_FALSE(status.ok());
}

TEST_F(DomStorageDatabaseTest, VerifyMigrationIgnoresSessionIdOrder) {
  std::unique_ptr<SessionStorageLevelDB> source;
  ASSERT_NO_FATAL_FAILURE(OpenSessionStorageLevelDB(&source));

  std::unique_ptr<SessionStorageSqlite> destination;
  ASSERT_NO_FATAL_FAILURE(OpenSessionStorageSqlite(&destination));

  // Write the metadata for a map in both `source` and `destination`.  However,
  // put the session IDs in a different order.
  DomStorageDatabase::MapLocator source_locator{kFirstSessionId,
                                                kFirstStorageKey, kFirstMapId};
  source_locator.AddSession(kSecondSessionId);

  DomStorageDatabase::Metadata source_metadata;
  source_metadata.map_metadata.push_back(
      {.map_locator = source_locator.Clone()});
  DbStatus status = source->PutMetadata(std::move(source_metadata));
  ASSERT_TRUE(status.ok()) << status.ToString();

  // Add metadata to `destination` using a different order for the session IDs.
  DomStorageDatabase::MapLocator destination_locator{
      kSecondSessionId, kFirstStorageKey, kFirstMapId};
  destination_locator.AddSession(kFirstSessionId);

  DomStorageDatabase::Metadata destination_metadata;
  destination_metadata.map_metadata.push_back(
      {.map_locator = destination_locator.Clone()});
  status = destination->PutMetadata(std::move(destination_metadata));
  ASSERT_TRUE(status.ok()) << status.ToString();

  // Write the same key/value pairs to both databases.
  const std::map<DomStorageDatabase::Key, DomStorageDatabase::Value> kEntries =
      {{ToBytes("key"), ToBytes("value")}};
  ASSERT_NO_FATAL_FAILURE(InsertMapEntries(*source, source_locator, kEntries));
  ASSERT_NO_FATAL_FAILURE(
      InsertMapEntries(*destination, destination_locator, kEntries));

  // Migration must succeed when the metadata contains the same session IDs in a
  // different order.
  status = VerifyMigration(*source, *destination);
  EXPECT_TRUE(status.ok()) << status.ToString();
}

TEST_F(DomStorageDatabaseTest, VerifyMigrationFailsOnMapContentsMismatch) {
  std::unique_ptr<SessionStorageLevelDB> source;
  ASSERT_NO_FATAL_FAILURE(OpenSessionStorageLevelDB(&source));

  std::unique_ptr<SessionStorageSqlite> destination;
  ASSERT_NO_FATAL_FAILURE(OpenSessionStorageSqlite(&destination));

  // Write different key/value pairs to each database.
  const DomStorageDatabase::MapLocator kMapLocator{
      kFirstSessionId, kFirstStorageKey, kFirstMapId};
  ASSERT_NO_FATAL_FAILURE(InsertMapEntries(
      *source, kMapLocator, {{ToBytes("key"), ToBytes("source-value")}}));
  ASSERT_NO_FATAL_FAILURE(
      InsertMapEntries(*destination, kMapLocator,
                       {{ToBytes("key"), ToBytes("destination-value")}}));

  // Verification must fail when key/value pairs differ.
  DbStatus status = VerifyMigration(*source, *destination);
  EXPECT_FALSE(status.ok());
}

TEST_F(DomStorageDatabaseTest, CloseDbDropsLevelDbLock) {
  // Create and open a new LevelDB.
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  const base::FilePath leveldb_path = DomStorageDatabase::GetLevelDbPath(
      StorageType::kLocalStorage, temp_dir.GetPath());

  auto instance =
      std::make_unique<LocalStorageLevelDB>(GetPassKey(),
                                            /*write_exp_tag=*/false);
  DbStatus status =
      instance->Open(leveldb_path, /*memory_dump_id=*/std::nullopt);
  ASSERT_TRUE(status.ok()) << status.ToString();
  ASSERT_TRUE(base::PathExists(leveldb_path));

  // Close the database, which must release its file locks.
  instance->Close();
  EXPECT_TRUE(DomStorageDatabaseLevelDB::Destroy(leveldb_path).ok());
  EXPECT_FALSE(base::PathExists(leveldb_path));
}

}  // namespace storage
