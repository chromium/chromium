// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/storage/dom_storage/leveldb/local_storage_leveldb.h"

#include <stdint.h>

#include <optional>
#include <string>
#include <vector>

#include "base/byte_size.h"
#include "base/containers/span.h"
#include "base/strings/strcat.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "components/services/storage/dom_storage/dom_storage_constants.h"
#include "components/services/storage/dom_storage/dom_storage_database.h"
#include "components/services/storage/dom_storage/leveldb/dom_storage_batch_operation_leveldb.h"
#include "components/services/storage/dom_storage/leveldb/dom_storage_database_leveldb.h"
#include "components/services/storage/dom_storage/test_support/dom_storage_database_testing.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"

namespace storage {

// Declare internal functions for unit testing that `local_storage_leveldb->cc`
// defines.
std::vector<uint8_t> ToBytes(std::string source);

std::optional<blink::StorageKey> TryExtractStorageKeyFromPrefixedKey(
    const DomStorageDatabase::Key& key,
    const DomStorageDatabase::KeyView& meta_data_prefix);

std::optional<DomStorageDatabase::MapMetadata> TryParseWriteMetadata(
    const DomStorageDatabase::KeyValuePair& leveldb_meta_entry);

std::optional<DomStorageDatabase::MapMetadata> TryParseAccessMetadata(
    const DomStorageDatabase::KeyValuePair& leveldb_meta_access_entry);

namespace {

constexpr const char kFakeUrlString[] = "https://fake.test";
constexpr base::ByteSize kMapTotalSize{312};

}  // namespace

class LocalStorageLevelDBTest : public testing::Test {
 protected:
  LocalStorageLevelDBTest();
  ~LocalStorageLevelDBTest() override = default;

  void OpenInMemory(std::unique_ptr<LocalStorageLevelDB>* result);

  // Populate the LevelDB with test values.
  void WriteEntries(LocalStorageLevelDB& database,
                    std::vector<DomStorageDatabase::KeyValuePair> entries);

  base::test::TaskEnvironment task_environment_;
  const blink::StorageKey kFakeUrlStorageKey;
  const base::Time kMapLastAccessed;
  const base::Time kMapLastModified;
};

LocalStorageLevelDBTest::LocalStorageLevelDBTest()
    : kFakeUrlStorageKey(
          blink::StorageKey::CreateFromStringForTesting(kFakeUrlString)),
      kMapLastAccessed(base::Time::Now() - base::Minutes(10)),
      kMapLastModified(base::Time::Now()) {}

void LocalStorageLevelDBTest::OpenInMemory(
    std::unique_ptr<LocalStorageLevelDB>* result) {
  auto instance = std::make_unique<LocalStorageLevelDB>(
      DomStorageDatabaseFactory::CreatePassKeyForTesting());

  DbStatus status =
      instance->Open(DomStorageDatabaseFactory::CreatePassKeyForTesting(),
                     /*directory=*/base::FilePath(), "LocalStorageLevelDBTest",
                     /*memory_dump_id=*/std::nullopt);

  ASSERT_TRUE(status.ok()) << status.ToString();
  *result = std::move(instance);
}

void LocalStorageLevelDBTest::WriteEntries(
    LocalStorageLevelDB& database,
    std::vector<DomStorageDatabase::KeyValuePair> entries) {
  std::unique_ptr<DomStorageBatchOperationLevelDB> batch =
      database.GetLevelDB().CreateBatchOperation();

  for (const DomStorageDatabase::KeyValuePair& entry : entries) {
    batch->Put(entry.key, entry.value);
  }

  DbStatus status = batch->Commit();
  ASSERT_TRUE(status.ok()) << status.ToString();
}

TEST_F(LocalStorageLevelDBTest, CreateAccessMetaDataKey) {
  DomStorageDatabase::Key access_metadata_key =
      LocalStorageLevelDB::CreateAccessMetaDataKey(kFakeUrlStorageKey);
  EXPECT_EQ(access_metadata_key,
            base::as_byte_span(std::string("METAACCESS:https://fake.test")));
}

TEST_F(LocalStorageLevelDBTest, CreateWriteMetaDataKey) {
  DomStorageDatabase::Key write_metadata_key =
      LocalStorageLevelDB::CreateWriteMetaDataKey(kFakeUrlStorageKey);
  EXPECT_EQ(write_metadata_key,
            base::as_byte_span(std::string("META:https://fake.test")));
}

TEST_F(LocalStorageLevelDBTest, TryExtractStorageKeyWithEmptyPrefix) {
  DomStorageDatabase::Key write_metadata_key =
      LocalStorageLevelDB::CreateWriteMetaDataKey(kFakeUrlStorageKey);

  std::optional<blink::StorageKey> storage_key =
      TryExtractStorageKeyFromPrefixedKey(write_metadata_key,
                                          /*meta_data_prefix=*/{});
  EXPECT_EQ(storage_key, std::nullopt);
}

TEST_F(LocalStorageLevelDBTest, TryExtractStorageKeyWithEmptyKey) {
  std::optional<blink::StorageKey> storage_key =
      TryExtractStorageKeyFromPrefixedKey(/*key=*/{}, kWriteMetaPrefix);
  EXPECT_EQ(storage_key, std::nullopt);
}

TEST_F(LocalStorageLevelDBTest, TryExtractStorageKeyWithTooLongPrefix) {
  DomStorageDatabase::Key write_metadata_key =
      LocalStorageLevelDB::CreateWriteMetaDataKey(kFakeUrlStorageKey);

  std::optional<blink::StorageKey> storage_key =
      TryExtractStorageKeyFromPrefixedKey(
          write_metadata_key,
          base::as_byte_span(std::string("META:https://fake.test")));
  EXPECT_EQ(storage_key, std::nullopt);
}

TEST_F(LocalStorageLevelDBTest, TryExtractStorageKeyWithInvalidStorageKey) {
  const DomStorageDatabase::Key kInvalidKey =
      ToBytes(std::string("META:invalid:\\\\fake.test"));

  std::optional<blink::StorageKey> storage_key =
      TryExtractStorageKeyFromPrefixedKey(kInvalidKey, kWriteMetaPrefix);
  EXPECT_EQ(storage_key, std::nullopt);
}

TEST_F(LocalStorageLevelDBTest, TryExtractStorageKeyWithWriteMetaData) {
  DomStorageDatabase::Key write_metadata_key =
      LocalStorageLevelDB::CreateWriteMetaDataKey(kFakeUrlStorageKey);

  std::optional<blink::StorageKey> storage_key =
      TryExtractStorageKeyFromPrefixedKey(write_metadata_key, kWriteMetaPrefix);
  EXPECT_EQ(storage_key, kFakeUrlStorageKey);
}

TEST_F(LocalStorageLevelDBTest, TryExtractStorageKeyWithAccessMetaData) {
  DomStorageDatabase::Key access_metadata_key =
      LocalStorageLevelDB::CreateAccessMetaDataKey(kFakeUrlStorageKey);

  std::optional<blink::StorageKey> storage_key =
      TryExtractStorageKeyFromPrefixedKey(access_metadata_key,
                                          kAccessMetaPrefix);
  EXPECT_EQ(storage_key, kFakeUrlStorageKey);
}

TEST_F(LocalStorageLevelDBTest, TryExtractStorageKeyWithWriteMetadataMismatch) {
  DomStorageDatabase::Key access_metadata_key =
      LocalStorageLevelDB::CreateAccessMetaDataKey(kFakeUrlStorageKey);

  std::optional<blink::StorageKey> storage_key =
      TryExtractStorageKeyFromPrefixedKey(access_metadata_key,
                                          kWriteMetaPrefix);
  EXPECT_EQ(storage_key, std::nullopt);
}

TEST_F(LocalStorageLevelDBTest,
       TryExtractStorageKeyWithAccessMetadataMismatch) {
  DomStorageDatabase::Key write_metadata_key =
      LocalStorageLevelDB::CreateWriteMetaDataKey(kFakeUrlStorageKey);

  std::optional<blink::StorageKey> storage_key =
      TryExtractStorageKeyFromPrefixedKey(write_metadata_key,
                                          kAccessMetaPrefix);
  EXPECT_EQ(storage_key, std::nullopt);
}

TEST_F(LocalStorageLevelDBTest, TryParseAccessMetadataWithInvalidKey) {
  std::optional<DomStorageDatabase::MapMetadata> map_metadata =
      TryParseAccessMetadata({
          LocalStorageLevelDB::CreateWriteMetaDataKey(kFakeUrlStorageKey),
          LocalStorageLevelDB::CreateAccessMetaDataValue(kMapLastAccessed),
      });
  EXPECT_EQ(map_metadata, std::nullopt);
}

TEST_F(LocalStorageLevelDBTest, TryParseAccessMetadataWithInvalidValue) {
  std::optional<DomStorageDatabase::MapMetadata> map_metadata =
      TryParseAccessMetadata({
          LocalStorageLevelDB::CreateAccessMetaDataKey(kFakeUrlStorageKey),
          /*value=*/{0x1, 0x2, 0x3},
      });
  EXPECT_EQ(map_metadata, std::nullopt);
}

TEST_F(LocalStorageLevelDBTest, TryParseAccessMetadata) {
  std::optional<DomStorageDatabase::MapMetadata> map_metadata =
      TryParseAccessMetadata({
          LocalStorageLevelDB::CreateAccessMetaDataKey(kFakeUrlStorageKey),
          LocalStorageLevelDB::CreateAccessMetaDataValue(kMapLastAccessed),
      });

  ASSERT_NE(map_metadata, std::nullopt);

  const DomStorageDatabase::MapMetadata kExpectedMapMetadata{
      .map_locator{kLocalStorageSessionId, kFakeUrlStorageKey},
      .last_accessed{kMapLastAccessed},
  };
  ExpectEqualsMapMetadata(*map_metadata, kExpectedMapMetadata);
}

TEST_F(LocalStorageLevelDBTest, TryParseWriteMetadataWithInvalidKey) {
  std::optional<DomStorageDatabase::MapMetadata> map_metadata =
      TryParseWriteMetadata({
          LocalStorageLevelDB::CreateAccessMetaDataKey(kFakeUrlStorageKey),
          LocalStorageLevelDB::CreateWriteMetaDataValue(kMapLastModified,
                                                        kMapTotalSize),
      });
  EXPECT_EQ(map_metadata, std::nullopt);
}

TEST_F(LocalStorageLevelDBTest, TryParseWriteMetadataWithInvalidValue) {
  std::optional<DomStorageDatabase::MapMetadata> map_metadata =
      TryParseWriteMetadata({
          LocalStorageLevelDB::CreateWriteMetaDataKey(kFakeUrlStorageKey),
          LocalStorageLevelDB::CreateAccessMetaDataValue(kMapLastAccessed),
      });
  EXPECT_EQ(map_metadata, std::nullopt);
}

TEST_F(LocalStorageLevelDBTest, TryWriteAccessMetadata) {
  std::optional<DomStorageDatabase::MapMetadata> map_metadata =
      TryParseWriteMetadata({
          LocalStorageLevelDB::CreateWriteMetaDataKey(kFakeUrlStorageKey),
          LocalStorageLevelDB::CreateWriteMetaDataValue(kMapLastModified,
                                                        kMapTotalSize),
      });

  ASSERT_NE(map_metadata, std::nullopt);

  const DomStorageDatabase::MapMetadata kExpectedMapMetadata{
      .map_locator{kLocalStorageSessionId, kFakeUrlStorageKey},
      .last_modified{kMapLastModified},
      .total_size{kMapTotalSize},
  };
  ExpectEqualsMapMetadata(*map_metadata, kExpectedMapMetadata);
}

TEST_F(LocalStorageLevelDBTest, ReadAllMetadataWithEmpty) {
  std::unique_ptr<LocalStorageLevelDB> local_storage_leveldb;
  ASSERT_NO_FATAL_FAILURE(OpenInMemory(&local_storage_leveldb));

  StatusOr<DomStorageDatabase::Metadata> all_metadata =
      local_storage_leveldb->ReadAllMetadata();
  ASSERT_TRUE(all_metadata.has_value()) << all_metadata.error().ToString();

  EXPECT_EQ(all_metadata->map_metadata.size(), 0u);
  EXPECT_EQ(all_metadata->next_map_id, std::nullopt);
}

TEST_F(LocalStorageLevelDBTest, ReadAllMetadataWithInvalid) {
  std::unique_ptr<LocalStorageLevelDB> local_storage_leveldb;
  ASSERT_NO_FATAL_FAILURE(OpenInMemory(&local_storage_leveldb));

  ASSERT_NO_FATAL_FAILURE(
      WriteEntries(*local_storage_leveldb, {
                                               {
                                                   ToBytes("META:fake_key"),
                                                   ToBytes("fake_value"),
                                               },
                                           }));

  StatusOr<DomStorageDatabase::Metadata> all_metadata =
      local_storage_leveldb->ReadAllMetadata();
  ASSERT_TRUE(all_metadata.has_value()) << all_metadata.error().ToString();

  EXPECT_EQ(all_metadata->map_metadata.size(), 0u);
  EXPECT_EQ(all_metadata->next_map_id, std::nullopt);
}

TEST_F(LocalStorageLevelDBTest, ReadAllMetadataWithAccessMetadata) {
  std::unique_ptr<LocalStorageLevelDB> local_storage_leveldb;
  ASSERT_NO_FATAL_FAILURE(OpenInMemory(&local_storage_leveldb));

  ASSERT_NO_FATAL_FAILURE(WriteEntries(
      *local_storage_leveldb,
      {
          {
              LocalStorageLevelDB::CreateAccessMetaDataKey(kFakeUrlStorageKey),
              LocalStorageLevelDB::CreateAccessMetaDataValue(kMapLastAccessed),
          },
      }));

  StatusOr<DomStorageDatabase::Metadata> all_metadata =
      local_storage_leveldb->ReadAllMetadata();
  ASSERT_TRUE(all_metadata.has_value()) << all_metadata.error().ToString();

  const DomStorageDatabase::MapMetadata kExpectedMapMetadata[] = {
      {
          .map_locator{kLocalStorageSessionId, kFakeUrlStorageKey},
          .last_accessed{kMapLastAccessed},
      },
  };
  ExpectEqualsMapMetadataSpan(all_metadata->map_metadata, kExpectedMapMetadata);
  EXPECT_EQ(all_metadata->next_map_id, std::nullopt);
}

TEST_F(LocalStorageLevelDBTest, ReadAllMetadataWithWriteMetadata) {
  std::unique_ptr<LocalStorageLevelDB> local_storage_leveldb;
  ASSERT_NO_FATAL_FAILURE(OpenInMemory(&local_storage_leveldb));

  ASSERT_NO_FATAL_FAILURE(WriteEntries(
      *local_storage_leveldb,
      {
          {
              LocalStorageLevelDB::CreateWriteMetaDataKey(kFakeUrlStorageKey),
              LocalStorageLevelDB::CreateWriteMetaDataValue(kMapLastModified,
                                                            kMapTotalSize),
          },
      }));

  StatusOr<DomStorageDatabase::Metadata> all_metadata =
      local_storage_leveldb->ReadAllMetadata();
  ASSERT_TRUE(all_metadata.has_value()) << all_metadata.error().ToString();

  const DomStorageDatabase::MapMetadata kExpectedMapMetadata[]{
      {
          .map_locator{kLocalStorageSessionId, kFakeUrlStorageKey},
          .last_modified{kMapLastModified},
          .total_size{kMapTotalSize},
      },
  };
  ExpectEqualsMapMetadataSpan(all_metadata->map_metadata, kExpectedMapMetadata);
  EXPECT_EQ(all_metadata->next_map_id, std::nullopt);
}

TEST_F(LocalStorageLevelDBTest, ReadAllMetadataWithWriteAndAccessMetadata) {
  std::unique_ptr<LocalStorageLevelDB> local_storage_leveldb;
  ASSERT_NO_FATAL_FAILURE(OpenInMemory(&local_storage_leveldb));

  ASSERT_NO_FATAL_FAILURE(WriteEntries(
      *local_storage_leveldb,
      {
          {
              LocalStorageLevelDB::CreateWriteMetaDataKey(kFakeUrlStorageKey),
              LocalStorageLevelDB::CreateWriteMetaDataValue(kMapLastModified,
                                                            kMapTotalSize),
          },
          {
              LocalStorageLevelDB::CreateAccessMetaDataKey(kFakeUrlStorageKey),
              LocalStorageLevelDB::CreateAccessMetaDataValue(kMapLastAccessed),
          },
      }));

  StatusOr<DomStorageDatabase::Metadata> all_metadata =
      local_storage_leveldb->ReadAllMetadata();
  ASSERT_TRUE(all_metadata.has_value()) << all_metadata.error().ToString();

  const DomStorageDatabase::MapMetadata kExpectedMapMetadata[] = {
      {
          .map_locator{kLocalStorageSessionId, kFakeUrlStorageKey},
          .last_accessed{kMapLastAccessed},
          .last_modified{kMapLastModified},
          .total_size{kMapTotalSize},
      },
  };
  ExpectEqualsMapMetadataSpan(all_metadata->map_metadata, kExpectedMapMetadata);
  EXPECT_EQ(all_metadata->next_map_id, std::nullopt);
}

// Combine all previous tests into a single test using four storage
// keys for four different maps.
TEST_F(LocalStorageLevelDBTest, ReadAllMetadataWithMultipleStorageKeys) {
  const blink::StorageKey kFirstStorageKey =
      blink::StorageKey::CreateFromStringForTesting("https://a-fake.test");

  const blink::StorageKey kSecondStorageKey =
      blink::StorageKey::CreateFromStringForTesting("https://b-fake.test");
  const base::Time kSecondLastAccessed = base::Time::Now() - base::Hours(10);
  const base::Time kSecondLastModified = base::Time::Now() - base::Hours(1);
  const base::ByteSize kSecondTotalSize{102454};

  const blink::StorageKey kThirdStorageKey =
      blink::StorageKey::CreateFromStringForTesting("https://c-fake.test");
  const base::Time kThirdLastModified = base::Time::Now() - base::Minutes(23);
  const base::ByteSize kThirdTotalSize{50121524};

  const blink::StorageKey kFourthStorageKey =
      blink::StorageKey::CreateFromStringForTesting("https://d-fake.test");
  const base::Time kFourthLastAccessed = base::Time::Now() - base::Days(10);

  std::unique_ptr<LocalStorageLevelDB> local_storage_leveldb;
  ASSERT_NO_FATAL_FAILURE(OpenInMemory(&local_storage_leveldb));

  ASSERT_NO_FATAL_FAILURE(WriteEntries(
      *local_storage_leveldb,
      {
          {
              LocalStorageLevelDB::CreateWriteMetaDataKey(kThirdStorageKey),
              LocalStorageLevelDB::CreateWriteMetaDataValue(kThirdLastModified,
                                                            kThirdTotalSize),
          },
          {
              LocalStorageLevelDB::CreateWriteMetaDataKey(kSecondStorageKey),
              LocalStorageLevelDB::CreateWriteMetaDataValue(kSecondLastModified,
                                                            kSecondTotalSize),
          },
          {
              LocalStorageLevelDB::CreateAccessMetaDataKey(kFirstStorageKey),
              LocalStorageLevelDB::CreateAccessMetaDataValue(kMapLastAccessed),
          },
          // Invalid entry.
          {
              ToBytes("METAACCESS:fake_key"),
              ToBytes("fake_value"),
          },
          {
              LocalStorageLevelDB::CreateAccessMetaDataKey(kFourthStorageKey),
              LocalStorageLevelDB::CreateAccessMetaDataValue(
                  kFourthLastAccessed),
          },
          {
              LocalStorageLevelDB::CreateAccessMetaDataKey(kSecondStorageKey),
              LocalStorageLevelDB::CreateAccessMetaDataValue(
                  kSecondLastAccessed),
          },
          {
              LocalStorageLevelDB::CreateWriteMetaDataKey(kFirstStorageKey),
              LocalStorageLevelDB::CreateWriteMetaDataValue(kMapLastModified,
                                                            kMapTotalSize),
          },
      }));

  StatusOr<DomStorageDatabase::Metadata> all_metadata =
      local_storage_leveldb->ReadAllMetadata();
  ASSERT_TRUE(all_metadata.has_value()) << all_metadata.error().ToString();

  const DomStorageDatabase::MapMetadata kExpectedAllMapMetadata[] = {
      {
          .map_locator{kLocalStorageSessionId, kFirstStorageKey},
          .last_accessed{kMapLastAccessed},
          .last_modified{kMapLastModified},
          .total_size{kMapTotalSize},
      },
      {
          .map_locator{kLocalStorageSessionId, kSecondStorageKey},
          .last_accessed{kSecondLastAccessed},
          .last_modified{kSecondLastModified},
          .total_size{kSecondTotalSize},
      },
      {
          .map_locator{kLocalStorageSessionId, kThirdStorageKey},
          .last_modified{kThirdLastModified},
          .total_size{kThirdTotalSize},
      },
      {
          .map_locator{kLocalStorageSessionId, kFourthStorageKey},
          .last_accessed{kFourthLastAccessed},
      },
  };
  ExpectEqualsMapMetadataSpan(all_metadata->map_metadata,
                              kExpectedAllMapMetadata);
  EXPECT_EQ(all_metadata->next_map_id, std::nullopt);
}

}  // namespace storage
