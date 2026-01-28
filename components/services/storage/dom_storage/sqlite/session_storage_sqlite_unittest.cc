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
constexpr int64_t kThirdMapId = 1567;

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

  const blink::StorageKey kFirstStorageKey =
      blink::StorageKey::CreateFromStringForTesting(kFirstUrlString);

  const blink::StorageKey kSecondStorageKey =
      blink::StorageKey::CreateFromStringForTesting(kSecondUrlString);

  base::ScopedTempDir temp_dir_;
  base::test::TaskEnvironment task_environment_;
  base::test::ScopedFeatureList scoped_feature_list_;
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
  constexpr int64_t kFirstNextMapId = 56;
  DomStorageDatabase::Metadata first_metadata_to_write;
  first_metadata_to_write.next_map_id = kFirstNextMapId;

  DbStatus status = database->PutMetadata(std::move(first_metadata_to_write));
  EXPECT_TRUE(status.ok()) << status.ToString();

  // Verify the first `next_map_id` was persisted.
  ASSERT_OK_AND_ASSIGN(DomStorageDatabase::Metadata read_metadata,
                       database->ReadAllMetadata());

  ASSERT_TRUE(read_metadata.next_map_id.has_value());
  EXPECT_EQ(*read_metadata.next_map_id, kFirstNextMapId);
  EXPECT_EQ(read_metadata.map_metadata.size(), 0u);

  // Write a second `next_map_id` value to overwrite the first.
  constexpr int64_t kSecondNextMapId = 57;
  DomStorageDatabase::Metadata second_metadata_to_write;
  second_metadata_to_write.next_map_id = kSecondNextMapId;

  status = database->PutMetadata(std::move(second_metadata_to_write));
  EXPECT_TRUE(status.ok()) << status.ToString();

  // Verify the second `next_map_id` replaced the first.
  ASSERT_OK_AND_ASSIGN(read_metadata, database->ReadAllMetadata());
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

  DomStorageDatabase::Metadata first_metadata_to_write;
  first_metadata_to_write.map_metadata =
      CloneMapMetadataVector(kMapMetadataToInsert);

  DbStatus status = database->PutMetadata(std::move(first_metadata_to_write));
  EXPECT_TRUE(status.ok()) << status.ToString();

  // Verify the map metadata was persisted.
  ASSERT_OK_AND_ASSIGN(DomStorageDatabase::Metadata read_metadata,
                       database->ReadAllMetadata());

  ASSERT_TRUE(read_metadata.next_map_id.has_value());
  EXPECT_EQ(*read_metadata.next_map_id, 0);
  ExpectEqualsMapMetadataSpan(read_metadata.map_metadata, kMapMetadataToInsert);

  // Replace the map metadata with a new `map_id` for the same session and
  // storage key.
  const DomStorageDatabase::MapMetadata kReplacementMapMetadata[] = {
      {
          .map_locator{kFirstSessionId, kFirstStorageKey, kSecondMapId},
      },
  };

  DomStorageDatabase::Metadata second_metadata_to_write;
  second_metadata_to_write.map_metadata =
      CloneMapMetadataVector(kReplacementMapMetadata);

  status = database->PutMetadata(std::move(second_metadata_to_write));
  EXPECT_TRUE(status.ok()) << status.ToString();

  // Verify the replacement map metadata overwrote the original.
  ASSERT_OK_AND_ASSIGN(read_metadata, database->ReadAllMetadata());

  ASSERT_TRUE(read_metadata.next_map_id.has_value());
  EXPECT_EQ(*read_metadata.next_map_id, 0);
  ExpectEqualsMapMetadataSpan(read_metadata.map_metadata,
                              kReplacementMapMetadata);
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

  DbStatus status = database->PutMetadata(std::move(metadata));
  EXPECT_TRUE(status.ok()) << status.ToString();

  // Verify all map metadata was persisted correctly.
  ASSERT_OK_AND_ASSIGN(DomStorageDatabase::Metadata read_metadata,
                       database->ReadAllMetadata());

  ASSERT_TRUE(read_metadata.next_map_id.has_value());
  EXPECT_EQ(*read_metadata.next_map_id, kNextMapId);
  ExpectEqualsMapMetadataSpan(read_metadata.map_metadata,
                              expected_map_metadata);
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

    DbStatus status = database->PutMetadata(std::move(metadata));
    EXPECT_TRUE(status.ok()) << status.ToString();

    // Verify metadata was written correctly before closing.
    ASSERT_OK_AND_ASSIGN(DomStorageDatabase::Metadata read_metadata,
                         database->ReadAllMetadata());

    ASSERT_TRUE(read_metadata.next_map_id.has_value());
    EXPECT_EQ(*read_metadata.next_map_id, kNextMapId);
    ExpectEqualsMapMetadataSpan(read_metadata.map_metadata,
                                expected_map_metadata);
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

}  // namespace storage
