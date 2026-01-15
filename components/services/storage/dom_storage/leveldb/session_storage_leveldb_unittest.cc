// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/storage/dom_storage/leveldb/session_storage_leveldb.h"

#include <memory>
#include <optional>
#include <string>

#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_view_util.h"
#include "base/test/gmock_expected_support.h"
#include "base/test/task_environment.h"
#include "components/services/storage/dom_storage/dom_storage_database.h"
#include "components/services/storage/dom_storage/leveldb/test_support/test_leveldb_utils.h"
#include "components/services/storage/dom_storage/test_support/dom_storage_database_testing.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"

namespace storage {

// Declare internal functions for unit testing that `session_storage_leveldb.cc`
// defines.
StatusOr<DomStorageDatabase::MapMetadata> ParseMapMetadata(
    const DomStorageDatabase::KeyValuePair& namespace_entry);

DomStorageDatabase::Key GetSessionPrefix(const std::string& session_id);

DomStorageDatabase::Key CreateMapMetadataKey(
    std::string session_id,
    const blink::StorageKey& storage_key);

DomStorageDatabase::Key GetMapPrefix(int64_t map_id);

std::vector<uint8_t> ToBytes(std::string source);

namespace {
constexpr const char kFakeSessionId[] = "ce8c7dc5_73b4_4320_a506_ce1f4fd3356f";
constexpr const char kFakeUrlString[] = "https://a-fake.test/";
constexpr int64_t kFakeMapId = 1565;

constexpr const char kOtherFakeSessionId[] =
    "36356e0b_1627_4492_a474_db76a8996bed";
constexpr const char kOtherFakeUrlString[] = "https://b-fake.test/";
constexpr int64_t kOtherFakeMapId = 1566;

constexpr const char kThirdFakeSessionId[] =
    "5fe0e896_c6d8_4d2b_8b3c_d26f47832125";
constexpr const char kThirdFakeUrlString[] = "https://c-fake.test/";
constexpr int64_t kThirdFakeMapId = 1567;

constexpr const char kFourthFakeSessionId[] =
    "b5675eaf_30eb_462d_8d82_c6ba8e6bee4c";
constexpr int64_t kFourthFakeMapId = 1570;

constexpr const uint8_t kExpectedVersion[] = {'1'};

constexpr const char kScriptKey1[] = "key_1";
constexpr const char kScriptKey2[] = "key_2";

void VerifyDatabaseVersionEntry(
    const DomStorageDatabase::KeyValuePair& version_entry) {
  EXPECT_EQ(version_entry.key, ToBytes(kSessionStorageLevelDBVersionKey));
  EXPECT_EQ(version_entry.value, ToBytes(kExpectedVersion));
}

// Return "map-<map_id>-<script_key>".
DomStorageDatabase::Key CreateMapEntryKey(int64_t map_id,
                                          std::string script_key) {
  DomStorageDatabase::Key map_data_key = GetMapPrefix(map_id);

  map_data_key.insert(map_data_key.end(), script_key.begin(), script_key.end());
  return map_data_key;
}

void CloneMapAndVerifyResults(
    SessionStorageLevelDB& session_storage_leveldb,
    const DomStorageDatabase::MapLocator& source_map_locator,
    const DomStorageDatabase::MapLocator& target_map_locator,
    const std::map<DomStorageDatabase::Key, DomStorageDatabase::Value>&
        expected_entries) {
  DbStatus status = session_storage_leveldb.CloneMap(
      source_map_locator.Clone(), target_map_locator.Clone());
  EXPECT_TRUE(status.ok()) << status.ToString();

  // Verify the cloned entries exist.
  ASSERT_OK_AND_ASSIGN(
      (std::map<DomStorageDatabase::Key, DomStorageDatabase::Value>
           cloned_entries),
      session_storage_leveldb.ReadMapKeyValues(target_map_locator.Clone()));

  EXPECT_EQ(cloned_entries, expected_entries);

  // Verify the source entries did not change.
  ASSERT_OK_AND_ASSIGN(
      (std::map<DomStorageDatabase::Key, DomStorageDatabase::Value>
           source_entries),
      session_storage_leveldb.ReadMapKeyValues(source_map_locator.Clone()));
  EXPECT_EQ(source_entries, expected_entries);
}

}  // namespace

class SessionStorageLevelDBTest : public testing::Test {
 protected:
  SessionStorageLevelDBTest();
  ~SessionStorageLevelDBTest() override = default;

  void OpenInMemory(std::unique_ptr<SessionStorageLevelDB>* result);

  base::test::TaskEnvironment task_environment_;

  const blink::StorageKey kFakeUrlStorageKey;
  const blink::StorageKey kOtherFakeUrlStorageKey;
  const blink::StorageKey kThirdFakeStorageKey;
};

SessionStorageLevelDBTest::SessionStorageLevelDBTest()
    : kFakeUrlStorageKey(
          blink::StorageKey::CreateFromStringForTesting(kFakeUrlString)),
      kOtherFakeUrlStorageKey(
          blink::StorageKey::CreateFromStringForTesting(kOtherFakeUrlString)),
      kThirdFakeStorageKey(
          blink::StorageKey::CreateFromStringForTesting(kThirdFakeUrlString)) {}

void SessionStorageLevelDBTest::OpenInMemory(
    std::unique_ptr<SessionStorageLevelDB>* result) {
  auto instance = std::make_unique<SessionStorageLevelDB>(
      DomStorageDatabaseFactory::CreatePassKeyForTesting());

  DbStatus status =
      instance->Open(DomStorageDatabaseFactory::CreatePassKeyForTesting(),
                     /*directory=*/base::FilePath(),
                     /*memory_dump_id=*/std::nullopt);

  ASSERT_TRUE(status.ok()) << status.ToString();
  *result = std::move(instance);
}

TEST_F(SessionStorageLevelDBTest, ParseMapMetadata) {
  StatusOr<DomStorageDatabase::MapMetadata> map_metadata = ParseMapMetadata({
      CreateMapMetadataKey(kFakeSessionId, kFakeUrlStorageKey),
      /*value=*/ToBytes("314"),
  });
  ASSERT_TRUE(map_metadata.has_value());

  const DomStorageDatabase::MapMetadata kExpectedMapMetadata{
      .map_locator{kFakeSessionId, kFakeUrlStorageKey, /*map_id=*/314},
  };
  ExpectEqualsMapMetadata(*map_metadata, kExpectedMapMetadata);
}

TEST_F(SessionStorageLevelDBTest, ParseMapMetadataWithSessionIdMissing) {
  std::string invalid_key = base::StrCat({
      base::as_string_view(kNamespacePrefix),
      std::string(kFakeSessionId).substr(1),
  });
  StatusOr<DomStorageDatabase::MapMetadata> map_metadata = ParseMapMetadata({
      ToBytes(invalid_key),
      /*value=*/ToBytes("314"),
  });
  ASSERT_FALSE(map_metadata.has_value());
  EXPECT_TRUE(map_metadata.error().IsCorruption());
}

TEST_F(SessionStorageLevelDBTest, ParseMapMetadataWithSeparatorMissing) {
  std::string invalid_key = base::StrCat({
      base::as_string_view(kNamespacePrefix),
      kFakeSessionId,
  });
  StatusOr<DomStorageDatabase::MapMetadata> map_metadata = ParseMapMetadata({
      ToBytes(invalid_key),
      /*value=*/ToBytes("314"),
  });
  ASSERT_FALSE(map_metadata.has_value());
  EXPECT_TRUE(map_metadata.error().IsCorruption());
}

TEST_F(SessionStorageLevelDBTest, ParseMapMetadataWithSeparatorInvalid) {
  std::string invalid_key = base::StrCat({
      base::as_string_view(kNamespacePrefix),
      kFakeSessionId,
      std::string(/*count=*/1, '*'),
      kFakeUrlString,
  });
  StatusOr<DomStorageDatabase::MapMetadata> map_metadata = ParseMapMetadata({
      ToBytes(invalid_key),
      /*value=*/ToBytes("314"),
  });
  ASSERT_FALSE(map_metadata.has_value());
  EXPECT_TRUE(map_metadata.error().IsCorruption());
}

TEST_F(SessionStorageLevelDBTest, ParseMapMetadataWithStorageKeyInvalid) {
  std::string invalid_key = base::StrCat({
      base::as_string_view(kNamespacePrefix),
      kFakeSessionId,
      std::string(/*count=*/1, kNamespaceStorageKeySeparator),
      "https://a-fake.test",
  });
  StatusOr<DomStorageDatabase::MapMetadata> map_metadata = ParseMapMetadata({
      ToBytes(invalid_key),
      /*value=*/ToBytes("314"),
  });
  ASSERT_FALSE(map_metadata.has_value());
  EXPECT_TRUE(map_metadata.error().IsCorruption());
}

TEST_F(SessionStorageLevelDBTest, ParseMapMetadataWithMapIdInvalid) {
  StatusOr<DomStorageDatabase::MapMetadata> map_metadata = ParseMapMetadata({
      CreateMapMetadataKey(kFakeSessionId, kFakeUrlStorageKey),
      /*value=*/ToBytes("invalid"),
  });
  ASSERT_FALSE(map_metadata.has_value());
  EXPECT_TRUE(map_metadata.error().IsCorruption());
}

TEST_F(SessionStorageLevelDBTest, ParseMapMetadataWithMapIdEmpty) {
  StatusOr<DomStorageDatabase::MapMetadata> map_metadata = ParseMapMetadata({
      CreateMapMetadataKey(kFakeSessionId, kFakeUrlStorageKey),
      /*value=*/ToBytes(""),
  });
  ASSERT_FALSE(map_metadata.has_value());
  EXPECT_TRUE(map_metadata.error().IsCorruption());
}

TEST_F(SessionStorageLevelDBTest, ReadAllMapMetadataWithEmptyDB) {
  std::unique_ptr<SessionStorageLevelDB> session_storage_leveldb;
  ASSERT_NO_FATAL_FAILURE(OpenInMemory(&session_storage_leveldb));

  StatusOr<DomStorageDatabase::Metadata> metadata =
      session_storage_leveldb->ReadAllMetadata();
  ASSERT_TRUE(metadata.has_value());

  // The next map ID must start at zero.
  ASSERT_TRUE(metadata->next_map_id.has_value());
  EXPECT_EQ(*metadata->next_map_id, 0);
  EXPECT_EQ(metadata->map_metadata.size(), 0u);
}

TEST_F(SessionStorageLevelDBTest, ReadAllMapMetadataWithNextMapId) {
  std::unique_ptr<SessionStorageLevelDB> session_storage_leveldb;
  ASSERT_NO_FATAL_FAILURE(OpenInMemory(&session_storage_leveldb));

  ASSERT_NO_FATAL_FAILURE(WriteEntries(*session_storage_leveldb,
                                       {
                                           {
                                               ToBytes(kNextMapIdKey),
                                               /*value=*/ToBytes("576847"),
                                           },
                                       }));

  StatusOr<DomStorageDatabase::Metadata> metadata =
      session_storage_leveldb->ReadAllMetadata();
  ASSERT_TRUE(metadata.has_value());

  ASSERT_TRUE(metadata->next_map_id.has_value());
  EXPECT_EQ(*metadata->next_map_id, 576847);

  EXPECT_EQ(metadata->map_metadata.size(), 0u);
}

TEST_F(SessionStorageLevelDBTest, ReadAllMapMetadataWithNextMapIdInvalid) {
  std::unique_ptr<SessionStorageLevelDB> session_storage_leveldb;
  ASSERT_NO_FATAL_FAILURE(OpenInMemory(&session_storage_leveldb));

  ASSERT_NO_FATAL_FAILURE(WriteEntries(
      *session_storage_leveldb, {
                                    {
                                        ToBytes(kNextMapIdKey),
                                        /*value=*/ToBytes("not_a_number"),
                                    },
                                }));

  StatusOr<DomStorageDatabase::Metadata> metadata =
      session_storage_leveldb->ReadAllMetadata();

  ASSERT_FALSE(metadata.has_value());
  EXPECT_TRUE(metadata.error().IsCorruption());
}

TEST_F(SessionStorageLevelDBTest, ReadAllMapMetadata) {
  std::unique_ptr<SessionStorageLevelDB> session_storage_leveldb;
  ASSERT_NO_FATAL_FAILURE(OpenInMemory(&session_storage_leveldb));

  ASSERT_NO_FATAL_FAILURE(WriteEntries(
      *session_storage_leveldb,
      {
          {
              CreateMapMetadataKey(kFakeSessionId, kFakeUrlStorageKey),
              /*value=*/ToBytes("5343"),
          },
      }));

  StatusOr<DomStorageDatabase::Metadata> metadata =
      session_storage_leveldb->ReadAllMetadata();

  ASSERT_TRUE(metadata.has_value());

  ASSERT_TRUE(metadata->next_map_id.has_value());
  EXPECT_EQ(*metadata->next_map_id, 0);

  const DomStorageDatabase::MapMetadata kExpectedMapMetadata[] = {
      {
          .map_locator{kFakeSessionId, kFakeUrlStorageKey, /*map_id=*/5343},
      },
  };
  ExpectEqualsMapMetadataSpan(metadata->map_metadata, kExpectedMapMetadata);
}

TEST_F(SessionStorageLevelDBTest, ReadAllMapMetadataWithMapInvalid) {
  std::unique_ptr<SessionStorageLevelDB> session_storage_leveldb;
  ASSERT_NO_FATAL_FAILURE(OpenInMemory(&session_storage_leveldb));

  std::string invalid_map_key = base::StrCat({
      base::as_string_view(kNamespacePrefix),
      kFakeSessionId,
      std::string(/*count=*/1, kNamespaceStorageKeySeparator),
      "https://a-fake.test",
  });

  ASSERT_NO_FATAL_FAILURE(WriteEntries(*session_storage_leveldb,
                                       {
                                           {
                                               ToBytes(invalid_map_key),
                                               /*value=*/ToBytes("34325342"),
                                           },
                                       }));

  StatusOr<DomStorageDatabase::Metadata> metadata =
      session_storage_leveldb->ReadAllMetadata();

  ASSERT_FALSE(metadata.has_value());
  EXPECT_TRUE(metadata.error().IsCorruption());
}

TEST_F(SessionStorageLevelDBTest, ReadAllMapMetadataWithMultipleEntries) {
  std::unique_ptr<SessionStorageLevelDB> session_storage_leveldb;
  ASSERT_NO_FATAL_FAILURE(OpenInMemory(&session_storage_leveldb));

  ASSERT_NO_FATAL_FAILURE(WriteEntries(
      *session_storage_leveldb,
      {
          {
              CreateMapMetadataKey(kFakeSessionId, kFakeUrlStorageKey),
              /*value=*/ToBytes("5343"),
          },
          {
              CreateMapMetadataKey(kOtherFakeSessionId, kFakeUrlStorageKey),
              /*value=*/ToBytes("5343"),
          },
          {
              CreateMapMetadataKey(kOtherFakeSessionId,
                                   kOtherFakeUrlStorageKey),
              /*value=*/ToBytes("5346"),
          },
      }));

  StatusOr<DomStorageDatabase::Metadata> metadata =
      session_storage_leveldb->ReadAllMetadata();

  ASSERT_TRUE(metadata.has_value());

  ASSERT_TRUE(metadata->next_map_id.has_value());
  EXPECT_EQ(*metadata->next_map_id, 0);

  DomStorageDatabase::MapMetadata expected_map_metadata[] = {
      {
          .map_locator{kOtherFakeSessionId, kFakeUrlStorageKey,
                       /*map_id=*/5343},
      },
      {
          .map_locator{kOtherFakeSessionId, kOtherFakeUrlStorageKey,
                       /*map_id=*/5346},
      },
  };

  // Both `kFakeSessionId` and `kOtherFakeSessionId` use cloned map 5343.
  expected_map_metadata[0].map_locator.AddSession(kFakeSessionId);

  ExpectEqualsMapMetadataSpan(metadata->map_metadata, expected_map_metadata);
}

TEST_F(SessionStorageLevelDBTest, CreateMapMetadataKey) {
  DomStorageDatabase::Key key =
      CreateMapMetadataKey(kFakeSessionId, kFakeUrlStorageKey);
  EXPECT_EQ(key, ToBytes("namespace-ce8c7dc5_73b4_4320_a506_ce1f4fd3356f-https:"
                         "//a-fake.test/"));
}

TEST_F(SessionStorageLevelDBTest, PutMetadata) {
  constexpr int64_t kNextMapId = kFakeMapId + 1;

  std::unique_ptr<SessionStorageLevelDB> session_storage_leveldb;
  ASSERT_NO_FATAL_FAILURE(OpenInMemory(&session_storage_leveldb));

  DomStorageDatabase::Metadata metadata;
  metadata.next_map_id = kNextMapId;
  metadata.map_metadata.push_back(
      {.map_locator{kFakeSessionId, kFakeUrlStorageKey, kFakeMapId}});

  DbStatus status = session_storage_leveldb->PutMetadata(std::move(metadata));
  EXPECT_TRUE(status.ok()) << status.ToString();

  // Verify the contents in the database, which includes the "version" entry.
  ASSERT_OK_AND_ASSIGN(
      std::vector<DomStorageDatabase::KeyValuePair> all_entries,
      session_storage_leveldb->GetLevelDBForTesting().GetPrefixed({}));
  ASSERT_EQ(all_entries.size(), 3u);

  EXPECT_EQ(all_entries[0].key,
            CreateMapMetadataKey(kFakeSessionId, kFakeUrlStorageKey));
  EXPECT_EQ(all_entries[0].value,
            base::as_byte_span(base::NumberToString(1565)));

  EXPECT_EQ(all_entries[1].key, ToBytes(kNextMapIdKey));
  EXPECT_EQ(all_entries[1].value,
            base::as_byte_span(base::NumberToString(kNextMapId)));

  VerifyDatabaseVersionEntry(all_entries[2]);
}

TEST_F(SessionStorageLevelDBTest, PutMetadataWithMultipleMaps) {
  constexpr int64_t kNextMapId = kOtherFakeMapId + 2;

  std::unique_ptr<SessionStorageLevelDB> session_storage_leveldb;
  ASSERT_NO_FATAL_FAILURE(OpenInMemory(&session_storage_leveldb));

  // Update the metadata for 3 different maps.
  DomStorageDatabase::Metadata metadata;
  metadata.next_map_id = kNextMapId;

  metadata.map_metadata.push_back(
      {.map_locator{kFakeSessionId, kFakeUrlStorageKey, kFakeMapId}});

  metadata.map_metadata.push_back(
      {.map_locator{kFakeSessionId, kOtherFakeUrlStorageKey, kOtherFakeMapId}});

  metadata.map_metadata.push_back({.map_locator{
      kOtherFakeSessionId, kOtherFakeUrlStorageKey, kOtherFakeMapId + 1}});

  DbStatus status = session_storage_leveldb->PutMetadata(std::move(metadata));

  // Repeat database verification twice.  The second `PutMetadata()` adds empty
  // metadata, which must not alter the database.
  for (int i = 0; i < 2; ++i) {
    EXPECT_TRUE(status.ok()) << status.ToString();

    // Verify the contents in the database, which includes the "version" entry.
    ASSERT_OK_AND_ASSIGN(
        std::vector<DomStorageDatabase::KeyValuePair> all_entries,
        session_storage_leveldb->GetLevelDBForTesting().GetPrefixed({}));
    ASSERT_EQ(all_entries.size(), 5u);

    EXPECT_EQ(
        all_entries[0].key,
        CreateMapMetadataKey(kOtherFakeSessionId, kOtherFakeUrlStorageKey));
    EXPECT_EQ(all_entries[0].value,
              base::as_byte_span(base::NumberToString(kOtherFakeMapId + 1)));

    EXPECT_EQ(all_entries[1].key,
              CreateMapMetadataKey(kFakeSessionId, kFakeUrlStorageKey));
    EXPECT_EQ(all_entries[1].value,
              base::as_byte_span(base::NumberToString(kFakeMapId)));

    EXPECT_EQ(all_entries[2].key,
              CreateMapMetadataKey(kFakeSessionId, kOtherFakeUrlStorageKey));
    EXPECT_EQ(all_entries[2].value,
              base::as_byte_span(base::NumberToString(kOtherFakeMapId)));

    EXPECT_EQ(all_entries[3].key, ToBytes(kNextMapIdKey));
    EXPECT_EQ(all_entries[3].value,
              base::as_byte_span(base::NumberToString(kNextMapId)));

    VerifyDatabaseVersionEntry(all_entries[4]);

    // Adding empty metadata must not modify the database.
    status = session_storage_leveldb->PutMetadata(/*metadata=*/{});
  }
}

TEST_F(SessionStorageLevelDBTest, PutMetadataWithMultipleSessions) {
  constexpr int64_t kNextMapId = kFakeMapId + 1;

  std::unique_ptr<SessionStorageLevelDB> session_storage_leveldb;
  ASSERT_NO_FATAL_FAILURE(OpenInMemory(&session_storage_leveldb));

  // Create the test metadata to read and write.
  std::vector<DomStorageDatabase::MapMetadata> expected_map_metadata;
  expected_map_metadata.push_back(
      {.map_locator{kFakeSessionId, kFakeUrlStorageKey, kFakeMapId}});

  // Clone `kFakeMapId` for `kOtherFakeSessionId`.
  expected_map_metadata[0].map_locator.AddSession(kOtherFakeSessionId);

  // Write `expected_map_metadata` to the database.
  DomStorageDatabase::Metadata write_metadata;
  write_metadata.next_map_id = kNextMapId;
  write_metadata.map_metadata = CloneMapMetadataVector(expected_map_metadata);
  DbStatus status =
      session_storage_leveldb->PutMetadata(std::move(write_metadata));
  EXPECT_TRUE(status.ok()) << status.ToString();

  // Read the metadata from the database, which must equal
  // `expected_map_metadata`.
  ASSERT_OK_AND_ASSIGN(DomStorageDatabase::Metadata read_metadata,
                       session_storage_leveldb->ReadAllMetadata());
  EXPECT_EQ(read_metadata.next_map_id, kNextMapId);
  ExpectEqualsMapMetadataSpan(read_metadata.map_metadata,
                              expected_map_metadata);
}

TEST_F(SessionStorageLevelDBTest, GetMapPrefix) {
  EXPECT_EQ(GetMapPrefix(1234), ToBytes("map-1234-"));
}

TEST_F(SessionStorageLevelDBTest, DeleteStorageKeysFromSessionWithMetadata) {
  std::unique_ptr<SessionStorageLevelDB> session_storage_leveldb;
  ASSERT_NO_FATAL_FAILURE(OpenInMemory(&session_storage_leveldb));

  // Add one metadata entry to the database.
  DomStorageDatabase::Metadata metadata;
  metadata.map_metadata.push_back({.map_locator{
      kFakeSessionId,
      kFakeUrlStorageKey,
      kFakeMapId,
  }});
  DbStatus status = session_storage_leveldb->PutMetadata(std::move(metadata));
  EXPECT_TRUE(status.ok()) << status.ToString();

  // Verify the database contains the `metadata` and "VERSION" entries.
  ASSERT_OK_AND_ASSIGN(
      std::vector<DomStorageDatabase::KeyValuePair> all_entries,
      session_storage_leveldb->GetLevelDBForTesting().GetPrefixed({}));
  EXPECT_EQ(all_entries.size(), 2u);

  // Delete the `metadata` entry from the database.
  status = session_storage_leveldb->DeleteStorageKeysFromSession(
      kFakeSessionId, /*metadata_to_delete=*/{kFakeUrlStorageKey},
      /*maps_to_delete=*/{});
  EXPECT_TRUE(status.ok()) << status.ToString();

  // Verify the contents in the database, which should only include the
  // "VERSION" entry.
  ASSERT_OK_AND_ASSIGN(
      all_entries,
      session_storage_leveldb->GetLevelDBForTesting().GetPrefixed({}));

  EXPECT_TRUE(status.ok()) << status.ToString();
  ASSERT_EQ(all_entries.size(), 1u);

  VerifyDatabaseVersionEntry(all_entries[0]);
}

TEST_F(SessionStorageLevelDBTest,
       DeleteStorageKeysFromSessionWithMapKeyValues) {
  std::unique_ptr<SessionStorageLevelDB> session_storage_leveldb;
  ASSERT_NO_FATAL_FAILURE(OpenInMemory(&session_storage_leveldb));

  // Add two map key/value entries to the database.
  ASSERT_NO_FATAL_FAILURE(WriteEntries(
      *session_storage_leveldb, {
                                    {
                                        CreateMapEntryKey(kFakeMapId, "key_1"),
                                        ToBytes("value_1"),
                                    },
                                    {
                                        CreateMapEntryKey(kFakeMapId, "key_2"),
                                        ToBytes("value_2"),
                                    },
                                }));

  // Verify the database contains two `key/value` entries and one "VERSION"
  // entry.
  ASSERT_OK_AND_ASSIGN(
      std::vector<DomStorageDatabase::KeyValuePair> all_entries,
      session_storage_leveldb->GetLevelDBForTesting().GetPrefixed({}));
  EXPECT_EQ(all_entries.size(), 3u);

  // Delete the two key/value entries from the database.
  std::vector<DomStorageDatabase::MapLocator> maps_to_delete;
  maps_to_delete.emplace_back(kFakeSessionId, kFakeUrlStorageKey, kFakeMapId);
  maps_to_delete.back().RemoveSession(kFakeSessionId);

  DbStatus status = session_storage_leveldb->DeleteStorageKeysFromSession(
      kFakeSessionId, /*metadata_to_delete=*/{kFakeUrlStorageKey},
      std::move(maps_to_delete));
  EXPECT_TRUE(status.ok()) << status.ToString();

  // Verify the contents in the database, which should only include the
  // "VERSION" entry.
  ASSERT_OK_AND_ASSIGN(
      all_entries,
      session_storage_leveldb->GetLevelDBForTesting().GetPrefixed({}));

  EXPECT_TRUE(status.ok()) << status.ToString();
  ASSERT_EQ(all_entries.size(), 1u);

  VerifyDatabaseVersionEntry(all_entries[0]);
}

TEST_F(SessionStorageLevelDBTest, DeleteStorageKeysFromSessionWithMapExcluded) {
  std::unique_ptr<SessionStorageLevelDB> session_storage_leveldb;
  ASSERT_NO_FATAL_FAILURE(OpenInMemory(&session_storage_leveldb));

  // Add one metadata entry to the database.
  DomStorageDatabase::Metadata metadata;
  metadata.map_metadata.push_back({.map_locator{
      kFakeSessionId,
      kFakeUrlStorageKey,
      kFakeMapId,
  }});
  DbStatus status = session_storage_leveldb->PutMetadata(std::move(metadata));
  EXPECT_TRUE(status.ok()) << status.ToString();

  // Add one key/value entry to the database.
  ASSERT_NO_FATAL_FAILURE(WriteEntries(
      *session_storage_leveldb, {
                                    {
                                        CreateMapEntryKey(kFakeMapId, "key_1"),
                                        ToBytes("value_1"),
                                    },
                                }));

  // Verify the database contains the metadata, key/value and "VERSION" entries.
  ASSERT_OK_AND_ASSIGN(
      std::vector<DomStorageDatabase::KeyValuePair> all_entries,
      session_storage_leveldb->GetLevelDBForTesting().GetPrefixed({}));
  EXPECT_EQ(all_entries.size(), 3u);

  // Delete the `metadata` entry from the database.
  status = session_storage_leveldb->DeleteStorageKeysFromSession(
      kFakeSessionId, /*metadata_to_delete=*/{kFakeUrlStorageKey},
      /*maps_to_delete=*/{});
  EXPECT_TRUE(status.ok()) << status.ToString();

  // Verify the contents in the database, which should include the
  // map key/value entry and the "VERSION" entry.
  ASSERT_OK_AND_ASSIGN(
      all_entries,
      session_storage_leveldb->GetLevelDBForTesting().GetPrefixed({}));

  EXPECT_TRUE(status.ok()) << status.ToString();
  ASSERT_EQ(all_entries.size(), 2u);

  // Verify the map key/value entry for `kFakeMapId`.
  EXPECT_EQ(all_entries[0].key, CreateMapEntryKey(kFakeMapId, "key_1"));
  EXPECT_EQ(all_entries[0].value, ToBytes("value_1"));

  VerifyDatabaseVersionEntry(all_entries[1]);
}

TEST_F(SessionStorageLevelDBTest,
       DeleteStorageKeysFromSessionWithMultipleStorageKeys) {
  std::unique_ptr<SessionStorageLevelDB> session_storage_leveldb;
  ASSERT_NO_FATAL_FAILURE(OpenInMemory(&session_storage_leveldb));

  // Create 3 maps used by 3 storage keys across two sessions.
  //
  // Add 4 metadata entries with one for each map usage.
  const DomStorageDatabase::MapMetadata kExpectedMapMetadataArray[] = {
      // The first session's metadata:
      {
          .map_locator{
              kFakeSessionId,
              kFakeUrlStorageKey,
              kFakeMapId,
          },
      },
      // The second session's metadata:
      {
          .map_locator{
              kOtherFakeSessionId,
              kFakeUrlStorageKey,
              kFakeMapId,
          },
      },
      {
          .map_locator{
              kOtherFakeSessionId,
              kOtherFakeUrlStorageKey,
              kOtherFakeMapId,
          },
      },
      {
          .map_locator{
              kOtherFakeSessionId,
              kThirdFakeStorageKey,
              kThirdFakeMapId,
          },
      },
  };
  std::vector<DomStorageDatabase::MapMetadata> expected_map_metadata =
      CloneMapMetadataVector(kExpectedMapMetadataArray);

  DomStorageDatabase::Metadata metadata;
  metadata.map_metadata = CloneMapMetadataVector(expected_map_metadata);
  DbStatus status = session_storage_leveldb->PutMetadata(std::move(metadata));
  EXPECT_TRUE(status.ok()) << status.ToString();

  // Write 6 map key/value entries.
  ASSERT_NO_FATAL_FAILURE(
      WriteEntries(*session_storage_leveldb,
                   {
                       // Write the first map's key/value pairs.
                       {
                           CreateMapEntryKey(kFakeMapId, "key_1"),
                           ToBytes("value_1"),
                       },
                       {
                           CreateMapEntryKey(kFakeMapId, "key_2"),
                           ToBytes("value_2"),
                       },
                       // Write the second map's key/value pairs.
                       {
                           CreateMapEntryKey(kOtherFakeMapId, "key_3"),
                           ToBytes("value_3"),
                       },
                       // Write the third map's key/value pairs.
                       {
                           CreateMapEntryKey(kThirdFakeMapId, "key_4"),
                           ToBytes("value_4"),
                       },
                       {
                           CreateMapEntryKey(kThirdFakeMapId, "key_5"),
                           ToBytes("value_5"),
                       },
                       {
                           CreateMapEntryKey(kThirdFakeMapId, "key_6"),
                           ToBytes("value_6"),
                       },
                   }));

  // Verify the database contains the four `metadata` entries, six `key/value`
  // entries and one "VERSION" entry.
  ASSERT_OK_AND_ASSIGN(
      std::vector<DomStorageDatabase::KeyValuePair> all_entries,
      session_storage_leveldb->GetLevelDBForTesting().GetPrefixed({}));
  EXPECT_EQ(all_entries.size(), 11u);

  // Delete `kFakeUrlStorageKey` from `kFakeSessionId`, which must remove one
  // metadata entry.
  status = session_storage_leveldb->DeleteStorageKeysFromSession(
      kFakeSessionId, /*metadata_to_delete=*/{kFakeUrlStorageKey},
      /*maps_to_delete=*/{});
  EXPECT_TRUE(status.ok()) << status.ToString();

  // Verify the contents in the database, which should include the following:
  // - Three metadata entries for `kOtherFakeSessionId`
  // - Six map key/value entries for `kFakeMapId`, `kOtherFakeMapId` and
  //   `kThirdFakeMapId`.
  // - One database VERSION entry.
  ASSERT_OK_AND_ASSIGN(
      all_entries,
      session_storage_leveldb->GetLevelDBForTesting().GetPrefixed({}));

  EXPECT_TRUE(status.ok()) << status.ToString();
  ASSERT_EQ(all_entries.size(), 10u);

  // Verify the map key/value entries for `kFakeMapId`.
  EXPECT_EQ(all_entries[0].key, CreateMapEntryKey(kFakeMapId, "key_1"));
  EXPECT_EQ(all_entries[0].value, ToBytes("value_1"));

  EXPECT_EQ(all_entries[1].key, CreateMapEntryKey(kFakeMapId, "key_2"));
  EXPECT_EQ(all_entries[1].value, ToBytes("value_2"));

  // Verify the map key/value entries for `kOtherFakeMapId`.
  EXPECT_EQ(all_entries[2].key, CreateMapEntryKey(kOtherFakeMapId, "key_3"));
  EXPECT_EQ(all_entries[2].value, ToBytes("value_3"));

  // Verify the map key/value entries for `kThirdFakeMapId`.
  EXPECT_EQ(all_entries[3].key, CreateMapEntryKey(kThirdFakeMapId, "key_4"));
  EXPECT_EQ(all_entries[3].value, ToBytes("value_4"));

  EXPECT_EQ(all_entries[4].key, CreateMapEntryKey(kThirdFakeMapId, "key_5"));
  EXPECT_EQ(all_entries[4].value, ToBytes("value_5"));

  EXPECT_EQ(all_entries[5].key, CreateMapEntryKey(kThirdFakeMapId, "key_6"));
  EXPECT_EQ(all_entries[5].value, ToBytes("value_6"));
  VerifyDatabaseVersionEntry(all_entries[9]);

  // Verify the three metadata entries for `kOtherFakeSessionId`.
  // Pop `kFakeUrlStorageKey` for `kFakeSessionId` from the front of
  // `expected_map_metadata`.
  expected_map_metadata.erase(expected_map_metadata.begin());
  ASSERT_OK_AND_ASSIGN(metadata, session_storage_leveldb->ReadAllMetadata());
  ExpectEqualsMapMetadataSpan(metadata.map_metadata, expected_map_metadata);

  // Delete `kFakeUrlStorageKey` from `kOtherFakeSessionId`, which must remove
  // one metadata entry and two map key/value entries.
  std::vector<DomStorageDatabase::MapLocator> maps_to_delete;
  maps_to_delete.emplace_back(kOtherFakeSessionId, kFakeUrlStorageKey,
                              kFakeMapId);
  maps_to_delete.back().RemoveSession(kOtherFakeSessionId);

  status = session_storage_leveldb->DeleteStorageKeysFromSession(
      kOtherFakeSessionId, /*metadata_to_delete=*/{kFakeUrlStorageKey},
      std::move(maps_to_delete));
  EXPECT_TRUE(status.ok()) << status.ToString();

  // Verify the contents in the database, which should include the following:
  // - Two metadata entries for `kOtherFakeSessionId`
  // - Four map key/value entry for `kOtherFakeMapId`.
  // - One database VERSION entry.
  all_entries.clear();
  ASSERT_OK_AND_ASSIGN(
      all_entries,
      session_storage_leveldb->GetLevelDBForTesting().GetPrefixed({}));

  EXPECT_TRUE(status.ok()) << status.ToString();
  ASSERT_EQ(all_entries.size(), 7u);

  // Verify the map key/value entry for `kOtherFakeMapId`.
  EXPECT_EQ(all_entries[0].key, CreateMapEntryKey(kOtherFakeMapId, "key_3"));
  EXPECT_EQ(all_entries[0].value, ToBytes("value_3"));

  // Verify the map key/value entries for `kThirdFakeMapId`.
  EXPECT_EQ(all_entries[1].key, CreateMapEntryKey(kThirdFakeMapId, "key_4"));
  EXPECT_EQ(all_entries[1].value, ToBytes("value_4"));

  EXPECT_EQ(all_entries[2].key, CreateMapEntryKey(kThirdFakeMapId, "key_5"));
  EXPECT_EQ(all_entries[2].value, ToBytes("value_5"));

  EXPECT_EQ(all_entries[3].key, CreateMapEntryKey(kThirdFakeMapId, "key_6"));
  EXPECT_EQ(all_entries[3].value, ToBytes("value_6"));

  VerifyDatabaseVersionEntry(all_entries[6]);

  // Verify the two remaining metadata entries in `kOtherFakeSessionId`.
  // Pop `kFakeUrlStorageKey` for `kFakeOtherSessionId` from the front of
  // `expected_map_metadata`.
  expected_map_metadata.erase(expected_map_metadata.begin());
  ASSERT_OK_AND_ASSIGN(metadata, session_storage_leveldb->ReadAllMetadata());
  ExpectEqualsMapMetadataSpan(metadata.map_metadata, expected_map_metadata);

  // Delete `kOtherFakeUrlStorageKey` and `kThirdFakeStorageKey` from
  // `kOtherFakeSessionId`, which must remove two metadata entries and four map
  // key/value entries.
  maps_to_delete.clear();

  maps_to_delete.emplace_back(kOtherFakeSessionId, kOtherFakeUrlStorageKey,
                              kOtherFakeMapId);
  maps_to_delete.back().RemoveSession(kOtherFakeSessionId);

  maps_to_delete.emplace_back(kOtherFakeSessionId, kThirdFakeStorageKey,
                              kThirdFakeMapId);
  maps_to_delete.back().RemoveSession(kOtherFakeSessionId);

  status = session_storage_leveldb->DeleteStorageKeysFromSession(
      kOtherFakeSessionId,
      /*metadata_to_delete=*/
      {kOtherFakeUrlStorageKey, kThirdFakeStorageKey},
      std::move(maps_to_delete));
  EXPECT_TRUE(status.ok()) << status.ToString();

  // Verify the contents in the database, which should only include the
  // "VERSION" entry.
  all_entries.clear();
  ASSERT_OK_AND_ASSIGN(
      all_entries,
      session_storage_leveldb->GetLevelDBForTesting().GetPrefixed({}));

  EXPECT_TRUE(status.ok()) << status.ToString();
  ASSERT_EQ(all_entries.size(), 1u);

  VerifyDatabaseVersionEntry(all_entries[0]);
}

TEST_F(SessionStorageLevelDBTest, GetSessionPrefix) {
  EXPECT_EQ(GetSessionPrefix(kFakeSessionId),
            ToBytes("namespace-ce8c7dc5_73b4_4320_a506_ce1f4fd3356f-"));
}

TEST_F(SessionStorageLevelDBTest, DeleteSessionsWithMetadata) {
  std::unique_ptr<SessionStorageLevelDB> session_storage_leveldb;
  ASSERT_NO_FATAL_FAILURE(OpenInMemory(&session_storage_leveldb));

  // Add one metadata entry to the database.
  DomStorageDatabase::Metadata metadata;
  metadata.map_metadata.push_back({.map_locator{
      kFakeSessionId,
      kFakeUrlStorageKey,
      kFakeMapId,
  }});
  DbStatus status = session_storage_leveldb->PutMetadata(std::move(metadata));
  EXPECT_TRUE(status.ok()) << status.ToString();

  // Verify the database contains the `metadata` and "VERSION" entries.
  ASSERT_OK_AND_ASSIGN(
      std::vector<DomStorageDatabase::KeyValuePair> all_entries,
      session_storage_leveldb->GetLevelDBForTesting().GetPrefixed({}));
  EXPECT_EQ(all_entries.size(), 2u);

  // Delete the `metadata` entry from the database.
  status = session_storage_leveldb->DeleteSessions({kFakeSessionId},
                                                   /*maps_to_delete=*/{});
  EXPECT_TRUE(status.ok()) << status.ToString();

  // Verify the contents in the database, which should only include the
  // "VERSION" entry.
  ASSERT_OK_AND_ASSIGN(
      all_entries,
      session_storage_leveldb->GetLevelDBForTesting().GetPrefixed({}));

  EXPECT_TRUE(status.ok()) << status.ToString();
  ASSERT_EQ(all_entries.size(), 1u);

  VerifyDatabaseVersionEntry(all_entries[0]);
}

TEST_F(SessionStorageLevelDBTest, DeleteSessionsWithMapKeyValues) {
  std::unique_ptr<SessionStorageLevelDB> session_storage_leveldb;
  ASSERT_NO_FATAL_FAILURE(OpenInMemory(&session_storage_leveldb));

  // Add two map key/value entries to the database.
  ASSERT_NO_FATAL_FAILURE(WriteEntries(
      *session_storage_leveldb, {
                                    {
                                        CreateMapEntryKey(kFakeMapId, "key_1"),
                                        ToBytes("value_1"),
                                    },
                                    {
                                        CreateMapEntryKey(kFakeMapId, "key_2"),
                                        ToBytes("value_2"),
                                    },
                                }));

  // Verify the database contains two `key/value` entries and one "VERSION"
  // entry.
  ASSERT_OK_AND_ASSIGN(
      std::vector<DomStorageDatabase::KeyValuePair> all_entries,
      session_storage_leveldb->GetLevelDBForTesting().GetPrefixed({}));
  EXPECT_EQ(all_entries.size(), 3u);

  // Delete the two key/value entries from the database.
  std::vector<DomStorageDatabase::MapLocator> maps_to_delete;
  maps_to_delete.emplace_back(kFakeSessionId, kFakeUrlStorageKey, kFakeMapId);
  maps_to_delete.back().RemoveSession(kFakeSessionId);

  DbStatus status = session_storage_leveldb->DeleteSessions(
      {kFakeSessionId}, std::move(maps_to_delete));
  EXPECT_TRUE(status.ok()) << status.ToString();

  // Verify the contents in the database, which should only include the
  // "VERSION" entry.
  ASSERT_OK_AND_ASSIGN(
      all_entries,
      session_storage_leveldb->GetLevelDBForTesting().GetPrefixed({}));

  EXPECT_TRUE(status.ok()) << status.ToString();
  ASSERT_EQ(all_entries.size(), 1u);

  VerifyDatabaseVersionEntry(all_entries[0]);
}

TEST_F(SessionStorageLevelDBTest, DeleteSessionsWithMapExcluded) {
  std::unique_ptr<SessionStorageLevelDB> session_storage_leveldb;
  ASSERT_NO_FATAL_FAILURE(OpenInMemory(&session_storage_leveldb));

  // Add one metadata entry to the database.
  DomStorageDatabase::Metadata metadata;
  metadata.map_metadata.push_back({.map_locator{
      kFakeSessionId,
      kFakeUrlStorageKey,
      kFakeMapId,
  }});
  DbStatus status = session_storage_leveldb->PutMetadata(std::move(metadata));
  EXPECT_TRUE(status.ok()) << status.ToString();

  // Add one key/value entry to the database.
  ASSERT_NO_FATAL_FAILURE(WriteEntries(
      *session_storage_leveldb, {
                                    {
                                        CreateMapEntryKey(kFakeMapId, "key_1"),
                                        ToBytes("value_1"),
                                    },
                                }));

  // Verify the database contains the metadata, key/value and "VERSION" entries.
  ASSERT_OK_AND_ASSIGN(
      std::vector<DomStorageDatabase::KeyValuePair> all_entries,
      session_storage_leveldb->GetLevelDBForTesting().GetPrefixed({}));
  EXPECT_EQ(all_entries.size(), 3u);

  // Delete the `metadata` entry from the database.
  status = session_storage_leveldb->DeleteSessions({kFakeSessionId},
                                                   /*maps_to_delete=*/{});
  EXPECT_TRUE(status.ok()) << status.ToString();

  // Verify the contents in the database, which should include the
  // map key/value entry and the "VERSION" entry.
  ASSERT_OK_AND_ASSIGN(
      all_entries,
      session_storage_leveldb->GetLevelDBForTesting().GetPrefixed({}));

  EXPECT_TRUE(status.ok()) << status.ToString();
  ASSERT_EQ(all_entries.size(), 2u);

  // Verify the map key/value entry for `kFakeMapId`.
  EXPECT_EQ(all_entries[0].key, CreateMapEntryKey(kFakeMapId, "key_1"));
  EXPECT_EQ(all_entries[0].value, ToBytes("value_1"));

  VerifyDatabaseVersionEntry(all_entries[1]);
}

TEST_F(SessionStorageLevelDBTest, DeleteSessionsWithMultipleStorageKeys) {
  std::unique_ptr<SessionStorageLevelDB> session_storage_leveldb;
  ASSERT_NO_FATAL_FAILURE(OpenInMemory(&session_storage_leveldb));

  // Create two maps used by two storage keys across three sessions.
  //
  // Add 4 metadata entries with one for each map usage.
  std::vector<DomStorageDatabase::MapMetadata> expected_map_metadata;
  expected_map_metadata.push_back({
      .map_locator{
          kFakeSessionId,
          kFakeUrlStorageKey,
          kFakeMapId,
      },
  });
  // Clone `kFakeMapId` for `kOtherFakeSessionId`.
  expected_map_metadata[0].map_locator.AddSession(kOtherFakeSessionId);
  expected_map_metadata.push_back({
      .map_locator{
          kOtherFakeSessionId,
          kOtherFakeUrlStorageKey,
          kOtherFakeMapId,
      },
  });
  // Clone `kOtherFakeMapId` for `kThirdFakeSessionId`.
  expected_map_metadata[1].map_locator.AddSession(kThirdFakeSessionId);

  DomStorageDatabase::Metadata metadata;
  metadata.map_metadata = CloneMapMetadataVector(expected_map_metadata);
  DbStatus status = session_storage_leveldb->PutMetadata(std::move(metadata));
  EXPECT_TRUE(status.ok()) << status.ToString();

  // Write 3 map key/value entries.
  ASSERT_NO_FATAL_FAILURE(
      WriteEntries(*session_storage_leveldb,
                   {// Write the first map's key/value pairs.
                    {
                        CreateMapEntryKey(kFakeMapId, "key_1"),
                        ToBytes("value_1"),
                    },
                    {
                        CreateMapEntryKey(kFakeMapId, "key_2"),
                        ToBytes("value_2"),
                    },
                    // Write the second map's key/value pairs.
                    {
                        CreateMapEntryKey(kOtherFakeMapId, "key_3"),
                        ToBytes("value_3"),
                    }}));

  // Verify the database contains the four `metadata` entries, three `key/value`
  // entries and one "VERSION" entry.
  ASSERT_OK_AND_ASSIGN(
      std::vector<DomStorageDatabase::KeyValuePair> all_entries,
      session_storage_leveldb->GetLevelDBForTesting().GetPrefixed({}));
  EXPECT_EQ(all_entries.size(), 8u);

  // Delete `kFakeSessionId`, which must remove one metadata entry.
  status = session_storage_leveldb->DeleteSessions({kFakeSessionId},
                                                   /*maps_to_delete=*/{});
  EXPECT_TRUE(status.ok()) << status.ToString();

  // Verify the contents in the database, which should include the following:
  // - Three metadata entries for `kOtherFakeSessionId` and
  //   `kThirdFakeSessionId`.
  // - Three map key/value entries for `kFakeMapId`, and `kOtherFakeMapId`.
  // - One database VERSION entry.
  ASSERT_OK_AND_ASSIGN(
      all_entries,
      session_storage_leveldb->GetLevelDBForTesting().GetPrefixed({}));

  EXPECT_TRUE(status.ok()) << status.ToString();
  ASSERT_EQ(all_entries.size(), 7u);

  // Verify the map key/value entries for `kFakeMapId`.
  EXPECT_EQ(all_entries[0].key, CreateMapEntryKey(kFakeMapId, "key_1"));
  EXPECT_EQ(all_entries[0].value, ToBytes("value_1"));

  EXPECT_EQ(all_entries[1].key, CreateMapEntryKey(kFakeMapId, "key_2"));
  EXPECT_EQ(all_entries[1].value, ToBytes("value_2"));

  // Verify the map key/value entries for `kOtherFakeMapId`.
  EXPECT_EQ(all_entries[2].key, CreateMapEntryKey(kOtherFakeMapId, "key_3"));
  EXPECT_EQ(all_entries[2].value, ToBytes("value_3"));

  VerifyDatabaseVersionEntry(all_entries[6]);

  // Verify the three metadata entries for `kOtherFakeSessionId` and
  // `kThirdFakeSessionId`. Pop `kFakeSessionId` from the front of
  // `expected_map_metadata`.
  expected_map_metadata[0].map_locator.RemoveSession(kFakeSessionId);
  ASSERT_OK_AND_ASSIGN(metadata, session_storage_leveldb->ReadAllMetadata());
  ExpectEqualsMapMetadataSpan(metadata.map_metadata, expected_map_metadata);

  // Delete `kOtherFakeSessionId` along with its no longer referenced map:
  // `kFakeMapId`. This must remove one metadata entry and two map key/value
  // entries.
  std::vector<DomStorageDatabase::MapLocator> maps_to_delete;
  maps_to_delete.emplace_back(kOtherFakeSessionId, kFakeUrlStorageKey,
                              kFakeMapId);
  maps_to_delete.back().RemoveSession(kOtherFakeSessionId);

  status = session_storage_leveldb->DeleteSessions({kOtherFakeSessionId},
                                                   std::move(maps_to_delete));
  EXPECT_TRUE(status.ok()) << status.ToString();

  // Verify the contents in the database, which should include the following:
  // - One metadata entry for `kThirdFakeSessionId`.
  // - One map key/value entry for `kOtherFakeMapId`.
  // - One database VERSION entry.
  ASSERT_OK_AND_ASSIGN(
      all_entries,
      session_storage_leveldb->GetLevelDBForTesting().GetPrefixed({}));

  EXPECT_TRUE(status.ok()) << status.ToString();
  ASSERT_EQ(all_entries.size(), 3u);

  // Verify the map key/value entries for `kOtherFakeMapId`.
  EXPECT_EQ(all_entries[0].key, CreateMapEntryKey(kOtherFakeMapId, "key_3"));
  EXPECT_EQ(all_entries[0].value, ToBytes("value_3"));

  VerifyDatabaseVersionEntry(all_entries[2]);

  // Verify the one metadata entries for `kThirdFakeMapId`.  Pop
  // `kOtherFakeSessionId` from the front of`expected_map_metadata`.
  expected_map_metadata.erase(expected_map_metadata.begin());
  expected_map_metadata[0].map_locator.RemoveSession(kOtherFakeSessionId);
  ASSERT_OK_AND_ASSIGN(metadata, session_storage_leveldb->ReadAllMetadata());
  ExpectEqualsMapMetadataSpan(metadata.map_metadata, expected_map_metadata);
}

TEST_F(SessionStorageLevelDBTest, ReadMapKeyValuesWithEmpty) {
  std::unique_ptr<SessionStorageLevelDB> session_storage_leveldb;
  ASSERT_NO_FATAL_FAILURE(OpenInMemory(&session_storage_leveldb));

  // An empty database must have no key/value pairs.
  DomStorageDatabase::MapLocator map_locator{kFakeSessionId, kFakeUrlStorageKey,
                                             kFakeMapId};
  ASSERT_OK_AND_ASSIGN(
      (std::map<DomStorageDatabase::Key, DomStorageDatabase::Value> entries),
      session_storage_leveldb->ReadMapKeyValues(std::move(map_locator)));
  EXPECT_EQ(entries.size(), 0u);
}

TEST_F(SessionStorageLevelDBTest, ReadMapKeyValues) {
  std::unique_ptr<SessionStorageLevelDB> session_storage_leveldb;
  ASSERT_NO_FATAL_FAILURE(OpenInMemory(&session_storage_leveldb));

  // Add two key/value pairs to a single map.
  const DomStorageDatabase::Value kValue1 = ToBytes("value_1");
  const DomStorageDatabase::Value kValue2 = ToBytes("value_2");

  ASSERT_NO_FATAL_FAILURE(
      WriteEntries(*session_storage_leveldb,
                   {
                       {
                           CreateMapEntryKey(kFakeMapId, kScriptKey1),
                           kValue1,
                       },
                       {
                           CreateMapEntryKey(kFakeMapId, kScriptKey2),
                           kValue2,
                       },
                   }));

  // Read the two key/value pairs from the database.
  DomStorageDatabase::MapLocator map_locator{kFakeSessionId, kFakeUrlStorageKey,
                                             kFakeMapId};
  ASSERT_OK_AND_ASSIGN(
      (std::map<DomStorageDatabase::Key, DomStorageDatabase::Value> entries),
      session_storage_leveldb->ReadMapKeyValues(std::move(map_locator)));

  ASSERT_EQ(entries.size(), 2u);
  EXPECT_EQ(entries[ToBytes(kScriptKey1)], kValue1);
  EXPECT_EQ(entries[ToBytes(kScriptKey2)], kValue2);
}

TEST_F(SessionStorageLevelDBTest, ReadMapKeyValuesWithMultipleMaps) {
  std::unique_ptr<SessionStorageLevelDB> session_storage_leveldb;
  ASSERT_NO_FATAL_FAILURE(OpenInMemory(&session_storage_leveldb));

  // Create two maps, adding a key/value pair to each map.
  const DomStorageDatabase::Value kValue1 = ToBytes("value_1");
  const DomStorageDatabase::Value kValue2 = ToBytes("value_2");

  ASSERT_NO_FATAL_FAILURE(
      WriteEntries(*session_storage_leveldb,
                   {
                       {
                           CreateMapEntryKey(kFakeMapId, kScriptKey1),
                           kValue1,
                       },
                       {
                           CreateMapEntryKey(kOtherFakeMapId, kScriptKey2),
                           kValue2,
                       },
                   }));

  // Read the first map's key/value pair.
  DomStorageDatabase::MapLocator map_locator{kFakeSessionId, kFakeUrlStorageKey,
                                             kFakeMapId};
  ASSERT_OK_AND_ASSIGN(
      (std::map<DomStorageDatabase::Key, DomStorageDatabase::Value> entries),
      session_storage_leveldb->ReadMapKeyValues(std::move(map_locator)));

  ASSERT_EQ(entries.size(), 1u);
  EXPECT_EQ(entries[ToBytes(kScriptKey1)], kValue1);

  // Read the second map's key/value pair.
  DomStorageDatabase::MapLocator other_map_locator{
      kFakeSessionId, kFakeUrlStorageKey, kOtherFakeMapId};
  ASSERT_OK_AND_ASSIGN(entries, session_storage_leveldb->ReadMapKeyValues(
                                    std::move(other_map_locator)));

  ASSERT_EQ(entries.size(), 1u);
  EXPECT_EQ(entries[ToBytes(kScriptKey2)], kValue2);
}

TEST_F(SessionStorageLevelDBTest, CloneMapWithEmpty) {
  std::unique_ptr<SessionStorageLevelDB> session_storage_leveldb;
  ASSERT_NO_FATAL_FAILURE(OpenInMemory(&session_storage_leveldb));

  // Clone an empty map, which is a no-op.
  DomStorageDatabase::MapLocator source_map_locator{
      kFakeSessionId, kFakeUrlStorageKey, kFakeMapId};

  DomStorageDatabase::MapLocator target_map_locator{
      kOtherFakeSessionId, kFakeUrlStorageKey, kOtherFakeMapId};

  ASSERT_NO_FATAL_FAILURE(
      CloneMapAndVerifyResults(*session_storage_leveldb, source_map_locator,
                               target_map_locator, /*expected_entries=*/{}));
}

TEST_F(SessionStorageLevelDBTest, CloneMap) {
  std::unique_ptr<SessionStorageLevelDB> session_storage_leveldb;
  ASSERT_NO_FATAL_FAILURE(OpenInMemory(&session_storage_leveldb));

  // Add two key/value pairs to a single map.
  const DomStorageDatabase::Value kValue1 = ToBytes("value_1");
  const DomStorageDatabase::Value kValue2 = ToBytes("value_2");

  ASSERT_NO_FATAL_FAILURE(
      WriteEntries(*session_storage_leveldb,
                   {
                       {
                           CreateMapEntryKey(kFakeMapId, kScriptKey1),
                           kValue1,
                       },
                       {
                           CreateMapEntryKey(kFakeMapId, kScriptKey2),
                           kValue2,
                       },
                   }));

  // Clone the map with two entries.
  DomStorageDatabase::MapLocator source_map_locator{
      kFakeSessionId, kFakeUrlStorageKey, kFakeMapId};

  DomStorageDatabase::MapLocator target_map_locator{
      kOtherFakeSessionId, kFakeUrlStorageKey, kOtherFakeMapId};

  ASSERT_NO_FATAL_FAILURE(
      CloneMapAndVerifyResults(*session_storage_leveldb, source_map_locator,
                               target_map_locator, /*expected_entries=*/
                               {
                                   {
                                       ToBytes(kScriptKey1),
                                       kValue1,
                                   },
                                   {
                                       ToBytes(kScriptKey2),
                                       kValue2,
                                   },
                               }));
}

TEST_F(SessionStorageLevelDBTest, CloneMapWithMultipleMaps) {
  std::unique_ptr<SessionStorageLevelDB> session_storage_leveldb;
  ASSERT_NO_FATAL_FAILURE(OpenInMemory(&session_storage_leveldb));

  // Create two maps, adding a key/value pair to each map.
  const DomStorageDatabase::Value kValue1 = ToBytes("value_1");
  const DomStorageDatabase::Value kValue2 = ToBytes("value_2");

  ASSERT_NO_FATAL_FAILURE(
      WriteEntries(*session_storage_leveldb,
                   {
                       {
                           CreateMapEntryKey(kFakeMapId, kScriptKey1),
                           kValue1,
                       },
                       {
                           CreateMapEntryKey(kOtherFakeMapId, kScriptKey2),
                           kValue2,
                       },
                   }));

  // Clone the first map.
  DomStorageDatabase::MapLocator first_source_map_locator{
      kFakeSessionId, kFakeUrlStorageKey, kFakeMapId};

  DomStorageDatabase::MapLocator first_target_map_locator{
      kThirdFakeSessionId, kFakeUrlStorageKey, kThirdFakeMapId};

  ASSERT_NO_FATAL_FAILURE(CloneMapAndVerifyResults(
      *session_storage_leveldb, first_source_map_locator,
      first_target_map_locator, /*expected_entries=*/
      {
          {
              ToBytes(kScriptKey1),
              kValue1,
          },
      }));

  // Clone the second map.
  DomStorageDatabase::MapLocator second_source_map_locator{
      kOtherFakeSessionId, kFakeUrlStorageKey, kOtherFakeMapId};

  DomStorageDatabase::MapLocator second_target_map_locator{
      kFourthFakeSessionId, kFakeUrlStorageKey, kFourthFakeMapId};

  ASSERT_NO_FATAL_FAILURE(CloneMapAndVerifyResults(
      *session_storage_leveldb, second_source_map_locator,
      second_target_map_locator, /*expected_entries=*/
      {
          {
              ToBytes(kScriptKey2),
              kValue2,
          },
      }));
}

TEST_F(SessionStorageLevelDBTest, UpdateMaps) {
  std::unique_ptr<SessionStorageLevelDB> session_storage_leveldb;
  ASSERT_NO_FATAL_FAILURE(OpenInMemory(&session_storage_leveldb));

  DomStorageDatabase::MapLocator map1_locator{kFakeSessionId,
                                              kFakeUrlStorageKey, kFakeMapId};

  DomStorageDatabase::MapLocator map2_locator{
      kFakeSessionId, kFakeUrlStorageKey, kOtherFakeMapId};

  ASSERT_NO_FATAL_FAILURE(
      TestUpdateMaps(*session_storage_leveldb, map1_locator, map2_locator));
}

}  // namespace storage
