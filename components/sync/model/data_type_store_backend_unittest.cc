// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/model/data_type_store_backend.h"

#include <utility>

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/task_environment.h"
#include "components/sync/protocol/data_type_store_schema_descriptor.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/leveldatabase/env_chromium.h"
#include "third_party/leveldatabase/src/include/leveldb/db.h"
#include "third_party/leveldatabase/src/include/leveldb/env.h"
#include "third_party/leveldatabase/src/include/leveldb/options.h"
#include "third_party/leveldatabase/src/include/leveldb/status.h"
#include "third_party/leveldatabase/src/include/leveldb/write_batch.h"

using testing::IsEmpty;
using testing::UnorderedElementsAre;

namespace syncer {
namespace {

class DataTypeStoreBackendTest : public testing::Test {
 protected:
  DataTypeStoreBackendTest() = default;
  ~DataTypeStoreBackendTest() override = default;

  // Required for task-posting during destruction.
  base::test::SingleThreadTaskEnvironment task_environment_;
};

// Test that after record is written to backend it can be read back even after
// backend is destroyed and recreated in the same environment.
TEST_F(DataTypeStoreBackendTest, WriteThenRead) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  scoped_refptr<DataTypeStoreBackend> backend =
      DataTypeStoreBackend::CreateUninitialized();
  std::optional<ModelError> error = backend->Init(temp_dir.GetPath(), {});
  ASSERT_FALSE(error) << error->ToString();

  // Write record.
  std::unique_ptr<leveldb::WriteBatch> write_batch(new leveldb::WriteBatch());
  write_batch->Put("prefix:id1", "data1");
  error = backend->WriteModifications(std::move(write_batch));
  ASSERT_FALSE(error) << error->ToString();

  // Read all records with prefix.
  DataTypeStore::RecordList record_list;
  error = backend->ReadAllRecordsWithPrefix("prefix:", &record_list);
  ASSERT_FALSE(error) << error->ToString();
  ASSERT_EQ(1ul, record_list.size());
  ASSERT_EQ("id1", record_list[0].id);
  ASSERT_EQ("data1", record_list[0].value);
  record_list.clear();

  // Recreate backend and read all records with prefix.
  backend = nullptr;
  backend = DataTypeStoreBackend::CreateUninitialized();
  error = backend->Init(temp_dir.GetPath(), {});
  ASSERT_FALSE(error) << error->ToString();

  error = backend->ReadAllRecordsWithPrefix("prefix:", &record_list);
  ASSERT_FALSE(error) << error->ToString();
  ASSERT_EQ(1ul, record_list.size());
  ASSERT_EQ("id1", record_list[0].id);
  ASSERT_EQ("data1", record_list[0].value);
}

// Test that ReadAllRecordsWithPrefix correclty filters records by prefix.
TEST_F(DataTypeStoreBackendTest, ReadAllRecordsWithPrefix) {
  scoped_refptr<DataTypeStoreBackend> backend =
      DataTypeStoreBackend::CreateInMemoryForTest();

  std::unique_ptr<leveldb::WriteBatch> write_batch(new leveldb::WriteBatch());
  write_batch->Put("prefix1:id1", "data1");
  write_batch->Put("prefix2:id2", "data2");
  std::optional<ModelError> error =
      backend->WriteModifications(std::move(write_batch));
  ASSERT_FALSE(error) << error->ToString();

  DataTypeStore::RecordList record_list;
  error = backend->ReadAllRecordsWithPrefix("prefix1:", &record_list);
  ASSERT_FALSE(error) << error->ToString();
  ASSERT_EQ(1UL, record_list.size());
  ASSERT_EQ("id1", record_list[0].id);
  ASSERT_EQ("data1", record_list[0].value);
}

// Test that deleted records are correctly marked as milling in results of
// ReadRecordsWithPrefix.
TEST_F(DataTypeStoreBackendTest, ReadDeletedRecord) {
  scoped_refptr<DataTypeStoreBackend> backend =
      DataTypeStoreBackend::CreateInMemoryForTest();

  // Create records, ensure they are returned by ReadRecordsWithPrefix.
  std::unique_ptr<leveldb::WriteBatch> write_batch(new leveldb::WriteBatch());
  write_batch->Put("prefix:id1", "data1");
  write_batch->Put("prefix:id2", "data2");
  std::optional<ModelError> error =
      backend->WriteModifications(std::move(write_batch));
  ASSERT_FALSE(error) << error->ToString();

  DataTypeStore::IdList id_list;
  DataTypeStore::IdList missing_id_list;
  DataTypeStore::RecordList record_list;
  id_list.push_back("id1");
  id_list.push_back("id2");
  error = backend->ReadRecordsWithPrefix("prefix:", id_list, &record_list,
                                         &missing_id_list);
  ASSERT_FALSE(error) << error->ToString();
  ASSERT_EQ(2UL, record_list.size());
  ASSERT_TRUE(missing_id_list.empty());

  // Delete one record.
  write_batch = std::make_unique<leveldb::WriteBatch>();
  write_batch->Delete("prefix:id2");
  error = backend->WriteModifications(std::move(write_batch));
  ASSERT_FALSE(error) << error->ToString();

  // Ensure deleted record id is returned in missing_id_list.
  record_list.clear();
  missing_id_list.clear();
  error = backend->ReadRecordsWithPrefix("prefix:", id_list, &record_list,
                                         &missing_id_list);
  ASSERT_FALSE(error) << error->ToString();
  ASSERT_EQ(1UL, record_list.size());
  ASSERT_EQ("id1", record_list[0].id);
  ASSERT_EQ(1UL, missing_id_list.size());
  ASSERT_EQ("id2", missing_id_list[0]);
}

// Test that DeleteDataAndMetadataForPrefix correctly deletes records by prefix.
TEST_F(DataTypeStoreBackendTest, DeleteDataAndMetadataForPrefix) {
  scoped_refptr<DataTypeStoreBackend> backend =
      DataTypeStoreBackend::CreateInMemoryForTest();

  auto write_batch = std::make_unique<leveldb::WriteBatch>();
  write_batch->Put("prefix1:id1", "data1");
  write_batch->Put("prefix2:id2", "data2");
  write_batch->Put("prefix2:id3", "data3");
  write_batch->Put("prefix3:id4", "data4");
  std::optional<ModelError> error =
      backend->WriteModifications(std::move(write_batch));
  ASSERT_FALSE(error) << error->ToString();

  error = backend->DeleteDataAndMetadataForPrefix("prefix2:");
  EXPECT_FALSE(error) << error->ToString();

  {
    DataTypeStore::RecordList record_list;
    error = backend->ReadAllRecordsWithPrefix("prefix2:", &record_list);
    EXPECT_FALSE(error) << error->ToString();
    EXPECT_EQ(0UL, record_list.size());
  }

  {
    DataTypeStore::RecordList record_list;
    error = backend->ReadAllRecordsWithPrefix("prefix1:", &record_list);
    EXPECT_FALSE(error) << error->ToString();
    EXPECT_EQ(1UL, record_list.size());
  }

  {
    DataTypeStore::RecordList record_list;
    error = backend->ReadAllRecordsWithPrefix("prefix3:", &record_list);
    EXPECT_FALSE(error) << error->ToString();
    EXPECT_EQ(1UL, record_list.size());
  }
}

// Test that initializing the database migrates it to the latest schema version.
TEST_F(DataTypeStoreBackendTest, MigrateNoSchemaVersionToLatestVersionTest) {
  scoped_refptr<DataTypeStoreBackend> backend =
      DataTypeStoreBackend::CreateInMemoryForTest();

  ASSERT_EQ(DataTypeStoreBackend::kLatestSchemaVersion,
            backend->GetStoreVersionForTest());
}

// Test that the 0 to 1 migration succeeds and sets the schema version to 1.
TEST_F(DataTypeStoreBackendTest, Migrate0To1Test) {
  scoped_refptr<DataTypeStoreBackend> backend =
      DataTypeStoreBackend::CreateInMemoryForTest();

  std::unique_ptr<leveldb::WriteBatch> write_batch(new leveldb::WriteBatch());
  write_batch->Delete(DataTypeStoreBackend::kDBSchemaDescriptorRecordId);
  std::optional<ModelError> error =
      backend->WriteModifications(std::move(write_batch));
  ASSERT_FALSE(error) << error->ToString();
  ASSERT_EQ(0, backend->GetStoreVersionForTest());

  error = backend->MigrateForTest(0, 1);
  EXPECT_FALSE(error) << error->ToString();
  EXPECT_EQ(1, backend->GetStoreVersionForTest());
}

// Test that migration to an unknown version fails
TEST_F(DataTypeStoreBackendTest, MigrateWithHigherExistingVersionFails) {
  scoped_refptr<DataTypeStoreBackend> backend =
      DataTypeStoreBackend::CreateInMemoryForTest();

  std::optional<ModelError> error =
      backend->MigrateForTest(DataTypeStoreBackend::kLatestSchemaVersion + 1,
                              DataTypeStoreBackend::kLatestSchemaVersion);
  ASSERT_TRUE(error);
  EXPECT_EQ("Schema version too high", error->message());
}

TEST_F(DataTypeStoreBackendTest, MigrateReadingListFromLocalToAccount) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  std::optional<ModelError> error;

  // Setup: Put some data into the persisted storage.
  {
    scoped_refptr<DataTypeStoreBackend> backend =
        DataTypeStoreBackend::CreateUninitialized();
    error = backend->Init(temp_dir.GetPath(),
                          /*prefixes_to_update=*/{});
    ASSERT_FALSE(error) << error->ToString();

    auto write_batch = std::make_unique<leveldb::WriteBatch>();
    // ReadingList data in the local store. This should be migrated.
    write_batch->Put("reading_list-dt-id1", "rl_data1");
    write_batch->Put("reading_list-md-id1", "rl_metadata1");
    write_batch->Put("reading_list-GlobalMetadata", "rl_global_metadata");
    // Some other types' data in the local and the account store. These should
    // not be touched.
    write_batch->Put("bookmarks-dt-id1", "bm_data1");
    write_batch->Put("bookmarks-md-id1", "bm_metadata1");
    write_batch->Put("bookmarks-GlobalMetadata", "bm_global_metadata");
    write_batch->Put("A-passwords-dt-id1", "pw_data1");
    write_batch->Put("A-passwords-md-id1", "pw_metadata1");
    write_batch->Put("A-passwords-GlobalMetadata", "pw_global_metadata");
    error = backend->WriteModifications(std::move(write_batch));
    ASSERT_FALSE(error) << error->ToString();
  }

  // Recreate the backend and trigger the ReadingList migration.
  scoped_refptr<DataTypeStoreBackend> backend =
      DataTypeStoreBackend::CreateUninitialized();
  error = backend->Init(
      temp_dir.GetPath(),
      /*prefixes_to_update=*/{{"reading_list", "A-reading_list"}});
  ASSERT_FALSE(error) << error->ToString();

  // Local ReadingList data should be empty now.
  {
    DataTypeStore::RecordList record_list;
    error = backend->ReadAllRecordsWithPrefix("reading_list-", &record_list);
    ASSERT_FALSE(error) << error->ToString();
    EXPECT_TRUE(record_list.empty());
  }
  // Instead, the existing ReadingList data should be in the account store.
  {
    DataTypeStore::RecordList record_list;
    error = backend->ReadAllRecordsWithPrefix("A-reading_list-", &record_list);
    ASSERT_FALSE(error) << error->ToString();
    EXPECT_THAT(
        record_list,
        UnorderedElementsAre(
            DataTypeStore::Record{"dt-id1", "rl_data1"},
            DataTypeStore::Record{"md-id1", "rl_metadata1"},
            DataTypeStore::Record{"GlobalMetadata", "rl_global_metadata"}));
  }
  // Data from other types should be unaffected.
  {
    DataTypeStore::RecordList record_list;
    error = backend->ReadAllRecordsWithPrefix("bookmarks-", &record_list);
    ASSERT_FALSE(error) << error->ToString();
    EXPECT_THAT(
        record_list,
        UnorderedElementsAre(
            DataTypeStore::Record{"dt-id1", "bm_data1"},
            DataTypeStore::Record{"md-id1", "bm_metadata1"},
            DataTypeStore::Record{"GlobalMetadata", "bm_global_metadata"}));
  }
  {
    DataTypeStore::RecordList record_list;
    error = backend->ReadAllRecordsWithPrefix("A-passwords-", &record_list);
    ASSERT_FALSE(error) << error->ToString();
    EXPECT_THAT(
        record_list,
        UnorderedElementsAre(
            DataTypeStore::Record{"dt-id1", "pw_data1"},
            DataTypeStore::Record{"md-id1", "pw_metadata1"},
            DataTypeStore::Record{"GlobalMetadata", "pw_global_metadata"}));
  }
}

TEST_F(DataTypeStoreBackendTest,
       MigrateReadingListFromLocalToAccount_Idempotent) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  std::optional<ModelError> error;

  // Setup: Put some data into the persisted storage.
  {
    scoped_refptr<DataTypeStoreBackend> backend =
        DataTypeStoreBackend::CreateUninitialized();
    error = backend->Init(temp_dir.GetPath(),
                          /*prefixes_to_update=*/{});
    ASSERT_FALSE(error) << error->ToString();

    auto write_batch = std::make_unique<leveldb::WriteBatch>();
    // ReadingList data in the local store. This should be migrated.
    write_batch->Put("reading_list-dt-id1", "rl_data1");
    write_batch->Put("reading_list-md-id1", "rl_metadata1");
    write_batch->Put("reading_list-GlobalMetadata", "rl_global_metadata");
    error = backend->WriteModifications(std::move(write_batch));
    ASSERT_FALSE(error) << error->ToString();
  }

  // Recreate the backend and trigger the ReadingList migration for the first
  // time.
  {
    scoped_refptr<DataTypeStoreBackend> backend =
        DataTypeStoreBackend::CreateUninitialized();
    error = backend->Init(
        temp_dir.GetPath(),
        /*prefixes_to_update=*/{{"reading_list", "A-reading_list"}});
    ASSERT_FALSE(error) << error->ToString();
  }

  // Recreate the backend and trigger the ReadingList migration *again*. This
  // should have no further effect.
  scoped_refptr<DataTypeStoreBackend> backend =
      DataTypeStoreBackend::CreateUninitialized();
  error = backend->Init(
      temp_dir.GetPath(),
      /*prefixes_to_update=*/{{"reading_list", "A-reading_list"}});
  ASSERT_FALSE(error) << error->ToString();

  // Local ReadingList data should be empty now.
  {
    DataTypeStore::RecordList record_list;
    error = backend->ReadAllRecordsWithPrefix("reading_list-", &record_list);
    ASSERT_FALSE(error) << error->ToString();
    EXPECT_TRUE(record_list.empty());
  }
  // Instead, the existing ReadingList data should be in the account store.
  {
    DataTypeStore::RecordList record_list;
    error = backend->ReadAllRecordsWithPrefix("A-reading_list-", &record_list);
    ASSERT_FALSE(error) << error->ToString();
    EXPECT_THAT(
        record_list,
        UnorderedElementsAre(
            DataTypeStore::Record{"dt-id1", "rl_data1"},
            DataTypeStore::Record{"md-id1", "rl_metadata1"},
            DataTypeStore::Record{"GlobalMetadata", "rl_global_metadata"}));
  }
}

TEST_F(DataTypeStoreBackendTest, PrefixesToDelete) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  // Setup: Put some data into the persisted storage.
  {
    scoped_refptr<DataTypeStoreBackend> backend =
        DataTypeStoreBackend::CreateUninitialized();
    std::optional<ModelError> error =
        backend->Init(temp_dir.GetPath(),
                      /*prefixes_to_update_or_delete=*/{});
    ASSERT_FALSE(error) << error->ToString();

    auto write_batch = std::make_unique<leveldb::WriteBatch>();
    write_batch->Put("deleted-prefix-deleted-suffix", "deleted-value");
    write_batch->Put("kept-prefix-kept-suffix", "kept-value");
    error = backend->WriteModifications(std::move(write_batch));
    ASSERT_FALSE(error) << error->ToString();
  }

  // Recreate the backend and trigger the migration.
  {
    scoped_refptr<DataTypeStoreBackend> backend =
        DataTypeStoreBackend::CreateUninitialized();
    std::optional<ModelError> error = backend->Init(
        temp_dir.GetPath(),
        /*prefixes_to_update_or_delete=*/{{"deleted-prefix-", std::nullopt}});
    ASSERT_FALSE(error) << error->ToString();

    DataTypeStore::RecordList record_list;
    error = backend->ReadAllRecordsWithPrefix("deleted-prefix-", &record_list);
    ASSERT_FALSE(error) << error->ToString();
    EXPECT_THAT(record_list, IsEmpty());

    error = backend->ReadAllRecordsWithPrefix("kept-prefix-", &record_list);
    ASSERT_FALSE(error) << error->ToString();
    EXPECT_THAT(record_list, UnorderedElementsAre(DataTypeStore::Record{
                                 "kept-suffix", "kept-value"}));
  }
}

// Tests that initializing store after corruption triggers recovery and results
// in successful store initialization.
TEST_F(DataTypeStoreBackendTest, RecoverAfterCorruption) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  leveldb::Status s;

  // Prepare environment that looks corrupt to leveldb.
  // Easiest way to simulate leveldb corruption is to create empty CURRENT file.
  base::WriteFile(temp_dir.GetPath().Append(FILE_PATH_LITERAL("CURRENT")), "");

  scoped_refptr<DataTypeStoreBackend> backend =
      DataTypeStoreBackend::CreateUninitialized();
  std::optional<ModelError> error = backend->Init(temp_dir.GetPath(), {});
  ASSERT_FALSE(error) << error->ToString();
}

}  // namespace
}  // namespace syncer
