// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/reading_list/core/reading_list_model_storage_impl.h"

#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/gmock_expected_support.h"
#include "base/test/simple_test_clock.h"
#include "base/test/task_environment.h"
#include "components/reading_list/core/reading_list_entry.h"
#include "components/sync/test/data_type_store_test_util.h"
#include "components/sync/test/test_matchers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {

using testing::_;
using testing::ElementsAre;
using testing::IsEmpty;
using testing::UnorderedElementsAre;

MATCHER_P(EntryHasUrl, expected_url, "") {
  return arg.second->URL() == expected_url;
}

// Helper function to load a storage and wait until loading completes.
ReadingListModelStorage::LoadResultOrError LoadStorageAndWait(
    ReadingListModelStorage* storage,
    base::Clock* clock) {
  base::RunLoop loop;
  ReadingListModelStorage::LoadResultOrError load_result_or_error;
  storage->Load(
      clock,
      base::BindLambdaForTesting(
          [&](ReadingListModelStorage::LoadResultOrError result_or_error) {
            load_result_or_error = std::move(result_or_error);
            loop.Quit();
          }));
  loop.Run();
  return load_result_or_error;
}

class ReadingListModelStorageImplTest : public testing::Test {
 protected:
  ReadingListModelStorageImplTest()
      : in_memory_store_(
            syncer::DataTypeStoreTestUtil::CreateInMemoryStoreForTest()),
        shared_store_factory_(
            syncer::DataTypeStoreTestUtil::FactoryForForwardingStore(
                in_memory_store_.get())) {}
  ~ReadingListModelStorageImplTest() override = default;

  base::test::SingleThreadTaskEnvironment task_environment_;
  base::SimpleTestClock clock_;
  const std::unique_ptr<syncer::DataTypeStore> in_memory_store_;
  const syncer::RepeatingDataTypeStoreFactory shared_store_factory_;
};

TEST_F(ReadingListModelStorageImplTest, LoadEmpty) {
  ReadingListModelStorageImpl storage(shared_store_factory_);

  ASSERT_OK_AND_ASSIGN(const ReadingListModelStorage::LoadResult result,
                       LoadStorageAndWait(&storage, &clock_));
  EXPECT_TRUE(result.first.empty());
  EXPECT_THAT(result.second, syncer::IsEmptyMetadataBatch());
}

TEST_F(ReadingListModelStorageImplTest, SaveEntry) {
  ReadingListModelStorageImpl storage(shared_store_factory_);

  ASSERT_TRUE(LoadStorageAndWait(&storage, &clock_).has_value());

  storage.EnsureBatchCreated()->SaveEntry(
      *base::MakeRefCounted<ReadingListEntry>(GURL("http://example.com/"),
                                              "Title", clock_.Now()));

  // To verify the write, use another storage with the same underlying in-memory
  // leveldb.
  ReadingListModelStorageImpl other_storage(shared_store_factory_);

  ASSERT_OK_AND_ASSIGN(const ReadingListModelStorage::LoadResult load_result,
                       LoadStorageAndWait(&other_storage, &clock_));
  EXPECT_THAT(load_result.first,
              ElementsAre(EntryHasUrl(GURL("http://example.com/"))));
}

TEST_F(ReadingListModelStorageImplTest, RemoveEntry) {
  ReadingListModelStorageImpl storage(shared_store_factory_);
  ASSERT_TRUE(LoadStorageAndWait(&storage, &clock_).has_value());
  storage.EnsureBatchCreated()->SaveEntry(
      *base::MakeRefCounted<ReadingListEntry>(GURL("http://example1.com/"),
                                              "Title 1", clock_.Now()));
  storage.EnsureBatchCreated()->SaveEntry(
      *base::MakeRefCounted<ReadingListEntry>(GURL("http://example2.com/"),
                                              "Title 2", clock_.Now()));

  // There should be two entries in storage.
  ReadingListModelStorageImpl second_storage(shared_store_factory_);
  ASSERT_THAT(LoadStorageAndWait(&second_storage, &clock_)->first,
              UnorderedElementsAre(EntryHasUrl(GURL("http://example1.com/")),
                                   EntryHasUrl(GURL("http://example2.com/"))));

  // Remove one of the two entries.
  second_storage.EnsureBatchCreated()->RemoveEntry(
      GURL("http://example1.com/"));

  // To verify the deletion, use a third storage with the same underlying
  // in-memory leveldb.
  ReadingListModelStorageImpl third_storage(shared_store_factory_);
  EXPECT_THAT(LoadStorageAndWait(&third_storage, &clock_)->first,
              ElementsAre(EntryHasUrl(GURL("http://example2.com/"))));
}

TEST_F(ReadingListModelStorageImplTest, DeleteAllEntriesAndSyncMetadata) {
  ReadingListModelStorageImpl storage(shared_store_factory_);
  ASSERT_TRUE(LoadStorageAndWait(&storage, &clock_).has_value());
  storage.EnsureBatchCreated()->SaveEntry(
      *base::MakeRefCounted<ReadingListEntry>(GURL("http://example1.com/"),
                                              "Title 1", clock_.Now()));
  storage.EnsureBatchCreated()->SaveEntry(
      *base::MakeRefCounted<ReadingListEntry>(GURL("http://example2.com/"),
                                              "Title 2", clock_.Now()));

  // There should be two entries in storage.
  ReadingListModelStorageImpl second_storage(shared_store_factory_);
  ASSERT_THAT(LoadStorageAndWait(&second_storage, &clock_)->first,
              UnorderedElementsAre(EntryHasUrl(GURL("http://example1.com/")),
                                   EntryHasUrl(GURL("http://example2.com/"))));

  // Delete everything.
  second_storage.DeleteAllEntriesAndSyncMetadata();

  // To verify the deletion, use a third storage with the same underlying
  // in-memory leveldb.
  ReadingListModelStorageImpl third_storage(shared_store_factory_);
  EXPECT_THAT(LoadStorageAndWait(&third_storage, &clock_)->first, IsEmpty());
}

}  // namespace
