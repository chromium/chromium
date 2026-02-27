// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/storage/dom_storage/sqlite/local_storage_sqlite.h"

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/test/gmock_expected_support.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/services/storage/dom_storage/features.h"
#include "components/services/storage/dom_storage/sqlite/sqlite_database_utils.h"
#include "components/services/storage/dom_storage/test_support/dom_storage_database_testing.h"
#include "sql/statement.h"
#include "sql/test/test_helpers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"

namespace storage {

namespace {
constexpr const char kFirstUrlString[] = "https://a-fake.test/";
constexpr const char kSecondUrlString[] = "https://b-fake.test/";
constexpr const char kThirdUrlString[] = "https://c-fake.test/";
constexpr base::ByteSize kMapTotalSize{312};

std::vector<uint8_t> ToBytes(std::string_view str) {
  return std::vector<uint8_t>(str.begin(), str.end());
}

}  // namespace

class LocalStorageSqliteTest : public testing::Test {
 protected:
  LocalStorageSqliteTest();
  ~LocalStorageSqliteTest() override;

  // Creates a path to the SQLite database file under `temp_dir_`.  Creates
  // `temp_dir_` when necessary.
  void GetDatabasePath(base::FilePath* result);

  // Returns the `PassKey` required to create and open `LocalStorageSqlite`.
  base::PassKey<DomStorageDatabaseFactory> GetPassKey();

  void OpenOnDisk(std::unique_ptr<LocalStorageSqlite>* result);

  void OpenInMemory(std::unique_ptr<LocalStorageSqlite>* result);

  // Writes `metadata` to the database and verifies it was persisted correctly.
  void InitializeMetadata(DomStorageDatabase& database,
                          const DomStorageDatabase::Metadata& metadata);

  // Uses `DomStorageDatabase::UpdateMaps()` to write `metadata_to_update` to
  // the database.  Afterwards, verifies with
  // `DomStorageDatabase::ReadAllMetadata()`.
  void UpdateMapWithMetadata(
      LocalStorageSqlite& database,
      const DomStorageDatabase::MapMetadata& metadata_to_update);

  const blink::StorageKey kFirstStorageKey =
      blink::StorageKey::CreateFromStringForTesting(kFirstUrlString);

  const blink::StorageKey kSecondStorageKey =
      blink::StorageKey::CreateFromStringForTesting(kSecondUrlString);

  const blink::StorageKey kThirdStorageKey =
      blink::StorageKey::CreateFromStringForTesting(kThirdUrlString);

  const DomStorageDatabase::MapLocator kFirstMapLocator{kFirstStorageKey,
                                                        /*map_id=*/1};

  const DomStorageDatabase::MapLocator kSecondMapLocator{kSecondStorageKey,
                                                         /*map_id=*/2};

  const DomStorageDatabase::MapLocator kThirdMapLocator{kThirdStorageKey,
                                                        /*map_id=*/3};

  const base::Time kMapLastAccessed = base::Time::Now() - base::Minutes(10);
  const base::Time kMapLastModified = base::Time::Now();

  base::test::ScopedFeatureList scoped_feature_list_;
  base::ScopedTempDir temp_dir_;
  base::test::TaskEnvironment task_environment_;
};

LocalStorageSqliteTest::LocalStorageSqliteTest() {
  scoped_feature_list_.InitAndEnableFeature(kDomStorageSqlite);
}

LocalStorageSqliteTest::~LocalStorageSqliteTest() = default;

void LocalStorageSqliteTest::GetDatabasePath(base::FilePath* result) {
  if (!temp_dir_.IsValid()) {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
  }
  *result = DomStorageDatabase::GetPath(StorageType::kLocalStorage,
                                        temp_dir_.GetPath());
}

base::PassKey<DomStorageDatabaseFactory> LocalStorageSqliteTest::GetPassKey() {
  return DomStorageDatabaseFactory::CreatePassKeyForTesting();
}

void LocalStorageSqliteTest::OpenOnDisk(
    std::unique_ptr<LocalStorageSqlite>* result) {
  base::FilePath database_path;
  ASSERT_NO_FATAL_FAILURE(GetDatabasePath(&database_path));

  std::unique_ptr<LocalStorageSqlite> instance =
      std::make_unique<LocalStorageSqlite>(GetPassKey());

  DbStatus status = instance->Open(GetPassKey(),
                                   /*database_path=*/database_path,
                                   /*memory_dump_id=*/std::nullopt);

  ASSERT_TRUE(status.ok()) << status.ToString();
  *result = std::move(instance);
}

void LocalStorageSqliteTest::OpenInMemory(
    std::unique_ptr<LocalStorageSqlite>* result) {
  std::unique_ptr<LocalStorageSqlite> instance =
      std::make_unique<LocalStorageSqlite>(GetPassKey());

  DbStatus status = instance->Open(GetPassKey(),
                                   /*database_path=*/base::FilePath(),
                                   /*memory_dump_id=*/std::nullopt);

  ASSERT_TRUE(status.ok()) << status.ToString();
  *result = std::move(instance);
}

void LocalStorageSqliteTest::InitializeMetadata(
    DomStorageDatabase& database,
    const DomStorageDatabase::Metadata& metadata) {
  // Write `metadata` to `database`.
  DomStorageDatabase::Metadata metadata_to_write;
  metadata_to_write.next_map_id = metadata.next_map_id;
  metadata_to_write.map_metadata =
      CloneMapMetadataVector(metadata.map_metadata);

  DbStatus status = database.PutMetadata(std::move(metadata_to_write));
  EXPECT_TRUE(status.ok()) << status.ToString();

  // Read back the metadata from the database to verify persistence.
  ASSERT_OK_AND_ASSIGN(DomStorageDatabase::Metadata actual_metadata,
                       database.ReadAllMetadata());

  // Local storage does not use `next_map_id`.
  EXPECT_EQ(actual_metadata.next_map_id, std::nullopt);

  ExpectEqualsMapMetadataSpan(actual_metadata.map_metadata,
                              metadata.map_metadata);
}

void LocalStorageSqliteTest::UpdateMapWithMetadata(
    LocalStorageSqlite& database,
    const DomStorageDatabase::MapMetadata& metadata_to_update) {
  // Write the map usage metadata to the database.
  std::vector<DomStorageDatabase::MapBatchUpdate> map_update;
  map_update.emplace_back(metadata_to_update.map_locator.Clone());
  map_update.back().map_usage = DomStorageDatabase::MapBatchUpdate::Usage();

  if (metadata_to_update.last_accessed) {
    map_update.back().map_usage->SetLastAccessed(
        *metadata_to_update.last_accessed);
  }

  if (metadata_to_update.last_modified) {
    map_update.back().map_usage->SetLastModifiedAndTotalSize(
        *metadata_to_update.last_modified, *metadata_to_update.total_size);
  }

  DbStatus status = database.UpdateMaps(std::move(map_update));
  EXPECT_TRUE(status.ok()) << status.ToString();

  // Read back the map usage metadata from the database.
  ASSERT_OK_AND_ASSIGN(DomStorageDatabase::Metadata all_metadata,
                       database.ReadAllMetadata());
  EXPECT_EQ(all_metadata.next_map_id, std::nullopt);
  ExpectEqualsMapMetadataSpan(all_metadata.map_metadata,
                              base::span_from_ref(metadata_to_update));
}

TEST_F(LocalStorageSqliteTest, OpenInMemory) {
  std::unique_ptr<LocalStorageSqlite> database;
  ASSERT_NO_FATAL_FAILURE(OpenInMemory(&database));
}

TEST_F(LocalStorageSqliteTest, OpenThenDestroyOnDisk) {
  std::unique_ptr<LocalStorageSqlite> database;
  ASSERT_NO_FATAL_FAILURE(OpenOnDisk(&database));
  database.reset();

  base::FilePath database_path;
  ASSERT_NO_FATAL_FAILURE(GetDatabasePath(&database_path));
  EXPECT_TRUE(base::PathExists(database_path));

  DbStatus status = sqlite::DestroyDatabase(database_path);
  EXPECT_TRUE(status.ok()) << status.ToString();
  EXPECT_FALSE(base::PathExists(database_path));
}

TEST_F(LocalStorageSqliteTest, VersionTooNew) {
  base::FilePath database_path;
  ASSERT_NO_FATAL_FAILURE(GetDatabasePath(&database_path));

  // Write the wrong version to the database
  std::unique_ptr<LocalStorageSqlite> database;
  ASSERT_NO_FATAL_FAILURE(OpenOnDisk(&database));
  database->PutVersionForTesting(9999);
  database.reset();

  // Opening the database with the wrong version must fail.
  database = std::make_unique<LocalStorageSqlite>(GetPassKey());
  DbStatus status = database->Open(GetPassKey(),
                                   /*database_path=*/database_path,
                                   /*memory_dump_id=*/std::nullopt);
  EXPECT_TRUE(status.IsNotFound());
}

// Verifies that reading metadata from an empty database returns default values:
// `next_map_id` should be `std::nullopt` and `map_metadata` should be empty.
TEST_F(LocalStorageSqliteTest, ReadAllMetadataWithEmpty) {
  std::unique_ptr<LocalStorageSqlite> database;
  ASSERT_NO_FATAL_FAILURE(OpenInMemory(&database));

  ASSERT_OK_AND_ASSIGN(DomStorageDatabase::Metadata metadata,
                       database->ReadAllMetadata());

  EXPECT_EQ(metadata.next_map_id, std::nullopt);
  EXPECT_EQ(metadata.map_metadata.size(), 0u);
}

// Verifies that `PutMetadata()` correctly persists map metadata for a single
// storage key, and that subsequent calls with the same storage key update the
// existing row rather than creating a new one. Also verifies that `COALESCE`
// preserves existing field values when new values are `NULL`, allowing partial
// updates (e.g., updating `last_modified` without overwriting `last_accessed`).
TEST_F(LocalStorageSqliteTest, PutMetadataThenUpdate) {
  std::unique_ptr<LocalStorageSqlite> database;
  ASSERT_NO_FATAL_FAILURE(OpenInMemory(&database));

  const DomStorageDatabase::MapMetadata kInitialMapMetadata[] = {
      {
          .map_locator{kFirstMapLocator.Clone()},
          .last_accessed = base::Time::UnixEpoch() + base::Days(1),
      },
  };
  std::vector<DomStorageDatabase::MapMetadata> expected_map_metadata =
      CloneMapMetadataVector(kInitialMapMetadata);

  ASSERT_NO_FATAL_FAILURE(InitializeMetadata(
      *database, DomStorageDatabase::Metadata(
                     CloneMapMetadataVector(expected_map_metadata))));

  // Update the metadata with `last_modified` and `total_size` (without
  // `last_accessed`). This should preserve `last_accessed` due to `COALESCE`.
  expected_map_metadata[0].last_modified =
      base::Time::UnixEpoch() + base::Days(20);
  expected_map_metadata[0].total_size = base::ByteSize(500u);

  // Verify the update preserved existing fields and added new ones.
  ASSERT_NO_FATAL_FAILURE(InitializeMetadata(
      *database, DomStorageDatabase::Metadata(
                     CloneMapMetadataVector(expected_map_metadata))));
}

// Verifies that `PutMetadata()` correctly handles multiple storage keys in a
// single call, each with different combinations of optional fields set. Also
// verifies that each storage key gets a unique `row_id` (used as `map_id`).
TEST_F(LocalStorageSqliteTest, PutMetadataWithMultipleMaps) {
  std::unique_ptr<LocalStorageSqlite> database;
  ASSERT_NO_FATAL_FAILURE(OpenInMemory(&database));

  // Insert metadata for multiple storage keys in a single call.
  const DomStorageDatabase::MapMetadata kExpectedMapMetadata[] = {
      {
          .map_locator{kFirstMapLocator.Clone()},
          .last_accessed = base::Time::UnixEpoch() + base::Days(1),
          .last_modified = base::Time::UnixEpoch() + base::Days(2),
          .total_size = base::ByteSize(100u),
      },
      {
          .map_locator{kSecondMapLocator.Clone()},
          .last_accessed = base::Time::UnixEpoch() + base::Days(10),
          .last_modified = base::Time::UnixEpoch() + base::Days(20),
          .total_size = base::ByteSize(500u),
      },
      {
          .map_locator{kThirdMapLocator.Clone()},
          .last_accessed = base::Time::UnixEpoch() + base::Days(100),
      },
  };
  ASSERT_NO_FATAL_FAILURE(InitializeMetadata(
      *database, DomStorageDatabase::Metadata(
                     CloneMapMetadataVector(kExpectedMapMetadata))));
}

// Verifies that metadata written to the database is persisted across database
// close and reopen operations.
TEST_F(LocalStorageSqliteTest, MetadataPersistence) {
  const DomStorageDatabase::MapMetadata kExpectedMapMetadata[] = {
      {
          .map_locator{kFirstMapLocator.Clone()},
          .last_accessed = base::Time::UnixEpoch() + base::Days(1),
          .last_modified = base::Time::UnixEpoch() + base::Days(2),
          .total_size = base::ByteSize(100u),
      },
      {
          .map_locator{kSecondMapLocator.Clone()},
          .last_accessed = base::Time::UnixEpoch() + base::Days(10),
      },
  };

  // Open database and write metadata.
  {
    std::unique_ptr<LocalStorageSqlite> database;
    ASSERT_NO_FATAL_FAILURE(OpenOnDisk(&database));

    ASSERT_NO_FATAL_FAILURE(InitializeMetadata(
        *database, DomStorageDatabase::Metadata(
                       CloneMapMetadataVector(kExpectedMapMetadata))));
  }

  // Re-open the database and verify metadata persisted.
  {
    std::unique_ptr<LocalStorageSqlite> database;
    ASSERT_NO_FATAL_FAILURE(OpenOnDisk(&database));

    ASSERT_OK_AND_ASSIGN(DomStorageDatabase::Metadata read_metadata,
                         database->ReadAllMetadata());

    ExpectEqualsMapMetadataSpan(read_metadata.map_metadata,
                                kExpectedMapMetadata);
    EXPECT_EQ(read_metadata.next_map_id, std::nullopt);
  }
}

// Verifies that `ReadAllMetadata()` returns a corruption error when the
// database contains a storage key that cannot be deserialized.
TEST_F(LocalStorageSqliteTest, ReadAllMetadataWithInvalidStorageKey) {
  // Write valid metadata to ensure the table exists and works.
  {
    std::unique_ptr<LocalStorageSqlite> database;
    ASSERT_NO_FATAL_FAILURE(OpenOnDisk(&database));

    DomStorageDatabase::Metadata valid_metadata;
    valid_metadata.map_metadata.push_back({
        .map_locator{kFirstMapLocator.Clone()},
        .last_accessed = base::Time::UnixEpoch() + base::Days(1),
    });
    DbStatus status = database->PutMetadata(std::move(valid_metadata));
    EXPECT_TRUE(status.ok()) << status.ToString();
  }

  // Use `sql::Database` directly to insert an invalid storage key.
  {
    base::FilePath database_path;
    ASSERT_NO_FATAL_FAILURE(GetDatabasePath(&database_path));

    sql::Database database(sql::test::kTestTag);
    EXPECT_TRUE(database.Open(database_path));

    static constexpr char kInsertInvalidStorageKey[] =
        "INSERT INTO maps (storage_key, last_accessed) VALUES (?, ?)";

    sql::Statement statement(
        database.GetUniqueStatement(kInsertInvalidStorageKey));
    statement.BindBlob(
        0, /*storage_key=*/std::vector<uint8_t>{0xFF, 0xFE, 0x00, 0x01});
    statement.BindTime(1, /*last_accessed=*/base::Time::UnixEpoch());

    EXPECT_TRUE(statement.Run());
  }

  // Re-open the database and verify that `ReadAllMetadata()` returns a
  // corruption error due to the invalid storage key.
  {
    std::unique_ptr<LocalStorageSqlite> database;
    ASSERT_NO_FATAL_FAILURE(OpenOnDisk(&database));

    StatusOr<DomStorageDatabase::Metadata> result = database->ReadAllMetadata();
    ASSERT_FALSE(result.has_value());
    EXPECT_TRUE(result.error().IsCorruption());
  }
}

// Verifies that `ReadAllMetadata()` returns a corruption error when the
// database contains a `total_size` value that cannot be represented as a
// `base::ByteSize` (e.g., a negative value).
TEST_F(LocalStorageSqliteTest, ReadAllMetadataWithInvalidTotalSize) {
  // Write valid metadata to ensure the table exists and works.
  {
    std::unique_ptr<LocalStorageSqlite> database;
    ASSERT_NO_FATAL_FAILURE(OpenOnDisk(&database));

    DomStorageDatabase::Metadata valid_metadata;
    valid_metadata.map_metadata.push_back({
        .map_locator{kFirstMapLocator.Clone()},
        .last_accessed = base::Time::UnixEpoch() + base::Days(1),
    });
    DbStatus status = database->PutMetadata(std::move(valid_metadata));
    EXPECT_TRUE(status.ok()) << status.ToString();
  }

  // Use `sql::Database` directly to insert a row with a negative `total_size`.
  {
    base::FilePath database_path;
    ASSERT_NO_FATAL_FAILURE(GetDatabasePath(&database_path));

    sql::Database database(sql::test::kTestTag);
    EXPECT_TRUE(database.Open(database_path));

    static constexpr char kInsertInvalidTotalSize[] =
        "INSERT INTO maps (storage_key, last_accessed, total_size) "
        "VALUES (?, ?, ?)";

    sql::Statement statement(
        database.GetUniqueStatement(kInsertInvalidTotalSize));
    statement.BindBlob(0, kSecondStorageKey.Serialize());
    statement.BindTime(1, /*last_accessed=*/base::Time::Now());
    statement.BindInt64(2, /*total_size=*/-100);

    EXPECT_TRUE(statement.Run());
  }

  // Re-open the database and verify that `ReadAllMetadata()` returns a
  // corruption error due to the invalid total_size.
  {
    std::unique_ptr<LocalStorageSqlite> database;
    ASSERT_NO_FATAL_FAILURE(OpenOnDisk(&database));

    StatusOr<DomStorageDatabase::Metadata> result = database->ReadAllMetadata();
    ASSERT_FALSE(result.has_value());
    EXPECT_TRUE(result.error().IsCorruption());
  }
}

// Verifies that `UpdateMaps()` correctly adds, modifies, and deletes key/value
// pairs across multiple maps.
TEST_F(LocalStorageSqliteTest, UpdateMaps) {
  std::unique_ptr<LocalStorageSqlite> database;
  ASSERT_NO_FATAL_FAILURE(OpenInMemory(&database));

  DomStorageDatabase::MapLocator map1_locator{kFirstStorageKey, /*map_id=*/1};
  DomStorageDatabase::MapLocator map2_locator{kSecondStorageKey, /*map_id=*/2};
  ASSERT_NO_FATAL_FAILURE(
      TestUpdateMaps(*database, map1_locator, map2_locator));
}

// Verifies that `UpdateMaps()` correctly persists the access metadata
// (`last_accessed` timestamp) for a map.
TEST_F(LocalStorageSqliteTest, UpdateMapsWithAccessMetadata) {
  std::unique_ptr<LocalStorageSqlite> database;
  ASSERT_NO_FATAL_FAILURE(OpenInMemory(&database));

  const DomStorageDatabase::MapMetadata kExpectedMapMetadata{
      .map_locator{kFirstMapLocator.Clone()},
      .last_accessed{kMapLastAccessed},
  };
  ASSERT_NO_FATAL_FAILURE(
      UpdateMapWithMetadata(*database, kExpectedMapMetadata));
}

// Verifies that `UpdateMaps()` correctly persists write metadata
// (`last_modified` timestamp and `total_size`) for a map.
TEST_F(LocalStorageSqliteTest, UpdateMapsWithWriteMetadata) {
  std::unique_ptr<LocalStorageSqlite> database;
  ASSERT_NO_FATAL_FAILURE(OpenInMemory(&database));

  const DomStorageDatabase::MapMetadata kExpectedMapMetadata{
      .map_locator{kFirstMapLocator.Clone()},
      .last_modified{kMapLastModified},
      .total_size{kMapTotalSize},
  };
  ASSERT_NO_FATAL_FAILURE(
      UpdateMapWithMetadata(*database, kExpectedMapMetadata));
}

// Verifies that `UpdateMaps()` with `DeleteAllUsage()` removes the map row
// from the database entirely, including all usage metadata fields
// (`last_accessed`, `last_modified`, `total_size`).
TEST_F(LocalStorageSqliteTest, UpdateMapsClearsMetadata) {
  std::unique_ptr<LocalStorageSqlite> database;
  ASSERT_NO_FATAL_FAILURE(OpenInMemory(&database));

  const DomStorageDatabase::MapMetadata kInitialMapMetadata{
      .map_locator{kFirstMapLocator.Clone()},
      .last_accessed{kMapLastAccessed},
      .last_modified{kMapLastModified},
      .total_size{kMapTotalSize},
  };
  ASSERT_NO_FATAL_FAILURE(
      UpdateMapWithMetadata(*database, kInitialMapMetadata));

  // Use `UpdateMaps()` with `DeleteAllUsage()` to delete the map row from the
  // database.
  std::vector<DomStorageDatabase::MapBatchUpdate> delete_metadata_update;
  delete_metadata_update.emplace_back(kInitialMapMetadata.map_locator.Clone());

  delete_metadata_update.back().map_usage =
      DomStorageDatabase::MapBatchUpdate::Usage();
  delete_metadata_update.back().map_usage->DeleteAllUsage();

  DbStatus status = database->UpdateMaps(std::move(delete_metadata_update));
  EXPECT_TRUE(status.ok()) << status.ToString();

  // Verify the map row has been deleted from the database.
  ASSERT_OK_AND_ASSIGN(DomStorageDatabase::Metadata all_metadata,
                       database->ReadAllMetadata());

  EXPECT_EQ(all_metadata.next_map_id, std::nullopt);
  EXPECT_EQ(all_metadata.map_metadata.size(), 0u);
}

// Verifies that `DeleteStorageKeysFromSession()` correctly removes metadata
// for a storage key when no map key/value pairs exist.
TEST_F(LocalStorageSqliteTest, DeleteStorageKeysFromSessionWithMetadata) {
  std::unique_ptr<LocalStorageSqlite> database;
  ASSERT_NO_FATAL_FAILURE(OpenInMemory(&database));

  // Write metadata for a storage key.
  ASSERT_NO_FATAL_FAILURE(InitializeMetadata(
      *database, DomStorageDatabase::Metadata(CloneMapMetadataVector({
                     {
                         .map_locator{kFirstMapLocator.Clone()},
                         .last_accessed{kMapLastAccessed},
                         .last_modified{kMapLastModified},
                         .total_size{kMapTotalSize},
                     },
                 }))));

  // Delete the storage key's metadata.
  std::vector<DomStorageDatabase::MapLocator> maps_to_delete;
  maps_to_delete.push_back(kFirstMapLocator.Clone());

  DbStatus status = database->DeleteStorageKeysFromSession(
      /*session_id*/ std::string(), /*metadata_to_delete=*/{kFirstStorageKey},
      std::move(maps_to_delete));
  EXPECT_TRUE(status.ok()) << status.ToString();

  // Verify the metadata was removed.
  ASSERT_OK_AND_ASSIGN(DomStorageDatabase::Metadata read_metadata,
                       database->ReadAllMetadata());
  EXPECT_EQ(read_metadata.map_metadata.size(), 0u);
}

// Verifies that `DeleteStorageKeysFromSession()` correctly removes both
// metadata and map key/value pairs for a storage key when the map is included
// in `maps_to_delete`.
TEST_F(LocalStorageSqliteTest, DeleteStorageKeysFromSessionWithMapKeyValues) {
  std::unique_ptr<LocalStorageSqlite> database;
  ASSERT_NO_FATAL_FAILURE(OpenInMemory(&database));

  // Write metadata for a storage key.
  ASSERT_NO_FATAL_FAILURE(InitializeMetadata(
      *database, DomStorageDatabase::Metadata(CloneMapMetadataVector({
                     {
                         .map_locator{kFirstMapLocator.Clone()},
                         .last_accessed{kMapLastAccessed},
                     },
                 }))));

  // Add some key/value pairs to the map.
  const std::map<DomStorageDatabase::Key, DomStorageDatabase::Value>
      kMapEntries{
          {ToBytes("key_1"), ToBytes("value_1")},
          {ToBytes("key_2"), ToBytes("value_2")},
      };
  ASSERT_NO_FATAL_FAILURE(
      InsertMapEntries(*database, kFirstMapLocator.Clone(), kMapEntries));

  // Delete the storage key's metadata and map key/value pairs.
  std::vector<DomStorageDatabase::MapLocator> maps_to_delete;
  maps_to_delete.emplace_back(kFirstMapLocator.Clone());

  DbStatus status = database->DeleteStorageKeysFromSession(
      /*session_id*/ std::string(), /*metadata_to_delete=*/{kFirstStorageKey},
      std::move(maps_to_delete));
  EXPECT_TRUE(status.ok()) << status.ToString();

  // Verify the metadata was removed.
  ASSERT_OK_AND_ASSIGN(DomStorageDatabase::Metadata read_metadata,
                       database->ReadAllMetadata());
  EXPECT_EQ(read_metadata.map_metadata.size(), 0u);

  // Verify the map key/value pairs were also removed.
  ASSERT_OK_AND_ASSIGN((std::map<DomStorageDatabase::Key,
                                 DomStorageDatabase::Value> actual_entries),
                       database->ReadMapKeyValues(kFirstMapLocator.Clone()));
  EXPECT_EQ(actual_entries.size(), 0u);
}

// Verifies that `DeleteStorageKeysFromSession()` correctly handles deleting
// multiple storage keys at once, removing their metadata and map key/value
// pairs while leaving unrelated storage keys intact.
TEST_F(LocalStorageSqliteTest,
       DeleteStorageKeysFromSessionWithMultipleStorageKeys) {
  std::unique_ptr<LocalStorageSqlite> database;
  ASSERT_NO_FATAL_FAILURE(OpenInMemory(&database));

  // Write metadata for three storage keys.
  ASSERT_NO_FATAL_FAILURE(InitializeMetadata(
      *database, DomStorageDatabase::Metadata(CloneMapMetadataVector({
                     {
                         .map_locator{kFirstMapLocator.Clone()},
                         .last_accessed{kMapLastAccessed},
                     },
                     {
                         .map_locator{kSecondMapLocator.Clone()},
                         .last_accessed{kMapLastAccessed},
                     },
                     {
                         .map_locator{kThirdMapLocator.Clone()},
                         .last_accessed{kMapLastAccessed},
                     },
                 }))));

  // Add key/value pairs to each map.
  const std::map<DomStorageDatabase::Key, DomStorageDatabase::Value>
      kFirstMapEntries{
          {ToBytes("key_1"), ToBytes("value_1")},
      };
  ASSERT_NO_FATAL_FAILURE(
      InsertMapEntries(*database, kFirstMapLocator.Clone(), kFirstMapEntries));

  const std::map<DomStorageDatabase::Key, DomStorageDatabase::Value>
      kSecondMapEntries{
          {ToBytes("key_2"), ToBytes("value_2")},
      };
  ASSERT_NO_FATAL_FAILURE(InsertMapEntries(*database, kSecondMapLocator.Clone(),
                                           kSecondMapEntries));

  const std::map<DomStorageDatabase::Key, DomStorageDatabase::Value>
      kThirdMapEntries{
          {ToBytes("key_3"), ToBytes("value_3")},
      };
  ASSERT_NO_FATAL_FAILURE(
      InsertMapEntries(*database, kThirdMapLocator.Clone(), kThirdMapEntries));

  // Delete the first and the third storage key's metadata and map.
  std::vector<DomStorageDatabase::MapLocator> maps_to_delete;
  maps_to_delete.emplace_back(kFirstMapLocator.Clone());
  maps_to_delete.emplace_back(kThirdMapLocator.Clone());

  DbStatus status = database->DeleteStorageKeysFromSession(
      /*session_id*/ std::string(),
      /*metadata_to_delete=*/{kFirstStorageKey, kThirdStorageKey},
      std::move(maps_to_delete));
  EXPECT_TRUE(status.ok()) << status.ToString();

  // Verify only the second storage key remains.
  ASSERT_OK_AND_ASSIGN(DomStorageDatabase::Metadata read_metadata,
                       database->ReadAllMetadata());
  ASSERT_EQ(read_metadata.map_metadata.size(), 1u);
  EXPECT_EQ(read_metadata.map_metadata[0].map_locator.storage_key(),
            kSecondStorageKey);

  ASSERT_OK_AND_ASSIGN((std::map<DomStorageDatabase::Key,
                                 DomStorageDatabase::Value> actual_entries),
                       database->ReadMapKeyValues(kFirstMapLocator.Clone()));
  EXPECT_EQ(actual_entries.size(), 0u);

  ASSERT_OK_AND_ASSIGN(actual_entries,
                       database->ReadMapKeyValues(kThirdMapLocator.Clone()));
  EXPECT_EQ(actual_entries.size(), 0u);

  // Verify the second map is unchanged.
  ASSERT_OK_AND_ASSIGN(actual_entries,
                       database->ReadMapKeyValues(kSecondMapLocator.Clone()));
  EXPECT_EQ(actual_entries, kSecondMapEntries);
}

// Verifies that `DeleteStorageKeysFromSession()` succeeds without error when
// attempting to delete a storage key that does not exist in the database. Also
// verifies that existing storage keys and their map key/value pairs remain
// unaffected by the delete operation on the non-existent key.
TEST_F(LocalStorageSqliteTest,
       DeleteStorageKeysFromSessionWithUnknownStorageKey) {
  std::unique_ptr<LocalStorageSqlite> database;
  ASSERT_NO_FATAL_FAILURE(OpenInMemory(&database));

  // Write metadata for a storage key, creating a map row.
  const DomStorageDatabase::MapMetadata kInitialMetadata[] = {
      {
          .map_locator{kFirstMapLocator.Clone()},
          .last_accessed{kMapLastAccessed},
      },
  };
  ASSERT_NO_FATAL_FAILURE(InitializeMetadata(
      *database,
      DomStorageDatabase::Metadata(CloneMapMetadataVector(kInitialMetadata))));

  // Add a key/value to the map.
  const std::map<DomStorageDatabase::Key, DomStorageDatabase::Value>
      kMapEntries{
          {ToBytes("key_1"), ToBytes("value_1")},
      };
  ASSERT_NO_FATAL_FAILURE(
      InsertMapEntries(*database, kFirstMapLocator.Clone(), kMapEntries));

  // Deleting a storage key that does not exist in the database must succeed.
  std::vector<DomStorageDatabase::MapLocator> maps_to_delete;
  maps_to_delete.emplace_back(kSecondMapLocator.Clone());

  DbStatus status = database->DeleteStorageKeysFromSession(
      /*session_id*/ std::string(), /*metadata_to_delete=*/{kSecondStorageKey},
      std::move(maps_to_delete));
  EXPECT_TRUE(status.ok()) << status.ToString();

  // Verify the metadata was removed.
  ASSERT_OK_AND_ASSIGN(DomStorageDatabase::Metadata read_metadata,
                       database->ReadAllMetadata());
  ExpectEqualsMapMetadataSpan(read_metadata.map_metadata, kInitialMetadata);

  // The key/value must remain.
  ASSERT_OK_AND_ASSIGN((std::map<DomStorageDatabase::Key,
                                 DomStorageDatabase::Value> actual_entries),
                       database->ReadMapKeyValues(kFirstMapLocator.Clone()));
  EXPECT_EQ(actual_entries, kMapEntries);
}

// Verifies that `PurgeOrigins()` correctly removes storage data for a matching
// first-party origin while leaving data for non-matching origins intact.
TEST_F(LocalStorageSqliteTest, PurgeOriginsWithMatchingOrigin) {
  std::unique_ptr<LocalStorageSqlite> database;
  ASSERT_NO_FATAL_FAILURE(OpenInMemory(&database));

  // Insert metadata for two storage keys.
  const DomStorageDatabase::MapMetadata kInitialMapMetadata[] = {
      {
          .map_locator{kFirstMapLocator.Clone()},
          .last_accessed{base::Time::Now()},
      },
      {
          .map_locator{kSecondMapLocator.Clone()},
          .last_modified{base::Time::Now()},
          .total_size{base::ByteSize(200)},
      },
  };
  ASSERT_NO_FATAL_FAILURE(InitializeMetadata(
      *database, DomStorageDatabase::Metadata(
                     CloneMapMetadataVector(kInitialMapMetadata))));

  // Insert map key/value pairs for both storage keys.
  const std::map<DomStorageDatabase::Key, DomStorageDatabase::Value>
      kFirstMapEntries{
          {ToBytes("key_1"), ToBytes("value_1")},
      };
  ASSERT_NO_FATAL_FAILURE(
      InsertMapEntries(*database, kFirstMapLocator.Clone(), kFirstMapEntries));

  const std::map<DomStorageDatabase::Key, DomStorageDatabase::Value>
      kSecondMapEntries{
          {ToBytes("key_2"), ToBytes("value_2")},
      };
  ASSERT_NO_FATAL_FAILURE(InsertMapEntries(*database, kSecondMapLocator.Clone(),
                                           kSecondMapEntries));

  // Purge the first storage key's origin.
  std::set<url::Origin> origins_to_purge;
  origins_to_purge.insert(kFirstMapLocator.storage_key().origin());
  DbStatus status = database->PurgeOrigins(std::move(origins_to_purge));
  EXPECT_TRUE(status.ok()) << status.ToString();

  // Verify the first storage key's data was removed.
  ASSERT_OK_AND_ASSIGN(DomStorageDatabase::Metadata read_metadata,
                       database->ReadAllMetadata());
  ExpectEqualsMapMetadataSpan(read_metadata.map_metadata,
                              base::span_from_ref(kInitialMapMetadata[1]));

  // Verify the first map's key/value pairs were removed.
  ASSERT_OK_AND_ASSIGN((std::map<DomStorageDatabase::Key,
                                 DomStorageDatabase::Value> actual_entries),
                       database->ReadMapKeyValues(kFirstMapLocator.Clone()));
  EXPECT_EQ(actual_entries.size(), 0u);

  // Verify the second storage key's data remains.
  ASSERT_OK_AND_ASSIGN(actual_entries,
                       database->ReadMapKeyValues(kSecondMapLocator.Clone()));
  EXPECT_EQ(actual_entries, kSecondMapEntries);
}

// Verifies that `PurgeOrigins()` correctly removes storage data for third-party
// contexts where the top-level site matches the purged origin.
TEST_F(LocalStorageSqliteTest, PurgeOriginsWithMatchingThirdPartyContext) {
  std::unique_ptr<LocalStorageSqlite> database;
  ASSERT_NO_FATAL_FAILURE(OpenInMemory(&database));

  // Create a first-party storage key.
  const blink::StorageKey kFirstPartyStorageKey =
      blink::StorageKey::CreateFromStringForTesting("https://embedded.test");

  const DomStorageDatabase::MapLocator kFirstPartyMapLocator{
      kFirstPartyStorageKey,
      /*map_id=*/1};

  // Create a third-party storage key where the top-level site is different
  // from the origin.
  const url::Origin kEmbeddedOrigin =
      url::Origin::Create(GURL("https://embedded.test"));

  const net::SchemefulSite kTopLevelSite(GURL("https://toplevel.test"));

  const blink::StorageKey kThirdPartyStorageKey =
      blink::StorageKey::Create(kEmbeddedOrigin, kTopLevelSite,
                                blink::mojom::AncestorChainBit::kCrossSite);

  const DomStorageDatabase::MapLocator kThirdPartyMapLocator{
      kThirdPartyStorageKey,
      /*map_id=*/2};

  // Insert metadata for two storage keys.
  const DomStorageDatabase::MapMetadata kInitialMapMetadata[] = {
      {
          .map_locator{kFirstPartyMapLocator.Clone()},
          .last_accessed{base::Time::Now()},
      },
      {
          .map_locator{kThirdPartyMapLocator.Clone()},
          .last_modified{base::Time::Now()},
          .total_size{base::ByteSize(200)},
      },
  };
  ASSERT_NO_FATAL_FAILURE(InitializeMetadata(
      *database, DomStorageDatabase::Metadata(
                     CloneMapMetadataVector(kInitialMapMetadata))));

  // Insert map key/value pairs for all storage keys.
  const std::map<DomStorageDatabase::Key, DomStorageDatabase::Value>
      kFirstPartyMapEntries{
          {ToBytes("key_1"), ToBytes("value_1")},
      };
  ASSERT_NO_FATAL_FAILURE(InsertMapEntries(
      *database, kFirstPartyMapLocator.Clone(), kFirstPartyMapEntries));

  const std::map<DomStorageDatabase::Key, DomStorageDatabase::Value>
      kThirdPartyMapEntries{
          {ToBytes("key_2"), ToBytes("value_2")},
      };
  ASSERT_NO_FATAL_FAILURE(InsertMapEntries(
      *database, kThirdPartyMapLocator.Clone(), kThirdPartyMapEntries));

  // Purge the top-level site's origin. This should remove the third-party
  // storage key because its top-level site matches.
  std::set<url::Origin> origins_to_purge;
  origins_to_purge.insert(url::Origin::Create(kTopLevelSite.GetURL()));
  DbStatus status = database->PurgeOrigins(std::move(origins_to_purge));
  EXPECT_TRUE(status.ok()) << status.ToString();

  // Verify the third-party's metadata was removed.
  ASSERT_OK_AND_ASSIGN(DomStorageDatabase::Metadata read_metadata,
                       database->ReadAllMetadata());
  ExpectEqualsMapMetadataSpan(read_metadata.map_metadata,
                              base::span_from_ref(kInitialMapMetadata[0]));

  // Verify the third-party map's key/value pairs were removed.
  ASSERT_OK_AND_ASSIGN(
      (std::map<DomStorageDatabase::Key, DomStorageDatabase::Value>
           actual_entries),
      database->ReadMapKeyValues(
          DomStorageDatabase::MapLocator(kThirdPartyStorageKey)));
  EXPECT_EQ(actual_entries.size(), 0u);

  // Verify the first-party map's key/value pairs remain.
  ASSERT_OK_AND_ASSIGN(
      actual_entries, database->ReadMapKeyValues(DomStorageDatabase::MapLocator(
                          kFirstPartyStorageKey)));
  EXPECT_EQ(actual_entries, kFirstPartyMapEntries);
}

}  // namespace storage
