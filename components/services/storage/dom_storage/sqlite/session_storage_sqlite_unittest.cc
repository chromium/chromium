// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/storage/dom_storage/sqlite/session_storage_sqlite.h"

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
constexpr const char kFirstSessionId[] = "ce8c7dc5_73b4_4320_a506_ce1f4fd3356f";
constexpr const char kFirstUrlString[] = "https://a-fake.test/";
constexpr int64_t kFirstMapId = 1565;

constexpr const char kSecondSessionId[] =
    "36356e0b_1627_4492_a474_db76a8996bed";
constexpr const char kSecondUrlString[] = "https://b-fake.test/";
constexpr int64_t kSecondMapId = 1566;

constexpr const char kThirdSessionId[] = "5fe0e896_c6d8_4d2b_8b3c_d26f47832125";
constexpr const char kThirdUrlString[] = "https://c-fake.test/";
constexpr int64_t kThirdMapId = 1567;

std::vector<uint8_t> ToBytes(std::string_view str) {
  return std::vector<uint8_t>(str.begin(), str.end());
}

}  // namespace

class SessionStorageSqliteTest : public testing::Test {
 protected:
  SessionStorageSqliteTest();
  ~SessionStorageSqliteTest() override;

  // Creates a path to the SQLite database file under `temp_dir_`.  Creates
  // `temp_dir_` when necessary.
  void GetDatabasePath(base::FilePath* result);

  // Returns the `PassKey` required to create and open `SessionStorageSqlite`.
  base::PassKey<DomStorageDatabaseFactory> GetPassKey();

  void OpenOnDisk(std::unique_ptr<SessionStorageSqlite>* result);

  void OpenInMemory(std::unique_ptr<SessionStorageSqlite>* result);

  // Writes `metadata` to the database and verifies it was persisted correctly.
  void InitializeMetadata(DomStorageDatabase& database,
                          const DomStorageDatabase::Metadata& metadata);

  const blink::StorageKey kFirstStorageKey =
      blink::StorageKey::CreateFromStringForTesting(kFirstUrlString);

  const blink::StorageKey kSecondStorageKey =
      blink::StorageKey::CreateFromStringForTesting(kSecondUrlString);

  const blink::StorageKey kThirdStorageKey =
      blink::StorageKey::CreateFromStringForTesting(kThirdUrlString);

  const DomStorageDatabase::MapLocator kFirstMapLocator{
      kFirstSessionId, kFirstStorageKey, kFirstMapId};

  const DomStorageDatabase::MapLocator kSecondMapLocator{
      kSecondSessionId, kSecondStorageKey, kSecondMapId};

  base::test::ScopedFeatureList scoped_feature_list_;
  base::ScopedTempDir temp_dir_;
  base::test::TaskEnvironment task_environment_;
};

SessionStorageSqliteTest::SessionStorageSqliteTest() {
  scoped_feature_list_.InitAndEnableFeature(kDomStorageSqlite);
}

SessionStorageSqliteTest::~SessionStorageSqliteTest() = default;

void SessionStorageSqliteTest::GetDatabasePath(base::FilePath* result) {
  if (!temp_dir_.IsValid()) {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
  }
  *result = DomStorageDatabase::GetPath(StorageType::kSessionStorage,
                                        temp_dir_.GetPath());
}

base::PassKey<DomStorageDatabaseFactory>
SessionStorageSqliteTest::GetPassKey() {
  return DomStorageDatabaseFactory::CreatePassKeyForTesting();
}

void SessionStorageSqliteTest::OpenOnDisk(
    std::unique_ptr<SessionStorageSqlite>* result) {
  base::FilePath database_path;
  ASSERT_NO_FATAL_FAILURE(GetDatabasePath(&database_path));

  std::unique_ptr<SessionStorageSqlite> instance =
      std::make_unique<SessionStorageSqlite>(GetPassKey());

  DbStatus status = instance->Open(GetPassKey(),
                                   /*database_path=*/database_path,
                                   /*memory_dump_id=*/std::nullopt);

  ASSERT_TRUE(status.ok()) << status.ToString();
  *result = std::move(instance);
}

void SessionStorageSqliteTest::OpenInMemory(
    std::unique_ptr<SessionStorageSqlite>* result) {
  std::unique_ptr<SessionStorageSqlite> instance =
      std::make_unique<SessionStorageSqlite>(GetPassKey());

  DbStatus status = instance->Open(GetPassKey(),
                                   /*database_path=*/base::FilePath(),
                                   /*memory_dump_id=*/std::nullopt);

  ASSERT_TRUE(status.ok()) << status.ToString();
  *result = std::move(instance);
}

void SessionStorageSqliteTest::InitializeMetadata(
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

  // Verify `next_map_id`. `SessionStorageSqlite` uses 0 as the default value
  // when `next_map_id` does not exist in the database.
  int64_t expected_next_map_id =
      metadata.next_map_id ? *metadata.next_map_id : 0;
  EXPECT_EQ(actual_metadata.next_map_id, expected_next_map_id);

  ExpectEqualsMapMetadataSpan(actual_metadata.map_metadata,
                              metadata.map_metadata);
}

TEST_F(SessionStorageSqliteTest, OpenInMemory) {
  std::unique_ptr<SessionStorageSqlite> database;
  ASSERT_NO_FATAL_FAILURE(OpenInMemory(&database));
}

TEST_F(SessionStorageSqliteTest, OpenThenDestroyOnDisk) {
  std::unique_ptr<SessionStorageSqlite> database;
  ASSERT_NO_FATAL_FAILURE(OpenOnDisk(&database));
  database.reset();

  base::FilePath database_path;
  ASSERT_NO_FATAL_FAILURE(GetDatabasePath(&database_path));
  EXPECT_TRUE(base::PathExists(database_path));

  DbStatus status = sqlite::DestroyDatabase(database_path);
  EXPECT_TRUE(status.ok()) << status.ToString();
  EXPECT_FALSE(base::PathExists(database_path));
}

TEST_F(SessionStorageSqliteTest, VersionTooNew) {
  base::FilePath database_path;
  ASSERT_NO_FATAL_FAILURE(GetDatabasePath(&database_path));

  // Write the wrong version to the database
  std::unique_ptr<SessionStorageSqlite> database;
  ASSERT_NO_FATAL_FAILURE(OpenOnDisk(&database));
  database->PutVersionForTesting(9999);
  database.reset();

  // Opening the database with the wrong version must fail.
  database = std::make_unique<SessionStorageSqlite>(GetPassKey());
  DbStatus status = database->Open(GetPassKey(),
                                   /*database_path=*/database_path,
                                   /*memory_dump_id=*/std::nullopt);
  EXPECT_TRUE(status.IsNotFound());
}

// Verifies that reading metadata from an empty database returns default values:
// `next_map_id` should be 0 and `map_metadata` should be empty.
TEST_F(SessionStorageSqliteTest, ReadAllMetadataWithEmpty) {
  std::unique_ptr<SessionStorageSqlite> database;
  ASSERT_NO_FATAL_FAILURE(OpenInMemory(&database));

  ASSERT_OK_AND_ASSIGN(DomStorageDatabase::Metadata metadata,
                       database->ReadAllMetadata());

  ASSERT_TRUE(metadata.next_map_id.has_value());
  EXPECT_EQ(*metadata.next_map_id, 0);
  EXPECT_EQ(metadata.map_metadata.size(), 0u);
}

// Verifies that `PutMetadata()` correctly persists `next_map_id` and that
// subsequent calls overwrite the previous value.
TEST_F(SessionStorageSqliteTest, PutNextMapId) {
  std::unique_ptr<SessionStorageSqlite> database;
  ASSERT_NO_FATAL_FAILURE(OpenInMemory(&database));

  // Write the first `next_map_id` value.
  DomStorageDatabase::Metadata metadata;
  metadata.next_map_id = 56;
  ASSERT_NO_FATAL_FAILURE(InitializeMetadata(*database, metadata));

  // Write a second `next_map_id` value to overwrite the first.
  constexpr int64_t kSecondNextMapId = 57;
  DomStorageDatabase::Metadata second_metadata_to_write;
  second_metadata_to_write.next_map_id = kSecondNextMapId;

  DbStatus status = database->PutMetadata(std::move(second_metadata_to_write));
  EXPECT_TRUE(status.ok()) << status.ToString();

  // Verify the second `next_map_id` replaced the first.
  ASSERT_OK_AND_ASSIGN(DomStorageDatabase::Metadata read_metadata,
                       database->ReadAllMetadata());
  ASSERT_TRUE(read_metadata.next_map_id.has_value());
  EXPECT_EQ(*read_metadata.next_map_id, kSecondNextMapId);
  EXPECT_EQ(read_metadata.map_metadata.size(), 0u);
}

// Verifies that `PutMetadata()` correctly persists map metadata and that
// subsequent calls with the same session/storage_key overwrite previous values.
TEST_F(SessionStorageSqliteTest, PutMapMetadata) {
  std::unique_ptr<SessionStorageSqlite> database;
  ASSERT_NO_FATAL_FAILURE(OpenInMemory(&database));

  // Insert map metadata for a single session and storage key.
  const DomStorageDatabase::MapMetadata kMapMetadataToInsert[] = {
      {
          .map_locator{kFirstSessionId, kFirstStorageKey, kFirstMapId},
      },
  };
  ASSERT_NO_FATAL_FAILURE(InitializeMetadata(
      *database, DomStorageDatabase::Metadata(
                     CloneMapMetadataVector(kMapMetadataToInsert))));

  // Replace the map metadata with a new `map_id` for the same session and
  // storage key.
  const DomStorageDatabase::MapMetadata kReplacementMapMetadata[] = {
      {
          .map_locator{kFirstSessionId, kFirstStorageKey, kSecondMapId},
      },
  };
  ASSERT_NO_FATAL_FAILURE(InitializeMetadata(
      *database, DomStorageDatabase::Metadata(
                     CloneMapMetadataVector(kReplacementMapMetadata))));
}

// Verifies that `PutMetadata()` correctly handles multiple sessions and storage
// keys, including cloned maps that share the same `map_id` across sessions.
TEST_F(SessionStorageSqliteTest, PutMapMetadataWithMultipleSessions) {
  constexpr int64_t kNextMapId = kSecondMapId + 2;

  std::unique_ptr<SessionStorageSqlite> database;
  ASSERT_NO_FATAL_FAILURE(OpenInMemory(&database));

  // Create metadata for 3 different maps:
  // - Map 1: `kFirstStorageKey` in `kFirstSessionId`, cloned to
  //          `kThirdSessionId`.
  // - Map 2: `kSecondStorageKey` in `kFirstSessionId`.
  // - Map 3: `kSecondStorageKey` in `kSecondSessionId`.
  std::vector<DomStorageDatabase::MapMetadata> expected_map_metadata;
  expected_map_metadata.push_back(
      {.map_locator{kFirstSessionId, kFirstStorageKey, kFirstMapId}});
  expected_map_metadata.push_back(
      {.map_locator{kFirstSessionId, kSecondStorageKey, kSecondMapId}});
  expected_map_metadata.push_back(
      {.map_locator{kSecondSessionId, kSecondStorageKey, kThirdMapId}});

  // Clone the first map to a third session (both sessions share the same
  // `map_id`).
  expected_map_metadata[0].map_locator.AddSession(kThirdSessionId);

  // Write metadata for all 3 maps along with the `next_map_id`.
  DomStorageDatabase::Metadata metadata;
  metadata.next_map_id = kNextMapId;
  metadata.map_metadata = CloneMapMetadataVector(expected_map_metadata);

  ASSERT_NO_FATAL_FAILURE(InitializeMetadata(*database, metadata));
}

// Verifies that metadata written to the database is persisted across opens and
// that the in-memory representation is in sync with the database.
TEST_F(SessionStorageSqliteTest, MetadataPersistence) {
  constexpr int64_t kNextMapId = 100;

  std::vector<DomStorageDatabase::MapMetadata> expected_map_metadata;
  expected_map_metadata.push_back(
      {.map_locator{kFirstSessionId, kFirstStorageKey, kFirstMapId}});
  expected_map_metadata.push_back(
      {.map_locator{kSecondSessionId, kSecondStorageKey, kSecondMapId}});

  // Open database and write metadata.
  {
    std::unique_ptr<SessionStorageSqlite> database;
    ASSERT_NO_FATAL_FAILURE(OpenOnDisk(&database));

    DomStorageDatabase::Metadata metadata;
    metadata.next_map_id = kNextMapId;
    metadata.map_metadata = CloneMapMetadataVector(expected_map_metadata);
    ASSERT_NO_FATAL_FAILURE(InitializeMetadata(*database, metadata));
  }

  // Re-open the database and verify metadata persisted.
  {
    std::unique_ptr<SessionStorageSqlite> database;
    ASSERT_NO_FATAL_FAILURE(OpenOnDisk(&database));

    ASSERT_OK_AND_ASSIGN(DomStorageDatabase::Metadata read_metadata,
                         database->ReadAllMetadata());

    ASSERT_TRUE(read_metadata.next_map_id.has_value());
    EXPECT_EQ(*read_metadata.next_map_id, kNextMapId);
    ExpectEqualsMapMetadataSpan(read_metadata.map_metadata,
                                expected_map_metadata);
  }
}

// Verifies that `ReadAllMetadata()` returns a corruption error when the
// database contains a storage key that cannot be deserialized.
TEST_F(SessionStorageSqliteTest, ReadAllMetadataWithInvalidStorageKey) {
  // Write valid metadata to ensure the table exists and works.
  {
    std::unique_ptr<SessionStorageSqlite> database;
    ASSERT_NO_FATAL_FAILURE(OpenOnDisk(&database));

    DomStorageDatabase::Metadata valid_metadata;
    valid_metadata.map_metadata.push_back({
        .map_locator{kFirstSessionId, kFirstStorageKey, kFirstMapId},
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
        "INSERT INTO session_metadata (session_id, storage_key, map_id) "
        "VALUES (?, ?, ?)";

    sql::Statement statement(
        database.GetUniqueStatement(kInsertInvalidStorageKey));
    statement.BindString(0, kSecondSessionId);
    statement.BindBlob(
        1, /*storage_key=*/std::vector<uint8_t>{0xFF, 0xFE, 0x00, 0x01});
    statement.BindInt64(2, kSecondMapId);

    EXPECT_TRUE(statement.Run());
  }

  // Re-open the database and verify that `ReadAllMetadata()` returns a
  // corruption error due to the invalid storage key.
  {
    std::unique_ptr<SessionStorageSqlite> database;
    ASSERT_NO_FATAL_FAILURE(OpenOnDisk(&database));

    StatusOr<DomStorageDatabase::Metadata> result = database->ReadAllMetadata();
    ASSERT_FALSE(result.has_value());
    EXPECT_TRUE(result.error().IsCorruption());
  }
}

// Verifies that `UpdateMaps()` correctly adds, modifies, and deletes key/value
// pairs across multiple maps.
TEST_F(SessionStorageSqliteTest, UpdateMaps) {
  std::unique_ptr<SessionStorageSqlite> database;
  ASSERT_NO_FATAL_FAILURE(OpenInMemory(&database));

  DomStorageDatabase::MapLocator map1_locator{kFirstSessionId, kFirstStorageKey,
                                              kFirstMapId};

  DomStorageDatabase::MapLocator map2_locator{kFirstSessionId,
                                              kSecondStorageKey, kSecondMapId};
  ASSERT_NO_FATAL_FAILURE(
      TestUpdateMaps(*database, map1_locator, map2_locator));
}

// Verifies that `CloneMap()` correctly copies all key/value pairs from the
// source map to the target map while leaving the source map unchanged.
TEST_F(SessionStorageSqliteTest, CloneMap) {
  std::unique_ptr<SessionStorageSqlite> database;
  ASSERT_NO_FATAL_FAILURE(OpenInMemory(&database));

  const DomStorageDatabase::MapLocator kSourceMapLocator{
      kFirstSessionId, kFirstStorageKey, kFirstMapId};
  const DomStorageDatabase::MapLocator kTargetMapLocator{
      kSecondSessionId, kSecondStorageKey, kSecondMapId};

  // Insert key/value pairs into the source map.
  const std::map<DomStorageDatabase::Key, DomStorageDatabase::Value>
      kSourceMapEntries{
          {ToBytes("key_1"), ToBytes("value_1")},
          {ToBytes("key_2"), ToBytes("value_2")},
          {ToBytes("key_3"), ToBytes("value_3")},
      };
  ASSERT_NO_FATAL_FAILURE(InsertMapEntries(*database, kSourceMapLocator.Clone(),
                                           kSourceMapEntries));

  // Clone the source map to the target map.
  DbStatus status =
      database->CloneMap(kSourceMapLocator.Clone(), kTargetMapLocator.Clone());
  EXPECT_TRUE(status.ok()) << status.ToString();

  // Verify the target map now contains the same entries as the source map.
  ASSERT_OK_AND_ASSIGN((std::map<DomStorageDatabase::Key,
                                 DomStorageDatabase::Value> target_entries),
                       database->ReadMapKeyValues(kTargetMapLocator.Clone()));
  EXPECT_EQ(target_entries, kSourceMapEntries);

  // Verify the source map is unchanged.
  ASSERT_OK_AND_ASSIGN((std::map<DomStorageDatabase::Key,
                                 DomStorageDatabase::Value> source_entries),
                       database->ReadMapKeyValues(kSourceMapLocator.Clone()));
  EXPECT_EQ(source_entries, kSourceMapEntries);
}

// Verifies deleting a session removes its metadata from the database.
TEST_F(SessionStorageSqliteTest, DeleteSessionsWithMetadata) {
  std::unique_ptr<SessionStorageSqlite> database;
  ASSERT_NO_FATAL_FAILURE(OpenInMemory(&database));

  // Add one metadata row to the database.
  ASSERT_NO_FATAL_FAILURE(InitializeMetadata(
      *database, DomStorageDatabase::Metadata(CloneMapMetadataVector(
                     {{.map_locator{kFirstMapLocator.Clone()}}}))));

  // Delete the session, which should remove its metadata.
  DbStatus status = database->DeleteSessions({kFirstSessionId},
                                             /*maps_to_delete=*/{});
  EXPECT_TRUE(status.ok()) << status.ToString();

  // Verify the metadata was removed.
  ASSERT_OK_AND_ASSIGN(DomStorageDatabase::Metadata read_metadata,
                       database->ReadAllMetadata());
  EXPECT_EQ(read_metadata.map_metadata.size(), 0u);
}

// Verifies deleting multiple sessions removes all their metadata,
// including when a session has multiple storage keys (multiple maps).
TEST_F(SessionStorageSqliteTest, DeleteMultipleSessionsWithMultipleMetadata) {
  std::unique_ptr<SessionStorageSqlite> database;
  ASSERT_NO_FATAL_FAILURE(OpenInMemory(&database));

  // Create additional maps for this test. The first session has two maps (one
  // for each storage key).
  const DomStorageDatabase::MapLocator kFirstSessionSecondMapLocator{
      kFirstSessionId, kSecondStorageKey, kThirdMapId};
  const DomStorageDatabase::MapLocator kThirdMapLocator{
      kThirdSessionId, kFirstStorageKey, /*map_id=*/4};

  // Add metadata for multiple sessions to the database. The first session has
  // two maps. The second and third sessions have one each.
  ASSERT_NO_FATAL_FAILURE(InitializeMetadata(
      *database, DomStorageDatabase::Metadata(CloneMapMetadataVector({
                     {.map_locator{kFirstMapLocator.Clone()}},
                     {.map_locator{kFirstSessionSecondMapLocator.Clone()}},
                     {.map_locator{kSecondMapLocator.Clone()}},
                     {.map_locator{kThirdMapLocator.Clone()}},
                 }))));

  // Delete the first two sessions, leaving the third session intact.
  DbStatus status =
      database->DeleteSessions({kFirstSessionId, kSecondSessionId},
                               /*maps_to_delete=*/{});
  EXPECT_TRUE(status.ok()) << status.ToString();

  // Verify only the third session's metadata remains.
  ASSERT_OK_AND_ASSIGN(DomStorageDatabase::Metadata read_metadata,
                       database->ReadAllMetadata());
  ExpectEqualsMapMetadataSpan(read_metadata.map_metadata,
                              {{.map_locator{kThirdMapLocator.Clone()}}});
}

// Verifies deleting a session removes its map key/value pairs when the map is
// no longer referenced by other sessions.
TEST_F(SessionStorageSqliteTest, DeleteSessionsWithMapKeyValues) {
  std::unique_ptr<SessionStorageSqlite> database;
  ASSERT_NO_FATAL_FAILURE(OpenInMemory(&database));

  // Add map key/value pairs to the database.
  const std::map<DomStorageDatabase::Key, DomStorageDatabase::Value>
      kMapEntries{
          {ToBytes("key_1"), ToBytes("value_1")},
          {ToBytes("key_2"), ToBytes("value_2")},
      };
  ASSERT_NO_FATAL_FAILURE(
      InsertMapEntries(*database, kFirstMapLocator.Clone(), kMapEntries));

  // Delete the session and its map key/value pairs.
  std::vector<DomStorageDatabase::MapLocator> maps_to_delete;
  maps_to_delete.push_back(kFirstMapLocator.Clone());
  maps_to_delete.back().RemoveSession(kFirstSessionId);

  DbStatus status =
      database->DeleteSessions({kFirstSessionId}, std::move(maps_to_delete));
  EXPECT_TRUE(status.ok()) << status.ToString();

  // Verify the map is now empty.
  ASSERT_OK_AND_ASSIGN((std::map<DomStorageDatabase::Key,
                                 DomStorageDatabase::Value> actual_entries),
                       database->ReadMapKeyValues(kFirstMapLocator.Clone()));
  EXPECT_EQ(actual_entries.size(), 0u);
}

// Verifies deleting a session removes key/value pairs from multiple maps
// when all maps are no longer referenced by other sessions.
TEST_F(SessionStorageSqliteTest, DeleteSessionsWithMultipleMaps) {
  std::unique_ptr<SessionStorageSqlite> database;
  ASSERT_NO_FATAL_FAILURE(OpenInMemory(&database));

  // Create a second map for the first session with a different storage key.
  const DomStorageDatabase::MapLocator kFirstSessionSecondMapLocator{
      kFirstSessionId, kSecondStorageKey, kSecondMapId};

  // Add key/value pairs to the first map.
  const std::map<DomStorageDatabase::Key, DomStorageDatabase::Value>
      kFirstMapEntries{
          {ToBytes("key_1"), ToBytes("value_1")},
          {ToBytes("key_2"), ToBytes("value_2")},
      };
  ASSERT_NO_FATAL_FAILURE(
      InsertMapEntries(*database, kFirstMapLocator.Clone(), kFirstMapEntries));

  // Add key/value pairs to the second map.
  const std::map<DomStorageDatabase::Key, DomStorageDatabase::Value>
      kSecondMapEntries{
          {ToBytes("key_3"), ToBytes("value_3")},
      };
  ASSERT_NO_FATAL_FAILURE(InsertMapEntries(
      *database, kFirstSessionSecondMapLocator.Clone(), kSecondMapEntries));

  // Delete the session and both of its maps.
  std::vector<DomStorageDatabase::MapLocator> maps_to_delete;
  maps_to_delete.push_back(kFirstMapLocator.Clone());
  maps_to_delete.back().RemoveSession(kFirstSessionId);
  maps_to_delete.push_back(kFirstSessionSecondMapLocator.Clone());
  maps_to_delete.back().RemoveSession(kFirstSessionId);

  DbStatus status =
      database->DeleteSessions({kFirstSessionId}, std::move(maps_to_delete));
  EXPECT_TRUE(status.ok()) << status.ToString();

  // Verify the metadata was removed.
  ASSERT_OK_AND_ASSIGN(DomStorageDatabase::Metadata read_metadata,
                       database->ReadAllMetadata());
  EXPECT_EQ(read_metadata.map_metadata.size(), 0u);

  // Verify both maps are now empty.
  ASSERT_OK_AND_ASSIGN((std::map<DomStorageDatabase::Key,
                                 DomStorageDatabase::Value> actual_map_entries),
                       database->ReadMapKeyValues(kFirstMapLocator.Clone()));
  EXPECT_EQ(actual_map_entries.size(), 0u);

  ASSERT_OK_AND_ASSIGN(
      actual_map_entries,
      database->ReadMapKeyValues(kFirstSessionSecondMapLocator.Clone()));
  EXPECT_EQ(actual_map_entries.size(), 0u);
}

// Verifies deleting a session removes its metadata but leaves the map key/value
// pairs intact when the map is not included in `maps_to_delete`.
TEST_F(SessionStorageSqliteTest, DeleteSessionsWithMapExcluded) {
  std::unique_ptr<SessionStorageSqlite> database;
  ASSERT_NO_FATAL_FAILURE(OpenInMemory(&database));

  // Add one metadata row to the database.
  ASSERT_NO_FATAL_FAILURE(InitializeMetadata(
      *database, DomStorageDatabase::Metadata(CloneMapMetadataVector(
                     {{.map_locator{kFirstMapLocator.Clone()}}}))));

  // Add some key/values to the database.
  const std::map<DomStorageDatabase::Key, DomStorageDatabase::Value>
      kMapEntries{
          {ToBytes("key_1"), ToBytes("value_1")},
          {ToBytes("key_2"), ToBytes("value_2")},
          {ToBytes("key_3"), ToBytes("value_3")},
      };
  ASSERT_NO_FATAL_FAILURE(
      InsertMapEntries(*database, kFirstMapLocator.Clone(), kMapEntries));

  // Delete the session metadata but not the map key/value entries.
  DbStatus status = database->DeleteSessions({kFirstSessionId},
                                             /*maps_to_delete=*/{});
  EXPECT_TRUE(status.ok()) << status.ToString();

  // Verify the metadata was removed but the map key/values remain.
  ASSERT_OK_AND_ASSIGN(DomStorageDatabase::Metadata read_metadata,
                       database->ReadAllMetadata());
  EXPECT_EQ(read_metadata.map_metadata.size(), 0u);

  ASSERT_OK_AND_ASSIGN((std::map<DomStorageDatabase::Key,
                                 DomStorageDatabase::Value> actual_entries),
                       database->ReadMapKeyValues(kFirstMapLocator.Clone()));
  EXPECT_EQ(actual_entries, kMapEntries);
}

// Verifies deleting multiple sessions removes their metadata and selectively
// deletes map key/value pairs based on whether they're still referenced by
// other sessions (cloned maps).
TEST_F(SessionStorageSqliteTest, DeleteSessionsWithMultipleStorageKeys) {
  std::unique_ptr<SessionStorageSqlite> database;
  ASSERT_NO_FATAL_FAILURE(OpenInMemory(&database));

  // Create two maps and two cloned maps used by two storage keys across three
  // sessions. This adds four metadata rows with one row for each map usage.
  std::vector<DomStorageDatabase::MapMetadata> expected_map_metadata;
  expected_map_metadata.push_back({.map_locator{kFirstMapLocator.Clone()}});
  expected_map_metadata.push_back({.map_locator{kSecondMapLocator.Clone()}});

  // Clone `kFirstMapLocator` for `kSecondSessionId`.
  expected_map_metadata[0].map_locator.AddSession(kSecondSessionId);

  // Clone `kSecondMapLocator` for `kThirdSessionId`.
  expected_map_metadata[1].map_locator.AddSession(kThirdSessionId);

  ASSERT_NO_FATAL_FAILURE(InitializeMetadata(
      *database, DomStorageDatabase::Metadata(
                     CloneMapMetadataVector(expected_map_metadata))));

  // Write the first map's key/value pairs.
  const std::map<DomStorageDatabase::Key, DomStorageDatabase::Value>
      kFirstMapEntries{
          {ToBytes("key_1"), ToBytes("value_1")},
          {ToBytes("key_2"), ToBytes("value_2")},
      };
  ASSERT_NO_FATAL_FAILURE(
      InsertMapEntries(*database, kFirstMapLocator.Clone(), kFirstMapEntries));

  // Write the second map's key/value pairs.
  const std::map<DomStorageDatabase::Key, DomStorageDatabase::Value>
      kSecondMapEntries{
          {ToBytes("key_3"), ToBytes("value_4")},
      };
  ASSERT_NO_FATAL_FAILURE(InsertMapEntries(*database, kSecondMapLocator.Clone(),
                                           kSecondMapEntries));

  // Delete `kFirstSessionId`, which must remove one metadata row.
  DbStatus status = database->DeleteSessions({kFirstSessionId},
                                             /*maps_to_delete=*/{});
  EXPECT_TRUE(status.ok()) << status.ToString();

  // Verify what remains:
  //  - Three metadata rows: Two for `kSecondSessionId` and one for
  //    `kThirdSessionId`.
  //  - Three map key/value entries: Two for `kFirstMapId` and one for
  //    `kSecondMapId`.
  ASSERT_OK_AND_ASSIGN(DomStorageDatabase::Metadata read_metadata,
                       database->ReadAllMetadata());

  // Remove `kFirstSessionId` from the front of `expected_map_metadata`.
  expected_map_metadata[0].map_locator.RemoveSession(kFirstSessionId);
  ExpectEqualsMapMetadataSpan(read_metadata.map_metadata,
                              expected_map_metadata);

  ASSERT_OK_AND_ASSIGN((std::map<DomStorageDatabase::Key,
                                 DomStorageDatabase::Value> actual_entries),
                       database->ReadMapKeyValues(kFirstMapLocator.Clone()));
  EXPECT_EQ(actual_entries, kFirstMapEntries);

  ASSERT_OK_AND_ASSIGN(actual_entries,
                       database->ReadMapKeyValues(kSecondMapLocator.Clone()));
  EXPECT_EQ(actual_entries, kSecondMapEntries);

  // Delete `kSecondSessionId` along with its no longer referenced map:
  // `kFirstMapId`. This must remove one metadata row and two map key/value
  // entries.
  std::vector<DomStorageDatabase::MapLocator> maps_to_delete;
  maps_to_delete.emplace_back(kSecondSessionId, kFirstStorageKey, kFirstMapId);
  maps_to_delete.back().RemoveSession(kSecondSessionId);

  status =
      database->DeleteSessions({kSecondSessionId}, std::move(maps_to_delete));
  EXPECT_TRUE(status.ok()) << status.ToString();

  // Verify one metadata row for `kThirdSessionId` and one map's key/values
  // remain.
  ASSERT_OK_AND_ASSIGN(read_metadata, database->ReadAllMetadata());

  // Pop `kSecondSessionId` from the front of `expected_map_metadata`.
  expected_map_metadata.erase(expected_map_metadata.begin());
  expected_map_metadata[0].map_locator.RemoveSession(kSecondSessionId);
  ExpectEqualsMapMetadataSpan(read_metadata.map_metadata,
                              expected_map_metadata);

  // Verify only the second map's key/value entries remains.
  ASSERT_OK_AND_ASSIGN(actual_entries,
                       database->ReadMapKeyValues(kFirstMapLocator.Clone()));
  EXPECT_EQ(actual_entries.size(), 0u);

  ASSERT_OK_AND_ASSIGN(actual_entries,
                       database->ReadMapKeyValues(kSecondMapLocator.Clone()));
  EXPECT_EQ(actual_entries, kSecondMapEntries);
}

// Verifies deleting storage keys from a session removes its metadata from the
// database.
TEST_F(SessionStorageSqliteTest, DeleteStorageKeysFromSessionWithMetadata) {
  std::unique_ptr<SessionStorageSqlite> database;
  ASSERT_NO_FATAL_FAILURE(OpenInMemory(&database));

  // Add one metadata row to the database.
  ASSERT_NO_FATAL_FAILURE(InitializeMetadata(
      *database, DomStorageDatabase::Metadata(CloneMapMetadataVector(
                     {{.map_locator{kFirstMapLocator.Clone()}}}))));

  // Delete the storage key's metadata from the session.
  DbStatus status = database->DeleteStorageKeysFromSession(
      kFirstSessionId, /*metadata_to_delete=*/{kFirstStorageKey},
      /*maps_to_delete=*/{});
  EXPECT_TRUE(status.ok()) << status.ToString();

  // Verify the metadata was removed.
  ASSERT_OK_AND_ASSIGN(DomStorageDatabase::Metadata read_metadata,
                       database->ReadAllMetadata());
  EXPECT_EQ(read_metadata.map_metadata.size(), 0u);
}

// Verifies deleting storage keys from a session removes map key/value pairs
// when the map is no longer referenced by other sessions.
TEST_F(SessionStorageSqliteTest, DeleteStorageKeysFromSessionWithMapKeyValues) {
  std::unique_ptr<SessionStorageSqlite> database;
  ASSERT_NO_FATAL_FAILURE(OpenInMemory(&database));

  // Add map key/value pairs to the database.
  const std::map<DomStorageDatabase::Key, DomStorageDatabase::Value>
      kMapEntries{
          {ToBytes("key_1"), ToBytes("value_1")},
          {ToBytes("key_2"), ToBytes("value_2")},
      };
  ASSERT_NO_FATAL_FAILURE(
      InsertMapEntries(*database, kFirstMapLocator.Clone(), kMapEntries));

  // Delete the storage key's metadata and map key/value pairs.
  std::vector<DomStorageDatabase::MapLocator> maps_to_delete;
  maps_to_delete.push_back(kFirstMapLocator.Clone());
  maps_to_delete.back().RemoveSession(kFirstSessionId);

  DbStatus status = database->DeleteStorageKeysFromSession(
      kFirstSessionId, /*metadata_to_delete=*/{kFirstStorageKey},
      std::move(maps_to_delete));
  EXPECT_TRUE(status.ok()) << status.ToString();

  // Verify the map is now empty.
  ASSERT_OK_AND_ASSIGN((std::map<DomStorageDatabase::Key,
                                 DomStorageDatabase::Value> actual_entries),
                       database->ReadMapKeyValues(kFirstMapLocator.Clone()));
  EXPECT_EQ(actual_entries.size(), 0u);
}

// Verifies deleting storage keys from a session removes metadata but leaves
// map key/value pairs intact when the map is not included in `maps_to_delete`.
TEST_F(SessionStorageSqliteTest, DeleteStorageKeysFromSessionWithMapExcluded) {
  std::unique_ptr<SessionStorageSqlite> database;
  ASSERT_NO_FATAL_FAILURE(OpenInMemory(&database));

  // Add one metadata row to the database.
  ASSERT_NO_FATAL_FAILURE(InitializeMetadata(
      *database, DomStorageDatabase::Metadata(CloneMapMetadataVector(
                     {{.map_locator{kFirstMapLocator.Clone()}}}))));

  // Add some key/values to the database.
  const std::map<DomStorageDatabase::Key, DomStorageDatabase::Value>
      kMapEntries{
          {ToBytes("key_1"), ToBytes("value_1")},
      };
  ASSERT_NO_FATAL_FAILURE(
      InsertMapEntries(*database, kFirstMapLocator.Clone(), kMapEntries));

  // Delete the storage key's metadata but not the map key/value entries.
  DbStatus status = database->DeleteStorageKeysFromSession(
      kFirstSessionId, /*metadata_to_delete=*/{kFirstStorageKey},
      /*maps_to_delete=*/{});
  EXPECT_TRUE(status.ok()) << status.ToString();

  // Verify the metadata was removed but the map key/values remain.
  ASSERT_OK_AND_ASSIGN(DomStorageDatabase::Metadata read_metadata,
                       database->ReadAllMetadata());
  EXPECT_EQ(read_metadata.map_metadata.size(), 0u);

  ASSERT_OK_AND_ASSIGN((std::map<DomStorageDatabase::Key,
                                 DomStorageDatabase::Value> actual_entries),
                       database->ReadMapKeyValues(kFirstMapLocator.Clone()));
  EXPECT_EQ(actual_entries, kMapEntries);
}

// Verifies deleting multiple storage keys from a session correctly removes
// metadata and selectively deletes map key/value pairs based on whether they're
// still referenced by other sessions (cloned maps).
TEST_F(SessionStorageSqliteTest,
       DeleteStorageKeysFromSessionWithMultipleStorageKeys) {
  std::unique_ptr<SessionStorageSqlite> database;
  ASSERT_NO_FATAL_FAILURE(OpenInMemory(&database));

  // Create three maps used by three storage keys across two sessions.
  std::vector<DomStorageDatabase::MapMetadata> expected_map_metadata;

  // The first session's metadata:
  expected_map_metadata.push_back({.map_locator{kFirstMapLocator.Clone()}});

  // Add two maps to the second session:
  expected_map_metadata.push_back({.map_locator{kSecondMapLocator.Clone()}});
  const DomStorageDatabase::MapLocator kThirdMapLocator{
      kSecondSessionId, kThirdStorageKey, kThirdMapId};
  expected_map_metadata.push_back({.map_locator{kThirdMapLocator.Clone()}});

  // Clone the first map in the second session.
  expected_map_metadata[0].map_locator.AddSession(kSecondSessionId);

  ASSERT_NO_FATAL_FAILURE(InitializeMetadata(
      *database, DomStorageDatabase::Metadata(
                     CloneMapMetadataVector(expected_map_metadata))));

  // Write map key/value pairs for all three maps.
  const std::map<DomStorageDatabase::Key, DomStorageDatabase::Value>
      kFirstMapEntries{
          {ToBytes("key_1"), ToBytes("value_1")},
          {ToBytes("key_2"), ToBytes("value_2")},
      };
  ASSERT_NO_FATAL_FAILURE(
      InsertMapEntries(*database, kFirstMapLocator.Clone(), kFirstMapEntries));

  const std::map<DomStorageDatabase::Key, DomStorageDatabase::Value>
      kSecondMapEntries{
          {ToBytes("key_3"), ToBytes("value_3")},
      };
  ASSERT_NO_FATAL_FAILURE(InsertMapEntries(*database, kSecondMapLocator.Clone(),
                                           kSecondMapEntries));

  const std::map<DomStorageDatabase::Key, DomStorageDatabase::Value>
      kThirdMapEntries{
          {ToBytes("key_4"), ToBytes("value_4")},
          {ToBytes("key_5"), ToBytes("value_5")},
          {ToBytes("key_6"), ToBytes("value_6")},
      };
  ASSERT_NO_FATAL_FAILURE(
      InsertMapEntries(*database, kThirdMapLocator.Clone(), kThirdMapEntries));

  // Delete `kFirstStorageKey` from `kFirstSessionId`. The map key/value pairs
  // must remain because `kSecondSessionId` still references the map.
  DbStatus status = database->DeleteStorageKeysFromSession(
      kFirstSessionId, /*metadata_to_delete=*/{kFirstStorageKey},
      /*maps_to_delete=*/{});
  EXPECT_TRUE(status.ok()) << status.ToString();

  // Verify the first session's metadata was removed but map data remains.
  expected_map_metadata[0].map_locator.RemoveSession(kFirstSessionId);
  ASSERT_OK_AND_ASSIGN(DomStorageDatabase::Metadata read_metadata,
                       database->ReadAllMetadata());
  ExpectEqualsMapMetadataSpan(read_metadata.map_metadata,
                              expected_map_metadata);

  ASSERT_OK_AND_ASSIGN((std::map<DomStorageDatabase::Key,
                                 DomStorageDatabase::Value> actual_entries),
                       database->ReadMapKeyValues(kFirstMapLocator.Clone()));
  EXPECT_EQ(actual_entries, kFirstMapEntries);

  ASSERT_OK_AND_ASSIGN(actual_entries,
                       database->ReadMapKeyValues(kSecondMapLocator.Clone()));
  EXPECT_EQ(actual_entries, kSecondMapEntries);

  ASSERT_OK_AND_ASSIGN(actual_entries,
                       database->ReadMapKeyValues(kThirdMapLocator.Clone()));
  EXPECT_EQ(actual_entries, kThirdMapEntries);

  // Delete `kFirstStorageKey` from `kSecondSessionId`, along with its now
  // unreferenced map.
  std::vector<DomStorageDatabase::MapLocator> maps_to_delete;
  maps_to_delete.emplace_back(kSecondSessionId, kFirstStorageKey, kFirstMapId);
  maps_to_delete.back().RemoveSession(kSecondSessionId);

  status = database->DeleteStorageKeysFromSession(
      kSecondSessionId, /*metadata_to_delete=*/{kFirstStorageKey},
      std::move(maps_to_delete));
  EXPECT_TRUE(status.ok()) << status.ToString();

  // Verify the first map's key/value pairs were deleted.
  ASSERT_OK_AND_ASSIGN(actual_entries,
                       database->ReadMapKeyValues(kFirstMapLocator.Clone()));
  EXPECT_EQ(actual_entries.size(), 0u);

  // Verify remaining maps are unaffected.
  ASSERT_OK_AND_ASSIGN(actual_entries,
                       database->ReadMapKeyValues(kSecondMapLocator.Clone()));
  EXPECT_EQ(actual_entries, kSecondMapEntries);

  ASSERT_OK_AND_ASSIGN(actual_entries,
                       database->ReadMapKeyValues(kThirdMapLocator.Clone()));
  EXPECT_EQ(actual_entries, kThirdMapEntries);

  // Verify remaining metadata, which must no longer include the first map.
  expected_map_metadata.erase(expected_map_metadata.begin());
  ASSERT_OK_AND_ASSIGN(read_metadata, database->ReadAllMetadata());
  ExpectEqualsMapMetadataSpan(read_metadata.map_metadata,
                              expected_map_metadata);
}

// Verifies deleting multiple storage keys from a session at the same
// time removes all their metadata entries and associated map key/value pairs.
TEST_F(SessionStorageSqliteTest, DeleteMultipleStorageKeysFromSession) {
  std::unique_ptr<SessionStorageSqlite> database;
  ASSERT_NO_FATAL_FAILURE(OpenInMemory(&database));

  // Create additional maps in the first session for this test.
  const DomStorageDatabase::MapLocator kFirstSessionSecondMapLocator{
      kFirstSessionId, kSecondStorageKey, kSecondMapId};
  const DomStorageDatabase::MapLocator kFirstSessionThirdMapLocator{
      kFirstSessionId, kThirdStorageKey, kThirdMapId};

  // Add metadata for three maps in the first session.
  ASSERT_NO_FATAL_FAILURE(InitializeMetadata(
      *database, DomStorageDatabase::Metadata(CloneMapMetadataVector({
                     {.map_locator{kFirstMapLocator.Clone()}},
                     {.map_locator{kFirstSessionSecondMapLocator.Clone()}},
                     {.map_locator{kFirstSessionThirdMapLocator.Clone()}},
                 }))));

  // Add key/value pairs to each of the three maps.
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
  ASSERT_NO_FATAL_FAILURE(InsertMapEntries(
      *database, kFirstSessionSecondMapLocator.Clone(), kSecondMapEntries));

  const std::map<DomStorageDatabase::Key, DomStorageDatabase::Value>
      kThirdMapEntries{
          {ToBytes("key_3"), ToBytes("value_3")},
      };
  ASSERT_NO_FATAL_FAILURE(InsertMapEntries(
      *database, kFirstSessionThirdMapLocator.Clone(), kThirdMapEntries));

  // Delete the first two storage keys and their maps, leaving the third.
  std::vector<DomStorageDatabase::MapLocator> maps_to_delete;
  maps_to_delete.push_back(kFirstMapLocator.Clone());
  maps_to_delete.back().RemoveSession(kFirstSessionId);
  maps_to_delete.push_back(kFirstSessionSecondMapLocator.Clone());
  maps_to_delete.back().RemoveSession(kFirstSessionId);

  DbStatus status = database->DeleteStorageKeysFromSession(
      kFirstSessionId, {kFirstStorageKey, kSecondStorageKey},
      std::move(maps_to_delete));
  EXPECT_TRUE(status.ok()) << status.ToString();

  // Verify only the third storage key's metadata remains.
  ASSERT_OK_AND_ASSIGN(DomStorageDatabase::Metadata read_metadata,
                       database->ReadAllMetadata());
  ExpectEqualsMapMetadataSpan(
      read_metadata.map_metadata,
      {{.map_locator{kFirstSessionThirdMapLocator.Clone()}}});

  // Verify the first two maps are empty.
  ASSERT_OK_AND_ASSIGN((std::map<DomStorageDatabase::Key,
                                 DomStorageDatabase::Value> actual_map_entries),
                       database->ReadMapKeyValues(kFirstMapLocator.Clone()));
  EXPECT_EQ(actual_map_entries.size(), 0u);

  ASSERT_OK_AND_ASSIGN(
      actual_map_entries,
      database->ReadMapKeyValues(kFirstSessionSecondMapLocator.Clone()));
  EXPECT_EQ(actual_map_entries.size(), 0u);

  // Verify the third map's key/value pairs remain.
  ASSERT_OK_AND_ASSIGN(
      actual_map_entries,
      database->ReadMapKeyValues(kFirstSessionThirdMapLocator.Clone()));
  EXPECT_EQ(actual_map_entries, kThirdMapEntries);
}

}  // namespace storage
