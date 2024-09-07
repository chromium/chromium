// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/bookmarks/browser/model_loader.h"

#include <memory>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "components/bookmarks/browser/bookmark_client.h"
#include "components/bookmarks/browser/bookmark_load_details.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace bookmarks {
namespace {

const char kLocalOrSyncableIdsReassignedMetricName[] =
    "Bookmarks.IdsReassigned.OnProfileLoad.LocalOrSyncable";
const char kAccountIdsReassignedMetricName[] =
    "Bookmarks.IdsReassigned.OnProfileLoad.Account";

const base::FilePath& GetTestDataDir() {
  static base::NoDestructor<base::FilePath> dir([]() {
    base::FilePath dir;
    base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &dir);
    return dir.AppendASCII("components")
        .AppendASCII("test")
        .AppendASCII("data");
  }());
  return *dir;
}

const BookmarkNode* FindNodeByUuid(const UuidIndex& index,
                                   const std::string& uuid_str) {
  const base::Uuid uuid = base::Uuid::ParseLowercase(uuid_str);
  CHECK(uuid.is_valid());
  const auto it = index.find(uuid);
  return it == index.end() ? nullptr : *it;
}

TEST(ModelLoaderTest, LoadEmptyModelFromInexistentFile) {
  base::HistogramTester histogram_tester;
  base::test::TaskEnvironment task_environment;
  const base::FilePath test_file =
      GetTestDataDir().AppendASCII("inexistent_bookmarks_file.json");
  ASSERT_FALSE(base::PathExists(test_file));

  base::test::TestFuture<std::unique_ptr<BookmarkLoadDetails>> details_future;
  scoped_refptr<ModelLoader> loader = ModelLoader::Create(
      /*local_or_syncable_file_path=*/test_file,
      /*account_file_path=*/base::FilePath(),
      /*load_managed_node_callback=*/LoadManagedNodeCallback(),
      details_future.GetCallback());

  const std::unique_ptr<BookmarkLoadDetails>& details = details_future.Get();

  ASSERT_NE(nullptr, details);
  ASSERT_NE(nullptr, details->bb_node());
  ASSERT_NE(nullptr, details->other_folder_node());
  ASSERT_NE(nullptr, details->mobile_folder_node());

  EXPECT_FALSE(details->required_recovery());
  EXPECT_FALSE(details->ids_reassigned());
  EXPECT_EQ(4, details->max_id());

  EXPECT_EQ(0u, details->bb_node()->children().size());
  EXPECT_EQ(0u, details->other_folder_node()->children().size());
  EXPECT_EQ(0u, details->mobile_folder_node()->children().size());

  EXPECT_EQ("", details->local_or_syncable_sync_metadata_str());

  // Permanent node ID's are subject to change, but expectations are listed
  // below for the purpose of documenting the current behavior.
  EXPECT_EQ(1u, details->bb_node()->id());
  EXPECT_EQ(2u, details->other_folder_node()->id());
  EXPECT_EQ(3u, details->mobile_folder_node()->id());

  histogram_tester.ExpectTotalCount(kLocalOrSyncableIdsReassignedMetricName,
                                    /*expected_count=*/0);
  histogram_tester.ExpectTotalCount(kAccountIdsReassignedMetricName,
                                    /*expected_count=*/0);
}

TEST(ModelLoaderTest, LoadNonEmptyModel) {
  base::HistogramTester histogram_tester;
  base::test::TaskEnvironment task_environment;
  const base::FilePath test_file =
      GetTestDataDir().AppendASCII("bookmarks/model_with_sync_metadata_1.json");
  ASSERT_TRUE(base::PathExists(test_file));

  base::test::TestFuture<std::unique_ptr<BookmarkLoadDetails>> details_future;
  scoped_refptr<ModelLoader> loader = ModelLoader::Create(
      /*local_or_syncable_file_path=*/test_file,
      /*account_file_path=*/base::FilePath(),
      /*load_managed_node_callback=*/LoadManagedNodeCallback(),
      details_future.GetCallback());

  const std::unique_ptr<BookmarkLoadDetails> details = details_future.Take();

  ASSERT_NE(nullptr, details);
  ASSERT_NE(nullptr, details->bb_node());
  ASSERT_NE(nullptr, details->other_folder_node());
  ASSERT_NE(nullptr, details->mobile_folder_node());

  EXPECT_FALSE(details->required_recovery());
  EXPECT_FALSE(details->ids_reassigned());
  EXPECT_EQ(11, details->max_id());

  EXPECT_EQ(1u, details->bb_node()->children().size());
  EXPECT_EQ(1u, details->other_folder_node()->children().size());
  EXPECT_EQ(1u, details->mobile_folder_node()->children().size());

  EXPECT_EQ("dummy-sync-metadata-1",
            details->local_or_syncable_sync_metadata_str());

  const UuidIndex uuid_index = details->owned_local_or_syncable_uuid_index();

  // Sanity-check the presence of one node.
  const BookmarkNode* folder_b1 =
      FindNodeByUuid(uuid_index, "da47f36f-050f-4ac9-aa35-ab0d93d39f95");
  ASSERT_NE(nullptr, folder_b1);
  EXPECT_EQ(u"Folder B1", folder_b1->GetTitle());
  EXPECT_EQ(4, folder_b1->id());

  histogram_tester.ExpectUniqueSample(kLocalOrSyncableIdsReassignedMetricName,
                                      /*sample=*/false,
                                      /*expected_bucket_count=*/1);
  histogram_tester.ExpectTotalCount(kAccountIdsReassignedMetricName,
                                    /*expected_count=*/0);
}

TEST(ModelLoaderTest, LoadNonEmptyModelFromOneFileWithInternalIdCollisions) {
  base::HistogramTester histogram_tester;
  base::test::TaskEnvironment task_environment;
  const base::FilePath test_file =
      GetTestDataDir().AppendASCII("bookmarks/model_with_duplicate_ids.json");
  ASSERT_TRUE(base::PathExists(test_file));

  base::test::TestFuture<std::unique_ptr<BookmarkLoadDetails>> details_future;
  scoped_refptr<ModelLoader> loader = ModelLoader::Create(
      /*local_or_syncable_file_path=*/test_file,
      /*account_file_path=*/base::FilePath(),
      /*load_managed_node_callback=*/LoadManagedNodeCallback(),
      details_future.GetCallback());

  const std::unique_ptr<BookmarkLoadDetails> details = details_future.Take();

  ASSERT_NE(nullptr, details);
  ASSERT_NE(nullptr, details->bb_node());
  ASSERT_NE(nullptr, details->other_folder_node());
  ASSERT_NE(nullptr, details->mobile_folder_node());

  EXPECT_TRUE(details->required_recovery());
  EXPECT_TRUE(details->ids_reassigned());
  EXPECT_EQ(10, details->max_id());

  EXPECT_EQ(1u, details->bb_node()->children().size());
  EXPECT_EQ(1u, details->other_folder_node()->children().size());
  EXPECT_EQ(1u, details->mobile_folder_node()->children().size());

  // Permanent node ID's are subject to change, but expectations are listed
  // below for the purpose of documenting the current behavior. Note that in
  // this case some permanent folders get non-standard IDs assigned.
  EXPECT_EQ(1u, details->bb_node()->id());
  EXPECT_EQ(4u, details->other_folder_node()->id());
  EXPECT_EQ(7u, details->mobile_folder_node()->id());

  EXPECT_EQ("dummy-sync-metadata-1",
            details->local_or_syncable_sync_metadata_str());

  const UuidIndex uuid_index = details->owned_local_or_syncable_uuid_index();

  // Sanity-check the presence of one node.
  const BookmarkNode* folder_b1 =
      FindNodeByUuid(uuid_index, "da47f36f-050f-4ac9-aa35-ab0d93d39f95");
  ASSERT_NE(nullptr, folder_b1);
  EXPECT_EQ(u"Folder B1", folder_b1->GetTitle());
  EXPECT_EQ(5, folder_b1->id());

  histogram_tester.ExpectUniqueSample(kLocalOrSyncableIdsReassignedMetricName,
                                      /*sample=*/true,
                                      /*expected_bucket_count=*/1);
  histogram_tester.ExpectTotalCount(kAccountIdsReassignedMetricName,
                                    /*expected_count=*/0);
}

TEST(ModelLoaderTest, LoadTwoFilesWithNonCollidingIds) {
  base::HistogramTester histogram_tester;
  base::test::TaskEnvironment task_environment;
  const base::FilePath test_file1 =
      GetTestDataDir().AppendASCII("bookmarks/model_with_sync_metadata_1.json");
  const base::FilePath test_file2 =
      GetTestDataDir().AppendASCII("bookmarks/model_with_sync_metadata_2.json");
  ASSERT_TRUE(base::PathExists(test_file1));
  ASSERT_TRUE(base::PathExists(test_file2));

  base::test::TestFuture<std::unique_ptr<BookmarkLoadDetails>> details_future;
  scoped_refptr<ModelLoader> loader = ModelLoader::Create(
      /*local_or_syncable_file_path=*/test_file1,
      /*account_file_path=*/test_file2,
      /*load_managed_node_callback=*/LoadManagedNodeCallback(),
      details_future.GetCallback());

  const std::unique_ptr<BookmarkLoadDetails> details = details_future.Take();

  ASSERT_NE(nullptr, details);
  ASSERT_NE(nullptr, details->bb_node());
  ASSERT_NE(nullptr, details->other_folder_node());
  ASSERT_NE(nullptr, details->mobile_folder_node());
  ASSERT_NE(nullptr, details->account_bb_node());
  ASSERT_NE(nullptr, details->account_other_folder_node());
  ASSERT_NE(nullptr, details->account_mobile_folder_node());

  EXPECT_FALSE(details->required_recovery());
  EXPECT_FALSE(details->ids_reassigned());

  EXPECT_EQ(24, details->max_id());

  EXPECT_EQ(1u, details->bb_node()->children().size());
  EXPECT_EQ(1u, details->other_folder_node()->children().size());
  EXPECT_EQ(1u, details->mobile_folder_node()->children().size());
  EXPECT_EQ(1u, details->account_bb_node()->children().size());
  EXPECT_EQ(1u, details->account_other_folder_node()->children().size());
  EXPECT_EQ(1u, details->account_mobile_folder_node()->children().size());

  EXPECT_EQ("dummy-sync-metadata-1",
            details->local_or_syncable_sync_metadata_str());
  EXPECT_EQ("dummy-sync-metadata-2", details->account_sync_metadata_str());

  const UuidIndex local_or_syncable_uuid_index =
      details->owned_local_or_syncable_uuid_index();
  const UuidIndex account_uuid_index = details->owned_account_uuid_index();

  // Sanity-check the presence of one node. The UUID should not have changed.
  const BookmarkNode* local_or_syncable_folder_b1 = FindNodeByUuid(
      local_or_syncable_uuid_index, "da47f36f-050f-4ac9-aa35-ab0d93d39f95");
  ASSERT_NE(nullptr, local_or_syncable_folder_b1);
  EXPECT_EQ(u"Folder B1", local_or_syncable_folder_b1->GetTitle());
  EXPECT_EQ(4, local_or_syncable_folder_b1->id());

  const BookmarkNode* account_folder_b1 = FindNodeByUuid(
      account_uuid_index, "da47f36f-050f-4ac9-aa35-ab0d93d39f95");
  ASSERT_NE(nullptr, account_folder_b1);
  EXPECT_EQ(u"Folder B1", account_folder_b1->GetTitle());
  EXPECT_EQ(23, account_folder_b1->id());

  histogram_tester.ExpectUniqueSample(kLocalOrSyncableIdsReassignedMetricName,
                                      /*sample=*/false,
                                      /*expected_bucket_count=*/1);
  histogram_tester.ExpectUniqueSample(kAccountIdsReassignedMetricName,
                                      /*sample=*/false,
                                      /*expected_bucket_count=*/1);
}

TEST(ModelLoaderTest, LoadTwoFilesWithCollidingIdsAcross) {
  base::HistogramTester histogram_tester;
  base::test::TaskEnvironment task_environment;
  const base::FilePath test_file =
      GetTestDataDir().AppendASCII("bookmarks/model_with_sync_metadata_1.json");
  ASSERT_TRUE(base::PathExists(test_file));

  base::test::TestFuture<std::unique_ptr<BookmarkLoadDetails>> details_future;
  scoped_refptr<ModelLoader> loader = ModelLoader::Create(
      /*local_or_syncable_file_path=*/test_file,
      /*account_file_path=*/test_file,
      /*load_managed_node_callback=*/LoadManagedNodeCallback(),
      details_future.GetCallback());

  const std::unique_ptr<BookmarkLoadDetails> details = details_future.Take();

  ASSERT_NE(nullptr, details);
  ASSERT_NE(nullptr, details->bb_node());
  ASSERT_NE(nullptr, details->other_folder_node());
  ASSERT_NE(nullptr, details->mobile_folder_node());
  ASSERT_NE(nullptr, details->account_bb_node());
  ASSERT_NE(nullptr, details->account_other_folder_node());
  ASSERT_NE(nullptr, details->account_mobile_folder_node());

  // ID collisions should have triggered recovery and reassignment of IDs.
  EXPECT_TRUE(details->required_recovery());
  EXPECT_TRUE(details->ids_reassigned());

  EXPECT_EQ(20, details->max_id());

  EXPECT_EQ(1u, details->bb_node()->children().size());
  EXPECT_EQ(1u, details->other_folder_node()->children().size());
  EXPECT_EQ(1u, details->mobile_folder_node()->children().size());
  EXPECT_EQ(1u, details->account_bb_node()->children().size());
  EXPECT_EQ(1u, details->account_other_folder_node()->children().size());
  EXPECT_EQ(1u, details->account_mobile_folder_node()->children().size());

  EXPECT_EQ("dummy-sync-metadata-1",
            details->local_or_syncable_sync_metadata_str());
  EXPECT_EQ("dummy-sync-metadata-1", details->account_sync_metadata_str());

  const UuidIndex local_or_syncable_uuid_index =
      details->owned_local_or_syncable_uuid_index();
  const UuidIndex account_uuid_index = details->owned_account_uuid_index();

  // Sanity-check the presence of one node. The UUID should not have changed.
  const BookmarkNode* account_folder_b1 = FindNodeByUuid(
      account_uuid_index, "da47f36f-050f-4ac9-aa35-ab0d93d39f95");
  ASSERT_NE(nullptr, account_folder_b1);
  EXPECT_EQ(u"Folder B1", account_folder_b1->GetTitle());
  EXPECT_EQ(4, account_folder_b1->id());

  const BookmarkNode* local_or_syncable_folder_b1 = FindNodeByUuid(
      local_or_syncable_uuid_index, "da47f36f-050f-4ac9-aa35-ab0d93d39f95");
  ASSERT_NE(nullptr, local_or_syncable_folder_b1);
  EXPECT_EQ(u"Folder B1", local_or_syncable_folder_b1->GetTitle());
  // The local-or-syncable node ID gets reassigned. The precise value isn't
  // important, but it is added here as overly-strict requirement to document
  // the behavior.
  EXPECT_EQ(15, local_or_syncable_folder_b1->id());

  histogram_tester.ExpectUniqueSample(kLocalOrSyncableIdsReassignedMetricName,
                                      /*sample=*/true,
                                      /*expected_bucket_count=*/1);
  histogram_tester.ExpectUniqueSample(kAccountIdsReassignedMetricName,
                                      /*sample=*/false,
                                      /*expected_bucket_count=*/1);
}

TEST(ModelLoaderTest, LoadTwoFilesWhereFirstHasInternalIdCollisions) {
  base::HistogramTester histogram_tester;
  base::test::TaskEnvironment task_environment;
  const base::FilePath test_file1 =
      GetTestDataDir().AppendASCII("bookmarks/model_with_duplicate_ids.json");
  const base::FilePath test_file2 =
      GetTestDataDir().AppendASCII("bookmarks/model_with_sync_metadata_2.json");
  ASSERT_TRUE(base::PathExists(test_file1));
  ASSERT_TRUE(base::PathExists(test_file2));

  base::test::TestFuture<std::unique_ptr<BookmarkLoadDetails>> details_future;
  scoped_refptr<ModelLoader> loader = ModelLoader::Create(
      /*local_or_syncable_file_path=*/test_file1,
      /*account_file_path=*/test_file2,
      /*load_managed_node_callback=*/LoadManagedNodeCallback(),
      details_future.GetCallback());

  const std::unique_ptr<BookmarkLoadDetails> details = details_future.Take();

  ASSERT_NE(nullptr, details);
  ASSERT_NE(nullptr, details->bb_node());
  ASSERT_NE(nullptr, details->other_folder_node());
  ASSERT_NE(nullptr, details->mobile_folder_node());
  ASSERT_NE(nullptr, details->account_bb_node());
  ASSERT_NE(nullptr, details->account_other_folder_node());
  ASSERT_NE(nullptr, details->account_mobile_folder_node());

  // ID collisions should have triggered recovery and reassignment of IDs.
  EXPECT_TRUE(details->required_recovery());
  EXPECT_TRUE(details->ids_reassigned());

  EXPECT_EQ(33, details->max_id());

  EXPECT_EQ(1u, details->bb_node()->children().size());
  EXPECT_EQ(1u, details->other_folder_node()->children().size());
  EXPECT_EQ(1u, details->mobile_folder_node()->children().size());
  EXPECT_EQ(1u, details->account_bb_node()->children().size());
  EXPECT_EQ(1u, details->account_other_folder_node()->children().size());
  EXPECT_EQ(1u, details->account_mobile_folder_node()->children().size());

  EXPECT_EQ("dummy-sync-metadata-1",
            details->local_or_syncable_sync_metadata_str());
  EXPECT_EQ("dummy-sync-metadata-2", details->account_sync_metadata_str());

  const UuidIndex local_or_syncable_uuid_index =
      details->owned_local_or_syncable_uuid_index();
  const UuidIndex account_uuid_index = details->owned_account_uuid_index();

  // Sanity-check the presence of one node. The UUID should not have changed.
  const BookmarkNode* account_folder_b1 = FindNodeByUuid(
      account_uuid_index, "da47f36f-050f-4ac9-aa35-ab0d93d39f95");
  ASSERT_NE(nullptr, account_folder_b1);
  EXPECT_EQ(u"Folder B1", account_folder_b1->GetTitle());
  // The node ID for account bookmarks stay unchanged. This isn't particularly
  // important, but it is added here as overly-strict requirement to document
  // the behavior.
  EXPECT_EQ(23, account_folder_b1->id());

  const BookmarkNode* local_or_syncable_folder_b1 = FindNodeByUuid(
      local_or_syncable_uuid_index, "da47f36f-050f-4ac9-aa35-ab0d93d39f95");
  ASSERT_NE(nullptr, local_or_syncable_folder_b1);
  EXPECT_EQ(u"Folder B1", local_or_syncable_folder_b1->GetTitle());
  // The node ID gets reassigned. The precise value isn't important, but it is
  // added here as overly-strict requirement to document the behavior.
  EXPECT_EQ(28, local_or_syncable_folder_b1->id());

  histogram_tester.ExpectUniqueSample(kLocalOrSyncableIdsReassignedMetricName,
                                      /*sample=*/true,
                                      /*expected_bucket_count=*/1);
  histogram_tester.ExpectUniqueSample(kAccountIdsReassignedMetricName,
                                      /*sample=*/false,
                                      /*expected_bucket_count=*/1);
}

TEST(ModelLoaderTest, LoadTwoFilesWhereSecondHasInternalIdCollisions) {
  base::HistogramTester histogram_tester;
  base::test::TaskEnvironment task_environment;
  const base::FilePath test_file1 =
      GetTestDataDir().AppendASCII("bookmarks/model_with_sync_metadata_2.json");
  const base::FilePath test_file2 =
      GetTestDataDir().AppendASCII("bookmarks/model_with_duplicate_ids.json");
  ASSERT_TRUE(base::PathExists(test_file1));
  ASSERT_TRUE(base::PathExists(test_file2));

  base::test::TestFuture<std::unique_ptr<BookmarkLoadDetails>> details_future;
  scoped_refptr<ModelLoader> loader = ModelLoader::Create(
      /*local_or_syncable_file_path=*/test_file1,
      /*account_file_path=*/test_file2,
      /*load_managed_node_callback=*/LoadManagedNodeCallback(),
      details_future.GetCallback());

  const std::unique_ptr<BookmarkLoadDetails> details = details_future.Take();

  ASSERT_NE(nullptr, details);
  ASSERT_NE(nullptr, details->bb_node());
  ASSERT_NE(nullptr, details->other_folder_node());
  ASSERT_NE(nullptr, details->mobile_folder_node());
  ASSERT_NE(nullptr, details->account_bb_node());
  ASSERT_NE(nullptr, details->account_other_folder_node());
  ASSERT_NE(nullptr, details->account_mobile_folder_node());

  // ID collisions should have triggered recovery and reassignment of IDs.
  EXPECT_TRUE(details->required_recovery());
  EXPECT_TRUE(details->ids_reassigned());

  EXPECT_EQ(19, details->max_id());

  EXPECT_EQ(1u, details->bb_node()->children().size());
  EXPECT_EQ(1u, details->other_folder_node()->children().size());
  EXPECT_EQ(1u, details->mobile_folder_node()->children().size());
  EXPECT_EQ(1u, details->account_bb_node()->children().size());
  EXPECT_EQ(1u, details->account_other_folder_node()->children().size());
  EXPECT_EQ(1u, details->account_mobile_folder_node()->children().size());

  EXPECT_EQ("dummy-sync-metadata-2",
            details->local_or_syncable_sync_metadata_str());
  EXPECT_EQ("dummy-sync-metadata-1", details->account_sync_metadata_str());

  const UuidIndex local_or_syncable_uuid_index =
      details->owned_local_or_syncable_uuid_index();
  const UuidIndex account_uuid_index = details->owned_account_uuid_index();

  // Sanity-check the presence of one node. The UUID should not have changed.
  const BookmarkNode* account_folder_b1 = FindNodeByUuid(
      account_uuid_index, "da47f36f-050f-4ac9-aa35-ab0d93d39f95");
  ASSERT_NE(nullptr, account_folder_b1);
  EXPECT_EQ(u"Folder B1", account_folder_b1->GetTitle());
  // The node ID gets reassigned. The precise value isn't important, but it is
  // added here as overly-strict requirement to document the behavior.
  EXPECT_EQ(5, account_folder_b1->id());

  const BookmarkNode* local_or_syncable_folder_b1 = FindNodeByUuid(
      local_or_syncable_uuid_index, "da47f36f-050f-4ac9-aa35-ab0d93d39f95");
  ASSERT_NE(nullptr, local_or_syncable_folder_b1);
  EXPECT_EQ(u"Folder B1", local_or_syncable_folder_b1->GetTitle());
  // The node ID gets reassigned. The precise value isn't important, but it is
  // added here as overly-strict requirement to document the behavior.
  EXPECT_EQ(14, local_or_syncable_folder_b1->id());

  histogram_tester.ExpectUniqueSample(kLocalOrSyncableIdsReassignedMetricName,
                                      /*sample=*/true,
                                      /*expected_bucket_count=*/1);
  histogram_tester.ExpectUniqueSample(kAccountIdsReassignedMetricName,
                                      /*sample=*/true,
                                      /*expected_bucket_count=*/1);
}

TEST(ModelLoaderTest, LoadTwoFilesWhereBothHaveInternalIdCollisions) {
  base::HistogramTester histogram_tester;
  base::test::TaskEnvironment task_environment;
  const base::FilePath test_file1 =
      GetTestDataDir().AppendASCII("bookmarks/model_with_duplicate_ids.json");
  const base::FilePath test_file2 =
      GetTestDataDir().AppendASCII("bookmarks/model_with_duplicate_ids.json");
  ASSERT_TRUE(base::PathExists(test_file1));
  ASSERT_TRUE(base::PathExists(test_file2));

  base::test::TestFuture<std::unique_ptr<BookmarkLoadDetails>> details_future;
  scoped_refptr<ModelLoader> loader = ModelLoader::Create(
      /*local_or_syncable_file_path=*/test_file1,
      /*account_file_path=*/test_file2,
      /*load_managed_node_callback=*/LoadManagedNodeCallback(),
      details_future.GetCallback());

  const std::unique_ptr<BookmarkLoadDetails> details = details_future.Take();

  ASSERT_NE(nullptr, details);
  ASSERT_NE(nullptr, details->bb_node());
  ASSERT_NE(nullptr, details->other_folder_node());
  ASSERT_NE(nullptr, details->mobile_folder_node());
  ASSERT_NE(nullptr, details->account_bb_node());
  ASSERT_NE(nullptr, details->account_other_folder_node());
  ASSERT_NE(nullptr, details->account_mobile_folder_node());

  // ID collisions should have triggered recovery and reassignment of IDs.
  EXPECT_TRUE(details->required_recovery());
  EXPECT_TRUE(details->ids_reassigned());

  EXPECT_EQ(19, details->max_id());

  EXPECT_EQ(1u, details->bb_node()->children().size());
  EXPECT_EQ(1u, details->other_folder_node()->children().size());
  EXPECT_EQ(1u, details->mobile_folder_node()->children().size());
  EXPECT_EQ(1u, details->account_bb_node()->children().size());
  EXPECT_EQ(1u, details->account_other_folder_node()->children().size());
  EXPECT_EQ(1u, details->account_mobile_folder_node()->children().size());

  EXPECT_EQ("dummy-sync-metadata-1",
            details->local_or_syncable_sync_metadata_str());
  EXPECT_EQ("dummy-sync-metadata-1", details->account_sync_metadata_str());

  const UuidIndex local_or_syncable_uuid_index =
      details->owned_local_or_syncable_uuid_index();
  const UuidIndex account_uuid_index = details->owned_account_uuid_index();

  // Sanity-check the presence of one node. The UUID should not have changed.
  const BookmarkNode* account_folder_b1 = FindNodeByUuid(
      account_uuid_index, "da47f36f-050f-4ac9-aa35-ab0d93d39f95");
  ASSERT_NE(nullptr, account_folder_b1);
  EXPECT_EQ(u"Folder B1", account_folder_b1->GetTitle());
  // The node ID gets reassigned. The precise value isn't important, but it is
  // added here as overly-strict requirement to document the behavior.
  EXPECT_EQ(5, account_folder_b1->id());

  const BookmarkNode* local_or_syncable_folder_b1 = FindNodeByUuid(
      local_or_syncable_uuid_index, "da47f36f-050f-4ac9-aa35-ab0d93d39f95");
  ASSERT_NE(nullptr, local_or_syncable_folder_b1);
  EXPECT_EQ(u"Folder B1", local_or_syncable_folder_b1->GetTitle());
  // The node ID gets reassigned. The precise value isn't important, but it is
  // added here as overly-strict requirement to document the behavior.
  EXPECT_EQ(14, local_or_syncable_folder_b1->id());

  histogram_tester.ExpectUniqueSample(kLocalOrSyncableIdsReassignedMetricName,
                                      /*sample=*/true,
                                      /*expected_bucket_count=*/1);
  histogram_tester.ExpectUniqueSample(kAccountIdsReassignedMetricName,
                                      /*sample=*/true,
                                      /*expected_bucket_count=*/1);
}

TEST(ModelLoaderTest, LoadTwoFilesWhereTheLocalOrSyncableFileDoesNotExist) {
  base::HistogramTester histogram_tester;
  base::test::TaskEnvironment task_environment;
  const base::FilePath test_file1 =
      GetTestDataDir().AppendASCII("bookmarks/inexistent_file.json");
  const base::FilePath test_file2 =
      GetTestDataDir().AppendASCII("bookmarks/model_with_sync_metadata_1.json");
  ASSERT_FALSE(base::PathExists(test_file1));
  ASSERT_TRUE(base::PathExists(test_file2));

  base::test::TestFuture<std::unique_ptr<BookmarkLoadDetails>> details_future;
  scoped_refptr<ModelLoader> loader = ModelLoader::Create(
      /*local_or_syncable_file_path=*/test_file1,
      /*account_file_path=*/test_file2,
      /*load_managed_node_callback=*/LoadManagedNodeCallback(),
      details_future.GetCallback());

  const std::unique_ptr<BookmarkLoadDetails> details = details_future.Take();

  ASSERT_NE(nullptr, details);
  ASSERT_NE(nullptr, details->bb_node());
  ASSERT_NE(nullptr, details->other_folder_node());
  ASSERT_NE(nullptr, details->mobile_folder_node());
  ASSERT_NE(nullptr, details->account_bb_node());
  ASSERT_NE(nullptr, details->account_other_folder_node());
  ASSERT_NE(nullptr, details->account_mobile_folder_node());

  // The JSON file used to load account nodes uses specific IDs that are
  // actually important for this test. One behavior being tested here is that
  // local-or-syncable bookmarks (inexistent file) will not be treated as
  // collision.
  ASSERT_EQ(1, details->account_bb_node()->id());
  ASSERT_EQ(2, details->account_other_folder_node()->id());
  ASSERT_EQ(10, details->account_mobile_folder_node()->id());

  EXPECT_FALSE(details->required_recovery());
  EXPECT_FALSE(details->ids_reassigned());

  EXPECT_EQ(1u, details->account_bb_node()->children().size());
  EXPECT_EQ(1u, details->account_other_folder_node()->children().size());
  EXPECT_EQ(1u, details->account_mobile_folder_node()->children().size());

  EXPECT_TRUE(details->bb_node()->children().empty());
  EXPECT_TRUE(details->other_folder_node()->children().empty());
  EXPECT_TRUE(details->mobile_folder_node()->children().empty());

  // Local-or-syncable permanent nodes should have been assigned new IDs that
  // do not conflict with account nodes.
  EXPECT_EQ(11, details->bb_node()->id());
  EXPECT_EQ(12, details->other_folder_node()->id());
  EXPECT_EQ(13, details->mobile_folder_node()->id());

  EXPECT_EQ(14, details->max_id());

  EXPECT_EQ("", details->local_or_syncable_sync_metadata_str());
  EXPECT_EQ("dummy-sync-metadata-1", details->account_sync_metadata_str());

  histogram_tester.ExpectTotalCount(kLocalOrSyncableIdsReassignedMetricName,
                                    /*expected_count=*/0);
  histogram_tester.ExpectUniqueSample(kAccountIdsReassignedMetricName,
                                      /*sample=*/false,
                                      /*expected_bucket_count=*/1);
}

}  // namespace

}  // namespace bookmarks
