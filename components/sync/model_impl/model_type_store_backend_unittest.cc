// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/model_impl/model_type_store_backend.h"

#include <utility>

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/metrics/histogram_tester.h"
#include "components/sync/protocol/model_type_store_schema_descriptor.pb.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/leveldatabase/env_chromium.h"
#include "third_party/leveldatabase/src/include/leveldb/db.h"
#include "third_party/leveldatabase/src/include/leveldb/env.h"
#include "third_party/leveldatabase/src/include/leveldb/options.h"
#include "third_party/leveldatabase/src/include/leveldb/status.h"
#include "third_party/leveldatabase/src/include/leveldb/write_batch.h"

using sync_pb::ModelTypeStoreSchemaDescriptor;

namespace syncer {
namespace {

// Test that after record is written to backend it can be read back even after
// backend is destroyed and recreated in the same environment.
TEST(ModelTypeStoreBackendTest, WriteThenRead) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  scoped_refptr<ModelTypeStoreBackend> backend =
      ModelTypeStoreBackend::CreateUninitialized();
  base::Optional<ModelError> error = backend->Init(temp_dir.GetPath());
  ASSERT_FALSE(error) << error->ToString();

  // Write record.
  std::unique_ptr<leveldb::WriteBatch> write_batch(new leveldb::WriteBatch());
  write_batch->Put("prefix:id1", "data1");
  error = backend->WriteModifications(std::move(write_batch));
  ASSERT_FALSE(error) << error->ToString();

  // Read all records with prefix.
  ModelTypeStore::RecordList record_list;
  error = backend->ReadAllRecordsWithPrefix("prefix:", &record_list);
  ASSERT_FALSE(error) << error->ToString();
  ASSERT_EQ(1ul, record_list.size());
  ASSERT_EQ("id1", record_list[0].id);
  ASSERT_EQ("data1", record_list[0].value);
  record_list.clear();

  // Recreate backend and read all records with prefix.
  backend = nullptr;
  backend = ModelTypeStoreBackend::CreateUninitialized();
  error = backend->Init(temp_dir.GetPath());
  ASSERT_FALSE(error) << error->ToString();

  error = backend->ReadAllRecordsWithPrefix("prefix:", &record_list);
  ASSERT_FALSE(error) << error->ToString();
  ASSERT_EQ(1ul, record_list.size());
  ASSERT_EQ("id1", record_list[0].id);
  ASSERT_EQ("data1", record_list[0].value);
}

// Test that ReadAllRecordsWithPrefix correclty filters records by prefix.
TEST(ModelTypeStoreBackendTest, ReadAllRecordsWithPrefix) {
  scoped_refptr<ModelTypeStoreBackend> backend =
      ModelTypeStoreBackend::CreateInMemoryForTest();

  std::unique_ptr<leveldb::WriteBatch> write_batch(new leveldb::WriteBatch());
  write_batch->Put("prefix1:id1", "data1");
  write_batch->Put("prefix2:id2", "data2");
  base::Optional<ModelError> error =
      backend->WriteModifications(std::move(write_batch));
  ASSERT_FALSE(error) << error->ToString();

  ModelTypeStore::RecordList record_list;
  error = backend->ReadAllRecordsWithPrefix("prefix1:", &record_list);
  ASSERT_FALSE(error) << error->ToString();
  ASSERT_EQ(1UL, record_list.size());
  ASSERT_EQ("id1", record_list[0].id);
  ASSERT_EQ("data1", record_list[0].value);
}

// Test that deleted records are correctly marked as milling in results of
// ReadRecordsWithPrefix.
TEST(ModelTypeStoreBackendTest, ReadDeletedRecord) {
  scoped_refptr<ModelTypeStoreBackend> backend =
      ModelTypeStoreBackend::CreateInMemoryForTest();

  // Create records, ensure they are returned by ReadRecordsWithPrefix.
  std::unique_ptr<leveldb::WriteBatch> write_batch(new leveldb::WriteBatch());
  write_batch->Put("prefix:id1", "data1");
  write_batch->Put("prefix:id2", "data2");
  base::Optional<ModelError> error =
      backend->WriteModifications(std::move(write_batch));
  ASSERT_FALSE(error) << error->ToString();

  ModelTypeStore::IdList id_list;
  ModelTypeStore::IdList missing_id_list;
  ModelTypeStore::RecordList record_list;
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
TEST(ModelTypeStoreBackendTest, DeleteDataAndMetadataForPrefix) {
  scoped_refptr<ModelTypeStoreBackend> backend =
      ModelTypeStoreBackend::CreateInMemoryForTest();

  auto write_batch = std::make_unique<leveldb::WriteBatch>();
  write_batch->Put("prefix1:id1", "data1");
  write_batch->Put("prefix2:id2", "data2");
  write_batch->Put("prefix2:id3", "data3");
  write_batch->Put("prefix3:id4", "data4");
  base::Optional<ModelError> error =
      backend->WriteModifications(std::move(write_batch));
  ASSERT_FALSE(error) << error->ToString();

  error = backend->DeleteDataAndMetadataForPrefix("prefix2:");
  EXPECT_FALSE(error) << error->ToString();

  {
    ModelTypeStore::RecordList record_list;
    error = backend->ReadAllRecordsWithPrefix("prefix2:", &record_list);
    EXPECT_FALSE(error) << error->ToString();
    EXPECT_EQ(0UL, record_list.size());
  }

  {
    ModelTypeStore::RecordList record_list;
    error = backend->ReadAllRecordsWithPrefix("prefix1:", &record_list);
    EXPECT_FALSE(error) << error->ToString();
    EXPECT_EQ(1UL, record_list.size());
  }

  {
    ModelTypeStore::RecordList record_list;
    error = backend->ReadAllRecordsWithPrefix("prefix3:", &record_list);
    EXPECT_FALSE(error) << error->ToString();
    EXPECT_EQ(1UL, record_list.size());
  }
}

// Test that initializing the database migrates it to the latest schema version.
TEST(ModelTypeStoreBackendTest, MigrateNoSchemaVersionToLatestVersionTest) {
  scoped_refptr<ModelTypeStoreBackend> backend =
      ModelTypeStoreBackend::CreateInMemoryForTest();

  ASSERT_EQ(ModelTypeStoreBackend::kLatestSchemaVersion,
            backend->GetStoreVersionForTest());
}

// Test that the 0 to 1 migration succeeds and sets the schema version to 1.
TEST(ModelTypeStoreBackendTest, Migrate0To1Test) {
  scoped_refptr<ModelTypeStoreBackend> backend =
      ModelTypeStoreBackend::CreateInMemoryForTest();

  std::unique_ptr<leveldb::WriteBatch> write_batch(new leveldb::WriteBatch());
  write_batch->Delete(ModelTypeStoreBackend::kDBSchemaDescriptorRecordId);
  base::Optional<ModelError> error =
      backend->WriteModifications(std::move(write_batch));
  ASSERT_FALSE(error) << error->ToString();
  ASSERT_EQ(0, backend->GetStoreVersionForTest());

  error = backend->MigrateForTest(0, 1);
  EXPECT_FALSE(error) << error->ToString();
  EXPECT_EQ(1, backend->GetStoreVersionForTest());
}

// Test that migration to an unknown version fails
TEST(ModelTypeStoreBackendTest, MigrateWithHigherExistingVersionFails) {
  scoped_refptr<ModelTypeStoreBackend> backend =
      ModelTypeStoreBackend::CreateInMemoryForTest();

  base::Optional<ModelError> error =
      backend->MigrateForTest(ModelTypeStoreBackend::kLatestSchemaVersion + 1,
                              ModelTypeStoreBackend::kLatestSchemaVersion);
  ASSERT_TRUE(error);
  EXPECT_EQ("Schema version too high", error->message());
}

// Tests that initializing store after corruption triggers recovery and results
// in successful store initialization.
TEST(ModelTypeStoreBackendTest, RecoverAfterCorruption) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  base::HistogramTester tester;
  leveldb::Status s;

  // Prepare environment that looks corrupt to leveldb.
  // Easiest way to simulate leveldb corruption is to create empty CURRENT file.
  base::WriteFile(temp_dir.GetPath().Append(FILE_PATH_LITERAL("CURRENT")), "",
                  0);

  scoped_refptr<ModelTypeStoreBackend> backend =
      ModelTypeStoreBackend::CreateUninitialized();
  base::Optional<ModelError> error = backend->Init(temp_dir.GetPath());
  ASSERT_FALSE(error) << error->ToString();

  // Check that both recovery and consecutive initialization are recorded in
  // histograms.
  tester.ExpectBucketCount(ModelTypeStoreBackend::kStoreInitResultHistogramName,
                           STORE_INIT_RESULT_SUCCESS, 1);
  tester.ExpectBucketCount(ModelTypeStoreBackend::kStoreInitResultHistogramName,
                           STORE_INIT_RESULT_RECOVERED_AFTER_CORRUPTION, 1);
}

}  // namespace
}  // namespace syncer
