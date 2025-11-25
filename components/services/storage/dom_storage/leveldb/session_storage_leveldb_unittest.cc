// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/storage/dom_storage/leveldb/session_storage_leveldb.h"

#include <memory>
#include <optional>
#include <string>

#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
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

std::vector<uint8_t> ToBytes(std::string source);

namespace {
constexpr const char kFakeSessionId[] = "ce8c7dc5_73b4_4320_a506_ce1f4fd3356f";
constexpr const char kFakeUrlString[] = "https://a-fake.test/";
constexpr int64_t kFakeMapId = 1565;

constexpr const char kOtherFakeSessionId[] =
    "36356e0b_1627_4492_a474_db76a8996bed";
constexpr const char kOtherFakeUrlString[] = "https://b-fake.test/";
constexpr int64_t kOtherFakeMapId = 1566;

constexpr const uint8_t kExpectedVersion[] = {'1'};

void VerifyDatabaseVersionEntry(
    const DomStorageDatabase::KeyValuePair& version_entry) {
  EXPECT_EQ(version_entry.key, ToBytes(kSessionStorageLevelDBVersionKey));
  EXPECT_EQ(version_entry.value, ToBytes(kExpectedVersion));
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
};

SessionStorageLevelDBTest::SessionStorageLevelDBTest()
    : kFakeUrlStorageKey(
          blink::StorageKey::CreateFromStringForTesting(kFakeUrlString)),
      kOtherFakeUrlStorageKey(
          blink::StorageKey::CreateFromStringForTesting(kOtherFakeUrlString)) {}

void SessionStorageLevelDBTest::OpenInMemory(
    std::unique_ptr<SessionStorageLevelDB>* result) {
  auto instance = std::make_unique<SessionStorageLevelDB>(
      DomStorageDatabaseFactory::CreatePassKeyForTesting());

  DbStatus status = instance->Open(
      DomStorageDatabaseFactory::CreatePassKeyForTesting(),
      /*directory=*/base::FilePath(), "SessionStorageLevelDBTest",
      /*memory_dump_id=*/std::nullopt);

  ASSERT_TRUE(status.ok()) << status.ToString();
  *result = std::move(instance);
}

TEST_F(SessionStorageLevelDBTest, ParseMapMetadata) {
  StatusOr<DomStorageDatabase::MapMetadata> map_metadata = ParseMapMetadata({
      SessionStorageLevelDB::CreateMapMetadataKey(kFakeSessionId,
                                                  kFakeUrlStorageKey),
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
      SessionStorageLevelDB::CreateMapMetadataKey(kFakeSessionId,
                                                  kFakeUrlStorageKey),
      /*value=*/ToBytes("invalid"),
  });
  ASSERT_FALSE(map_metadata.has_value());
  EXPECT_TRUE(map_metadata.error().IsCorruption());
}

TEST_F(SessionStorageLevelDBTest, ParseMapMetadataWithMapIdEmpty) {
  StatusOr<DomStorageDatabase::MapMetadata> map_metadata = ParseMapMetadata({
      SessionStorageLevelDB::CreateMapMetadataKey(kFakeSessionId,
                                                  kFakeUrlStorageKey),
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

  ASSERT_NO_FATAL_FAILURE(
      WriteEntries(*session_storage_leveldb,
                   {
                       {
                           SessionStorageLevelDB::CreateMapMetadataKey(
                               kFakeSessionId, kFakeUrlStorageKey),
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

  ASSERT_NO_FATAL_FAILURE(
      WriteEntries(*session_storage_leveldb,
                   {
                       {
                           SessionStorageLevelDB::CreateMapMetadataKey(
                               kFakeSessionId, kFakeUrlStorageKey),
                           /*value=*/ToBytes("5343"),
                       },
                       {
                           SessionStorageLevelDB::CreateMapMetadataKey(
                               kOtherFakeSessionId, kFakeUrlStorageKey),
                           /*value=*/ToBytes("5343"),
                       },
                       {
                           SessionStorageLevelDB::CreateMapMetadataKey(
                               kOtherFakeSessionId, kOtherFakeUrlStorageKey),
                           /*value=*/ToBytes("5346"),
                       },
                   }));

  StatusOr<DomStorageDatabase::Metadata> metadata =
      session_storage_leveldb->ReadAllMetadata();

  ASSERT_TRUE(metadata.has_value());

  ASSERT_TRUE(metadata->next_map_id.has_value());
  EXPECT_EQ(*metadata->next_map_id, 0);

  const DomStorageDatabase::MapMetadata kExpectedMapMetadata[] = {
      {
          .map_locator{kOtherFakeSessionId, kFakeUrlStorageKey,
                       /*map_id=*/5343},
      },
      {
          .map_locator{kOtherFakeSessionId, kOtherFakeUrlStorageKey,
                       /*map_id=*/5346},
      },
      {
          .map_locator{kFakeSessionId, kFakeUrlStorageKey, /*map_id=*/5343},
      },
  };
  ExpectEqualsMapMetadataSpan(metadata->map_metadata, kExpectedMapMetadata);
}

TEST_F(SessionStorageLevelDBTest, CreateMapMetadataKey) {
  DomStorageDatabase::Key key = SessionStorageLevelDB::CreateMapMetadataKey(
      kFakeSessionId, kFakeUrlStorageKey);
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
      session_storage_leveldb->GetLevelDB().GetPrefixed({}));
  ASSERT_EQ(all_entries.size(), 3u);

  EXPECT_EQ(all_entries[0].key, SessionStorageLevelDB::CreateMapMetadataKey(
                                    kFakeSessionId, kFakeUrlStorageKey));
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
        session_storage_leveldb->GetLevelDB().GetPrefixed({}));
    ASSERT_EQ(all_entries.size(), 5u);

    EXPECT_EQ(all_entries[0].key,
              SessionStorageLevelDB::CreateMapMetadataKey(
                  kOtherFakeSessionId, kOtherFakeUrlStorageKey));
    EXPECT_EQ(all_entries[0].value,
              base::as_byte_span(base::NumberToString(kOtherFakeMapId + 1)));

    EXPECT_EQ(all_entries[1].key, SessionStorageLevelDB::CreateMapMetadataKey(
                                      kFakeSessionId, kFakeUrlStorageKey));
    EXPECT_EQ(all_entries[1].value,
              base::as_byte_span(base::NumberToString(kFakeMapId)));

    EXPECT_EQ(all_entries[2].key, SessionStorageLevelDB::CreateMapMetadataKey(
                                      kFakeSessionId, kOtherFakeUrlStorageKey));
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

}  // namespace storage
