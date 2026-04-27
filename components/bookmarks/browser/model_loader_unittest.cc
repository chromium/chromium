// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/bookmarks/browser/model_loader.h"

#include <memory>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/no_destructor.h"
#include "base/path_service.h"
#include "base/strings/strcat.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_file_util.h"
#include "base/test/test_future.h"
#include "components/bookmarks/browser/bookmark_client.h"
#include "components/bookmarks/browser/bookmark_load_details.h"
#include "components/bookmarks/common/bookmark_metrics.h"
#include "components/bookmarks/common/storage_file_encryption_type.h"
#include "components/bookmarks/test/bookmark_test_with_encryption_stages.h"
#include "components/os_crypt/async/browser/test_utils.h"
#include "components/os_crypt/async/common/encryptor.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace bookmarks {
namespace {

constexpr char kLocalOrSyncableIdsReassignedMetricName[] =
    "Bookmarks.IdsReassigned2.OnProfileLoad.LocalOrSyncable";
constexpr char kAccountIdsReassignedMetricName[] =
    "Bookmarks.IdsReassigned2.OnProfileLoad.Account";
constexpr char kUserFolderCountMetricName[] =
    "Bookmarks.UserFolder.OnProfileLoad.Count";
constexpr char kUserFolderTopLevelCountMetricName[] =
    "Bookmarks.UserFolder.OnProfileLoad.TopLevelCount";
constexpr char kUserFolderBookmarkBarTopLevelItemsMetricName[] =
    "Bookmarks.UserFolder.OnProfileLoad.BookmarkBarTopLevelItems";
constexpr char kBookmarksFileLoadResultMetricName[] =
    "Bookmarks.BookmarksFileLoadResult";
constexpr char kEncryptedBookmarksFileMatchesResultMetricName[] =
    "Bookmarks.EncryptedBookmarksFileMatchesResult";
constexpr char kFallbackToClearTextFileOnLoadResultMetricName[] =
    "Bookmarks.FallbackToClearTextFileOnLoadResult";
constexpr char kBookmarksStorageFileSizeAtStartupMetricName[] =
    "Bookmarks.Storage.FileSizeAtStartup2";
constexpr char kBookmarksEncryptedStorageFileSizeAtStartupMetricName[] =
    "Bookmarks.Storage.EncryptedFileSizeAtStartup";
constexpr char kBookmarksTimeToReadFileMetricName[] =
    "Bookmarks.TimeToReadFile";
constexpr char kBookmarksAverageNodeSizeMetricName[] =
    "Bookmarks.AverageNodeSize";
constexpr char kBookmarksDeleteFileMetricName[] =
    "Bookmarks.DeleteClearTextFile";
constexpr base::FilePath::CharType kBackupExtension[] =
    FILE_PATH_LITERAL("bak");

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

MATCHER(FileAndBackupFileExist, "") {
  return base::PathExists(arg) &&
         base::PathExists(arg.ReplaceExtension(kBackupExtension));
}

TEST(ModelLoaderTest, LoadEmptyModelFromInexistentFile) {
  base::HistogramTester histogram_tester;
  base::test::TaskEnvironment task_environment;
  const base::FilePath test_file =
      GetTestDataDir().AppendASCII("inexistent_bookmarks_file.json");
  ASSERT_FALSE(base::PathExists(test_file));

  base::test::TestFuture<std::unique_ptr<BookmarkLoadDetails>> details_future;
  scoped_refptr<ModelLoader> loader = ModelLoader::Create(
      /*encryptor=*/nullptr,
      /*local_or_syncable_file_path=*/test_file,
      /*encrypted_local_or_syncable_file_path=*/base::FilePath(),
      /*account_file_path=*/base::FilePath(),
      /*encrypted_account_file_path=*/base::FilePath(),
      LoadManagedNodeCallback(),
      /*save_local_or_syncable_secondary_file_callback=*/base::DoNothing(),
      /*save_account_secondary_file_callback=*/base::DoNothing(),
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

  histogram_tester.ExpectTotalCount(kUserFolderCountMetricName,
                                    /*expected_count=*/1);
  histogram_tester.ExpectBucketCount(kUserFolderCountMetricName,
                                     /*sample=*/0, /*expected_count=*/1);

  histogram_tester.ExpectTotalCount(kUserFolderTopLevelCountMetricName,
                                    /*expected_count=*/1);
  histogram_tester.ExpectBucketCount(kUserFolderTopLevelCountMetricName,
                                     /*sample=*/0, /*expected_count=*/1);

  histogram_tester.ExpectTotalCount(
      kUserFolderBookmarkBarTopLevelItemsMetricName,
      /*expected_count=*/1);
  histogram_tester.ExpectBucketCount(
      kUserFolderBookmarkBarTopLevelItemsMetricName,
      /*sample=*/0, /*expected_count=*/1);

  histogram_tester.ExpectTotalCount(
      base::StrCat({kBookmarksFileLoadResultMetricName, ".LocalOrSyncable",
                    ".ClearText"}),
      /*expected_count=*/1);
  histogram_tester.ExpectBucketCount(
      base::StrCat({kBookmarksFileLoadResultMetricName, ".LocalOrSyncable",
                    ".ClearText"}),
      /*sample=*/metrics::BookmarksFileLoadResult::kFileMissing,
      /*expected_count=*/1);
  histogram_tester.ExpectTotalCount(
      base::StrCat(
          {kBookmarksFileLoadResultMetricName, ".Account", ".ClearText"}),
      /*expected_count=*/0);
}

TEST(ModelLoaderTest, LoadEmptyModelFromInvalidJson) {
  base::HistogramTester histogram_tester;
  base::test::TaskEnvironment task_environment;
  const base::FilePath test_file =
      GetTestDataDir().AppendASCII("bookmarks/model_invalid_json.json");
  ASSERT_TRUE(base::PathExists(test_file));

  base::test::TestFuture<std::unique_ptr<BookmarkLoadDetails>> details_future;
  scoped_refptr<ModelLoader> loader = ModelLoader::Create(
      /*encryptor=*/nullptr,
      /*local_or_syncable_file_path=*/test_file,
      /*encrypted_local_or_syncable_file_path=*/base::FilePath(),
      /*account_file_path=*/base::FilePath(),
      /*encrypted_account_file_path=*/base::FilePath(),
      LoadManagedNodeCallback(),
      /*save_local_or_syncable_secondary_file_callback=*/base::DoNothing(),
      /*save_account_secondary_file_callback=*/base::DoNothing(),
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

  histogram_tester.ExpectTotalCount(kUserFolderCountMetricName,
                                    /*expected_count=*/1);
  histogram_tester.ExpectBucketCount(kUserFolderCountMetricName,
                                     /*sample=*/0, /*expected_count=*/1);

  histogram_tester.ExpectTotalCount(kUserFolderTopLevelCountMetricName,
                                    /*expected_count=*/1);
  histogram_tester.ExpectBucketCount(kUserFolderTopLevelCountMetricName,
                                     /*sample=*/0, /*expected_count=*/1);

  histogram_tester.ExpectTotalCount(
      kUserFolderBookmarkBarTopLevelItemsMetricName,
      /*expected_count=*/1);
  histogram_tester.ExpectBucketCount(
      kUserFolderBookmarkBarTopLevelItemsMetricName,
      /*sample=*/0, /*expected_count=*/1);
  histogram_tester.ExpectTotalCount(
      base::StrCat({kBookmarksFileLoadResultMetricName, ".LocalOrSyncable",
                    ".ClearText"}),
      /*expected_count=*/1);
  histogram_tester.ExpectBucketCount(
      base::StrCat({kBookmarksFileLoadResultMetricName, ".LocalOrSyncable",
                    ".ClearText"}),
      /*sample=*/
      metrics::BookmarksFileLoadResult::kJSONParsingFailed,
      /*expected_count=*/1);
  histogram_tester.ExpectTotalCount(
      base::StrCat(
          {kBookmarksFileLoadResultMetricName, ".Account", ".ClearText"}),
      /*expected_count=*/0);
}

TEST(ModelLoaderTest, LoadEmptyFromImproperlyEncodedJSON) {
  base::HistogramTester histogram_tester;
  base::test::TaskEnvironment task_environment;
  const base::FilePath test_file =
      GetTestDataDir().AppendASCII("bookmarks/model_without_version.json");
  ASSERT_TRUE(base::PathExists(test_file));

  base::test::TestFuture<std::unique_ptr<BookmarkLoadDetails>> details_future;
  scoped_refptr<ModelLoader> loader = ModelLoader::Create(
      /*encryptor=*/nullptr,
      /*local_or_syncable_file_path=*/test_file,
      /*encrypted_local_or_syncable_file_path=*/base::FilePath(),
      /*account_file_path=*/base::FilePath(),
      /*encrypted_account_file_path=*/base::FilePath(),
      LoadManagedNodeCallback(),
      /*save_local_or_syncable_secondary_file_callback=*/base::DoNothing(),
      /*save_account_secondary_file_callback=*/base::DoNothing(),
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

  histogram_tester.ExpectTotalCount(kUserFolderCountMetricName,
                                    /*expected_count=*/1);
  histogram_tester.ExpectBucketCount(kUserFolderCountMetricName,
                                     /*sample=*/0, /*expected_count=*/1);

  histogram_tester.ExpectTotalCount(kUserFolderTopLevelCountMetricName,
                                    /*expected_count=*/1);
  histogram_tester.ExpectBucketCount(kUserFolderTopLevelCountMetricName,
                                     /*sample=*/0, /*expected_count=*/1);

  histogram_tester.ExpectTotalCount(
      kUserFolderBookmarkBarTopLevelItemsMetricName,
      /*expected_count=*/1);
  histogram_tester.ExpectBucketCount(
      kUserFolderBookmarkBarTopLevelItemsMetricName,
      /*sample=*/0, /*expected_count=*/1);
  histogram_tester.ExpectTotalCount(
      base::StrCat({kBookmarksFileLoadResultMetricName, ".LocalOrSyncable",
                    ".ClearText"}),
      /*expected_count=*/1);
  histogram_tester.ExpectBucketCount(
      base::StrCat({kBookmarksFileLoadResultMetricName, ".LocalOrSyncable",
                    ".ClearText"}),
      /*sample=*/
      metrics::BookmarksFileLoadResult::kBookmarkCodecDecodingFailed,
      /*expected_count=*/1);
  histogram_tester.ExpectTotalCount(
      base::StrCat(
          {kBookmarksFileLoadResultMetricName, ".Account", ".ClearText"}),
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
      /*encryptor=*/nullptr,
      /*local_or_syncable_file_path=*/test_file,
      /*encrypted_local_or_syncable_file_path=*/base::FilePath(),
      /*account_file_path=*/base::FilePath(),
      /*encrypted_account_file_path=*/base::FilePath(),
      LoadManagedNodeCallback(),
      /*save_local_or_syncable_secondary_file_callback=*/base::DoNothing(),
      /*save_account_secondary_file_callback=*/base::DoNothing(),
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

  histogram_tester.ExpectTotalCount(kUserFolderCountMetricName,
                                    /*expected_count=*/1);
  histogram_tester.ExpectBucketCount(kUserFolderCountMetricName,
                                     /*sample=*/3, /*expected_count=*/1);

  histogram_tester.ExpectTotalCount(kUserFolderTopLevelCountMetricName,
                                    /*expected_count=*/1);
  histogram_tester.ExpectBucketCount(kUserFolderTopLevelCountMetricName,
                                     /*sample=*/3, /*expected_count=*/1);

  histogram_tester.ExpectTotalCount(
      kUserFolderBookmarkBarTopLevelItemsMetricName,
      /*expected_count=*/1);
  histogram_tester.ExpectBucketCount(
      kUserFolderBookmarkBarTopLevelItemsMetricName,
      /*sample=*/1, /*expected_count=*/1);

  histogram_tester.ExpectTotalCount(
      base::StrCat({kBookmarksFileLoadResultMetricName, ".LocalOrSyncable",
                    ".ClearText"}),
      /*expected_count=*/1);
  histogram_tester.ExpectBucketCount(
      base::StrCat({kBookmarksFileLoadResultMetricName, ".LocalOrSyncable",
                    ".ClearText"}),
      /*sample=*/metrics::BookmarksFileLoadResult::kSuccess,
      /*expected_count=*/1);
  histogram_tester.ExpectTotalCount(
      base::StrCat(
          {kBookmarksFileLoadResultMetricName, ".Account", ".ClearText"}),
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
      /*encryptor=*/nullptr,
      /*local_or_syncable_file_path=*/test_file,
      /*encrypted_local_or_syncable_file_path=*/base::FilePath(),
      /*account_file_path=*/base::FilePath(),
      /*encrypted_account_file_path=*/base::FilePath(),
      LoadManagedNodeCallback(),
      /*save_local_or_syncable_secondary_file_callback=*/base::DoNothing(),
      /*save_account_secondary_file_callback=*/base::DoNothing(),
      details_future.GetCallback());

  const std::unique_ptr<BookmarkLoadDetails> details = details_future.Take();

  ASSERT_NE(nullptr, details);
  ASSERT_NE(nullptr, details->bb_node());
  ASSERT_NE(nullptr, details->other_folder_node());
  ASSERT_NE(nullptr, details->mobile_folder_node());

  EXPECT_TRUE(details->required_recovery());
  EXPECT_TRUE(details->ids_reassigned());
  EXPECT_EQ(12, details->max_id());

  EXPECT_EQ(1u, details->bb_node()->children().size());
  EXPECT_EQ(1u, details->other_folder_node()->children().size());
  EXPECT_EQ(1u, details->mobile_folder_node()->children().size());

  // Permanent node ID's are subject to change, but expectations are listed
  // below for the purpose of documenting the current behavior. Note that in
  // this case some permanent folders get non-standard IDs assigned.
  EXPECT_EQ(1u, details->bb_node()->id());
  EXPECT_EQ(4u, details->other_folder_node()->id());
  EXPECT_EQ(10u, details->mobile_folder_node()->id());

  EXPECT_EQ("dummy-sync-metadata-1",
            details->local_or_syncable_sync_metadata_str());

  const UuidIndex uuid_index = details->owned_local_or_syncable_uuid_index();

  // Sanity-check the presence of one node.
  const BookmarkNode* folder_b1 =
      FindNodeByUuid(uuid_index, "da47f36f-050f-4ac9-aa35-ab0d93d39f95");
  ASSERT_NE(nullptr, folder_b1);
  EXPECT_EQ(u"Folder B1", folder_b1->GetTitle());
  EXPECT_EQ(11, folder_b1->id());

  histogram_tester.ExpectUniqueSample(kLocalOrSyncableIdsReassignedMetricName,
                                      /*sample=*/true,
                                      /*expected_bucket_count=*/1);
  histogram_tester.ExpectTotalCount(kAccountIdsReassignedMetricName,
                                    /*expected_count=*/0);

  histogram_tester.ExpectTotalCount(
      base::StrCat({kBookmarksFileLoadResultMetricName, ".LocalOrSyncable",
                    ".ClearText"}),
      /*expected_count=*/1);
  histogram_tester.ExpectBucketCount(
      base::StrCat({kBookmarksFileLoadResultMetricName, ".LocalOrSyncable",
                    ".ClearText"}),
      /*sample=*/metrics::BookmarksFileLoadResult::kSuccess,
      /*expected_count=*/1);
  histogram_tester.ExpectTotalCount(
      base::StrCat(
          {kBookmarksFileLoadResultMetricName, ".Account", ".ClearText"}),
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
      /*encryptor=*/nullptr,
      /*local_or_syncable_file_path=*/test_file1,
      /*encrypted_local_or_syncable_file_path=*/base::FilePath(),
      /*account_file_path=*/test_file2,
      /*encrypted_account_file_path=*/base::FilePath(),
      LoadManagedNodeCallback(),
      /*save_local_or_syncable_secondary_file_callback=*/base::DoNothing(),
      /*save_account_secondary_file_callback=*/base::DoNothing(),
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

  histogram_tester.ExpectTotalCount(kUserFolderCountMetricName,
                                    /*expected_count=*/1);
  histogram_tester.ExpectBucketCount(kUserFolderCountMetricName,
                                     /*sample=*/6, /*expected_count=*/1);

  histogram_tester.ExpectTotalCount(kUserFolderTopLevelCountMetricName,
                                    /*expected_count=*/1);
  histogram_tester.ExpectBucketCount(kUserFolderTopLevelCountMetricName,
                                     /*sample=*/6, /*expected_count=*/1);

  histogram_tester.ExpectTotalCount(
      kUserFolderBookmarkBarTopLevelItemsMetricName,
      /*expected_count=*/1);
  histogram_tester.ExpectBucketCount(
      kUserFolderBookmarkBarTopLevelItemsMetricName,
      /*sample=*/1, /*expected_count=*/1);

  histogram_tester.ExpectTotalCount(
      base::StrCat({kBookmarksFileLoadResultMetricName, ".LocalOrSyncable",
                    ".ClearText"}),
      /*expected_count=*/1);
  histogram_tester.ExpectBucketCount(
      base::StrCat({kBookmarksFileLoadResultMetricName, ".LocalOrSyncable",
                    ".ClearText"}),
      /*sample=*/metrics::BookmarksFileLoadResult::kSuccess,
      /*expected_count=*/1);
  histogram_tester.ExpectTotalCount(
      base::StrCat(
          {kBookmarksFileLoadResultMetricName, ".Account", ".ClearText"}),
      /*expected_count=*/1);
  histogram_tester.ExpectBucketCount(
      base::StrCat(
          {kBookmarksFileLoadResultMetricName, ".Account", ".ClearText"}),
      /*sample=*/metrics::BookmarksFileLoadResult::kSuccess,
      /*expected_count=*/1);
}

TEST(ModelLoaderTest, LoadTwoFilesWithCollidingIdsAcross) {
  base::HistogramTester histogram_tester;
  base::test::TaskEnvironment task_environment;
  const base::FilePath test_file =
      GetTestDataDir().AppendASCII("bookmarks/model_with_sync_metadata_1.json");
  ASSERT_TRUE(base::PathExists(test_file));

  base::test::TestFuture<std::unique_ptr<BookmarkLoadDetails>> details_future;
  scoped_refptr<ModelLoader> loader = ModelLoader::Create(
      /*encryptor=*/nullptr,
      /*local_or_syncable_file_path=*/test_file,
      /*encrypted_local_or_syncable_file_path=*/base::FilePath(),
      /*account_file_path=*/test_file,
      /*encrypted_account_file_path=*/base::FilePath(),
      LoadManagedNodeCallback(),
      /*save_local_or_syncable_secondary_file_callback=*/base::DoNothing(),
      /*save_account_secondary_file_callback=*/base::DoNothing(),
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
      /*encryptor=*/nullptr,
      /*local_or_syncable_file_path=*/test_file1,
      /*encrypted_local_or_syncable_file_path=*/base::FilePath(),
      /*account_file_path=*/test_file2,
      /*encrypted_account_file_path=*/base::FilePath(),
      LoadManagedNodeCallback(),
      /*save_local_or_syncable_secondary_file_callback=*/base::DoNothing(),
      /*save_account_secondary_file_callback=*/base::DoNothing(),
      details_future.GetCallback());

  const std::unique_ptr<BookmarkLoadDetails> details = details_future.Take();

  ASSERT_NE(nullptr, details);
  ASSERT_NE(nullptr, details->bb_node());
  ASSERT_NE(nullptr, details->other_folder_node());
  ASSERT_NE(nullptr, details->mobile_folder_node());
  ASSERT_NE(nullptr, details->account_bb_node());
  ASSERT_NE(nullptr, details->account_other_folder_node());
  ASSERT_NE(nullptr, details->account_mobile_folder_node());

  // ID collisions should have triggered recovery and reassignment of some IDs.
  EXPECT_TRUE(details->required_recovery());
  EXPECT_TRUE(details->ids_reassigned());

  EXPECT_EQ(25, details->max_id());

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
  EXPECT_EQ(24, local_or_syncable_folder_b1->id());

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
      /*encryptor=*/nullptr,
      /*local_or_syncable_file_path=*/test_file1,
      /*encrypted_local_or_syncable_file_path=*/base::FilePath(),
      /*account_file_path=*/test_file2,
      /*encrypted_account_file_path=*/base::FilePath(),
      LoadManagedNodeCallback(),
      /*save_local_or_syncable_secondary_file_callback=*/base::DoNothing(),
      /*save_account_secondary_file_callback=*/base::DoNothing(),
      details_future.GetCallback());

  const std::unique_ptr<BookmarkLoadDetails> details = details_future.Take();

  ASSERT_NE(nullptr, details);
  ASSERT_NE(nullptr, details->bb_node());
  ASSERT_NE(nullptr, details->other_folder_node());
  ASSERT_NE(nullptr, details->mobile_folder_node());
  ASSERT_NE(nullptr, details->account_bb_node());
  ASSERT_NE(nullptr, details->account_other_folder_node());
  ASSERT_NE(nullptr, details->account_mobile_folder_node());

  // ID collisions should have triggered recovery and reassignment of some IDs.
  EXPECT_TRUE(details->required_recovery());
  EXPECT_TRUE(details->ids_reassigned());

  EXPECT_EQ(25, details->max_id());

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
  EXPECT_EQ(11, account_folder_b1->id());

  const BookmarkNode* local_or_syncable_folder_b1 = FindNodeByUuid(
      local_or_syncable_uuid_index, "da47f36f-050f-4ac9-aa35-ab0d93d39f95");
  ASSERT_NE(nullptr, local_or_syncable_folder_b1);
  EXPECT_EQ(u"Folder B1", local_or_syncable_folder_b1->GetTitle());
  // The node ID gets reassigned. The precise value isn't important, but it is
  // added here as overly-strict requirement to document the behavior.
  EXPECT_EQ(23, local_or_syncable_folder_b1->id());

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
      /*encryptor=*/nullptr,
      /*local_or_syncable_file_path=*/test_file1,
      /*encrypted_local_or_syncable_file_path=*/base::FilePath(),
      /*account_file_path=*/test_file2,
      /*encrypted_account_file_path=*/base::FilePath(),
      LoadManagedNodeCallback(),
      /*save_local_or_syncable_secondary_file_callback=*/base::DoNothing(),
      /*save_account_secondary_file_callback=*/base::DoNothing(),
      details_future.GetCallback());

  const std::unique_ptr<BookmarkLoadDetails> details = details_future.Take();

  ASSERT_NE(nullptr, details);
  ASSERT_NE(nullptr, details->bb_node());
  ASSERT_NE(nullptr, details->other_folder_node());
  ASSERT_NE(nullptr, details->mobile_folder_node());
  ASSERT_NE(nullptr, details->account_bb_node());
  ASSERT_NE(nullptr, details->account_other_folder_node());
  ASSERT_NE(nullptr, details->account_mobile_folder_node());

  // ID collisions should have triggered recovery and reassignment of some IDs.
  EXPECT_TRUE(details->required_recovery());
  EXPECT_TRUE(details->ids_reassigned());

  EXPECT_EQ(21, details->max_id());

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
  EXPECT_EQ(11, account_folder_b1->id());

  const BookmarkNode* local_or_syncable_folder_b1 = FindNodeByUuid(
      local_or_syncable_uuid_index, "da47f36f-050f-4ac9-aa35-ab0d93d39f95");
  ASSERT_NE(nullptr, local_or_syncable_folder_b1);
  EXPECT_EQ(u"Folder B1", local_or_syncable_folder_b1->GetTitle());
  // The node ID gets reassigned. The precise value isn't important, but it is
  // added here as overly-strict requirement to document the behavior.
  EXPECT_EQ(16, local_or_syncable_folder_b1->id());

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
      /*encryptor=*/nullptr,
      /*local_or_syncable_file_path=*/test_file1,
      /*encrypted_local_or_syncable_file_path=*/base::FilePath(),
      /*account_file_path=*/test_file2,
      /*encrypted_account_file_path=*/base::FilePath(),
      LoadManagedNodeCallback(),
      /*save_local_or_syncable_secondary_file_callback=*/base::DoNothing(),
      /*save_account_secondary_file_callback=*/base::DoNothing(),
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

  histogram_tester.ExpectTotalCount(
      base::StrCat({kBookmarksFileLoadResultMetricName, ".LocalOrSyncable",
                    ".ClearText"}),
      /*expected_count=*/1);
  histogram_tester.ExpectBucketCount(
      base::StrCat({kBookmarksFileLoadResultMetricName, ".LocalOrSyncable",
                    ".ClearText"}),
      /*sample=*/metrics::BookmarksFileLoadResult::kFileMissing,
      /*expected_count=*/1);
  histogram_tester.ExpectTotalCount(
      base::StrCat(
          {kBookmarksFileLoadResultMetricName, ".Account", ".ClearText"}),
      /*expected_count=*/1);
  histogram_tester.ExpectBucketCount(
      base::StrCat(
          {kBookmarksFileLoadResultMetricName, ".Account", ".ClearText"}),
      /*sample=*/metrics::BookmarksFileLoadResult::kSuccess,
      /*expected_count=*/1);
}

TEST(ModelLoaderTest, LoadModelWithNestedUserFolders) {
  base::HistogramTester histogram_tester;
  base::test::TaskEnvironment task_environment;
  const base::FilePath test_file =
      GetTestDataDir().AppendASCII("bookmarks/model_nested_user_folders.json");
  ASSERT_TRUE(base::PathExists(test_file));

  base::test::TestFuture<std::unique_ptr<BookmarkLoadDetails>> details_future;
  scoped_refptr<ModelLoader> loader = ModelLoader::Create(
      /*encryptor=*/nullptr,
      /*local_or_syncable_file_path=*/test_file,
      /*encrypted_local_or_syncable_file_path=*/base::FilePath(),
      /*account_file_path=*/base::FilePath(),
      /*encrypted_account_file_path=*/base::FilePath(),
      LoadManagedNodeCallback(),
      /*save_local_or_syncable_secondary_file_callback=*/base::DoNothing(),
      /*save_account_secondary_file_callback=*/base::DoNothing(),
      details_future.GetCallback());

  const std::unique_ptr<BookmarkLoadDetails> details = details_future.Take();
  ASSERT_NE(nullptr, details);

  histogram_tester.ExpectTotalCount(kUserFolderCountMetricName,
                                    /*expected_count=*/1);
  histogram_tester.ExpectBucketCount(kUserFolderCountMetricName,
                                     /*sample=*/7, /*expected_count=*/1);

  histogram_tester.ExpectTotalCount(kUserFolderTopLevelCountMetricName,
                                    /*expected_count=*/1);
  histogram_tester.ExpectBucketCount(kUserFolderTopLevelCountMetricName,
                                     /*sample=*/3, /*expected_count=*/1);

  histogram_tester.ExpectTotalCount(
      kUserFolderBookmarkBarTopLevelItemsMetricName,
      /*expected_count=*/1);
  histogram_tester.ExpectBucketCount(
      kUserFolderBookmarkBarTopLevelItemsMetricName,
      /*sample=*/2, /*expected_count=*/1);
}

void VerifyPrimaryLoadCorrectlySecondaryHasGivenResult(
    base::HistogramTester& histogram_tester,
    std::string_view primary_histogram_suffix,
    std::string_view secondary_histogram_suffix,
    metrics::BookmarksFileLoadResult secondary_result) {
  // Primary file load succeeds
  histogram_tester.ExpectUniqueSample(
      base::StrCat({kBookmarksFileLoadResultMetricName, ".LocalOrSyncable",
                    primary_histogram_suffix}),
      metrics::BookmarksFileLoadResult::kSuccess,
      /*expected_bucket_count=*/1);
  histogram_tester.ExpectUniqueSample(
      base::StrCat({kBookmarksFileLoadResultMetricName, ".Account",
                    primary_histogram_suffix}),
      metrics::BookmarksFileLoadResult::kSuccess,
      /*expected_bucket_count=*/1);

  histogram_tester.ExpectUniqueSample(
      base::StrCat({kBookmarksFileLoadResultMetricName, ".LocalOrSyncable",
                    secondary_histogram_suffix}),
      secondary_result,
      /*expected_bucket_count=*/1);
  histogram_tester.ExpectUniqueSample(
      base::StrCat({kBookmarksFileLoadResultMetricName, ".Account",
                    secondary_histogram_suffix}),
      secondary_result,
      /*expected_bucket_count=*/1);
}

base::FilePath CreateCopyWithBackup(const base::FilePath& filepath,
                                    const std::string_view& file_name) {
  const base::FilePath temp_dir = base::CreateUniqueTempDirectoryScopedToTest();
  const base::FilePath copy_filepath = temp_dir.AppendASCII(file_name);
  base::CopyFile(filepath, copy_filepath);
  base::CopyFile(filepath, copy_filepath.ReplaceExtension(kBackupExtension));
  return copy_filepath;
}

std::optional<base::FilePath> CreateTempEncryptedFile(
    const base::FilePath& file,
    const std::string_view& file_name,
    const scoped_refptr<base::RefCountedData<const os_crypt_async::Encryptor>>&
        encryptor) {
  std::string file_content;
  if (!base::ReadFileToString(file, &file_content)) {
    return std::nullopt;
  }
  std::string encrypted_file_content;
  if (!encryptor->data.EncryptString(file_content, &encrypted_file_content)) {
    return std::nullopt;
  }
  const base::FilePath temp_dir = base::CreateUniqueTempDirectoryScopedToTest();
  const base::FilePath encrypted_file = temp_dir.AppendASCII(file_name);
  if (!base::WriteFile(encrypted_file, encrypted_file_content)) {
    return std::nullopt;
  }
  return encrypted_file;
}

class ModelLoaderWithSecondayFileTest
    : public testing::TestWithParam<BookmarkEncryptionStage> {
 protected:
  ModelLoaderWithSecondayFileTest() {
    test::InitFeaturesForBookmarkTestEncryptionStage(feature_list_, GetParam());
  }

  bool IsEncryptedFilePrimary() {
    return GetParam() == BookmarkEncryptionStage::kWriteBothReadPreferEncrypted;
  }

  StorageFileEncryptionType GetSecondaryStorageFileEncryptionType() {
    return IsEncryptedFilePrimary() ? StorageFileEncryptionType::kClearText
                                    : StorageFileEncryptionType::kEncrypted;
  }

  base::FilePath CreateTempEncryptedFileOrClearTextCopy(
      bool primary_content,
      std::string_view existing_file_name,
      std::string_view new_file_name) {
    const base::FilePath existing_file_path =
        GetTestDataDir().AppendASCII(existing_file_name);
    if ((primary_content && IsEncryptedFilePrimary()) ||
        (!primary_content && !IsEncryptedFilePrimary())) {
      std::optional<base::FilePath> encrypted_file_path =
          CreateTempEncryptedFile(existing_file_path,
                                  base::StrCat({"Encrypted", new_file_name}),
                                  encryptor_);
      return encrypted_file_path.value();
    }
    return CreateCopyWithBackup(existing_file_path, new_file_name);
  }

  scoped_refptr<ModelLoader> CreateModelLoader(
      const base::FilePath& primary_local_or_syncable_file_path,
      const base::FilePath& secondary_local_or_syncable_file_path,
      const base::FilePath& primary_account_file_path,
      const base::FilePath& secondary_account_file_path,
      ModelLoader::SaveSingleFileCallback
          save_local_or_syncable_single_file_callback,
      ModelLoader::SaveSingleFileCallback save_account_single_file_callback) {
    if (IsEncryptedFilePrimary()) {
      return ModelLoader::Create(
          encryptor_, secondary_local_or_syncable_file_path,
          primary_local_or_syncable_file_path, secondary_account_file_path,
          primary_account_file_path, LoadManagedNodeCallback(),
          std::move(save_local_or_syncable_single_file_callback),
          std::move(save_account_single_file_callback),
          /*callback=*/base::DoNothing());
    }

    return ModelLoader::Create(
        encryptor_, primary_local_or_syncable_file_path,
        secondary_local_or_syncable_file_path, primary_account_file_path,
        secondary_account_file_path, LoadManagedNodeCallback(),
        std::move(save_local_or_syncable_single_file_callback),
        std::move(save_account_single_file_callback),
        /*callback=*/base::DoNothing());
  }

  std::string GetPrimaryEncryptionHistogramSuffix() {
    return IsEncryptedFilePrimary() ? ".Encrypted" : ".ClearText";
  }

  std::string GetSecondaryEncryptionHistogramSuffix() {
    return IsEncryptedFilePrimary() ? ".ClearText" : ".Encrypted";
  }

  const base::FilePath& GetClearTextFilePath(
      const base::FilePath& primary_file_path,
      const base::FilePath& secondary_file_path) {
    return IsEncryptedFilePrimary() ? secondary_file_path : primary_file_path;
  }

  void VerifyPrimaryLoadCorrectlySecondaryHasGivenResult(
      base::HistogramTester& histogram_tester,
      metrics::BookmarksFileLoadResult secondary_result) {
    bookmarks::VerifyPrimaryLoadCorrectlySecondaryHasGivenResult(
        histogram_tester, GetPrimaryEncryptionHistogramSuffix(),
        GetSecondaryEncryptionHistogramSuffix(), secondary_result);
  }

  base::test::ScopedFeatureList feature_list_;
  scoped_refptr<base::RefCountedData<const os_crypt_async::Encryptor>>
      encryptor_ = base::MakeRefCounted<
          base::RefCountedData<const os_crypt_async::Encryptor>>(
          std::in_place,
          os_crypt_async::GetTestEncryptorForTesting());
};

TEST_P(ModelLoaderWithSecondayFileTest,
       LoadBookmarks_ShouldCreateSecondaryFilesWhenMissing) {
  base::HistogramTester histogram_tester;
  base::test::TaskEnvironment task_environment{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  const base::FilePath primary_local_or_syncable_file_path =
      CreateTempEncryptedFileOrClearTextCopy(
          /*primary_content=*/true,
          /*existing_file_name=*/"bookmarks/model_with_sync_metadata_1.json",
          /*new_file_name=*/"TestBookmarks1");
  const base::FilePath secondary_local_or_syncable_file_path =
      GetTestDataDir().AppendASCII("bookmarks/missing_file_1.json");
  const base::FilePath primary_account_file_path =
      CreateTempEncryptedFileOrClearTextCopy(
          /*primary_content=*/true,
          /*existing_file_name=*/"bookmarks/model_with_sync_metadata_2.json",
          /*new_file_name=*/"TestBookmarks2");
  const base::FilePath secondary_account_file_path =
      GetTestDataDir().AppendASCII("bookmarks/missing_file_2.json");

  base::test::TestFuture<StorageFileEncryptionType, std::string>
      save_local_or_syncable_bookmark_future;
  base::test::TestFuture<StorageFileEncryptionType, std::string>
      save_account_bookmark_future;
  scoped_refptr<ModelLoader> loader =
      CreateModelLoader(primary_local_or_syncable_file_path,
                        secondary_local_or_syncable_file_path,
                        primary_account_file_path, secondary_account_file_path,
                        save_local_or_syncable_bookmark_future.GetCallback(),
                        save_account_bookmark_future.GetCallback());

  task_environment.FastForwardUntilNoTasksRemain();

  VerifyPrimaryLoadCorrectlySecondaryHasGivenResult(
      histogram_tester,
      /*secondary_result=*/metrics::BookmarksFileLoadResult::kFileMissing);
  // Verify that the save encrypted file callback is called for both files.
  EXPECT_EQ(
      GetSecondaryStorageFileEncryptionType(),
      save_local_or_syncable_bookmark_future.Get<StorageFileEncryptionType>());
  EXPECT_EQ(GetSecondaryStorageFileEncryptionType(),
            save_account_bookmark_future.Get<StorageFileEncryptionType>());
}

TEST_P(ModelLoaderWithSecondayFileTest,
       LoadBookmarks_ShouldReportContentMismatch) {
  base::HistogramTester histogram_tester;
  base::test::TaskEnvironment task_environment{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  const base::FilePath primary_local_or_syncable_file_path =
      CreateTempEncryptedFileOrClearTextCopy(
          /*primary_content=*/true,
          /*existing_file_name=*/"bookmarks/model_with_sync_metadata_1.json",
          /*new_file_name=*/"TestBookmarks1");
  const base::FilePath secondary_local_or_syncable_file_path =
      CreateTempEncryptedFileOrClearTextCopy(
          /*primary_content=*/false,
          /*existing_file_name=*/"bookmarks/model_with_sync_metadata_2.json",
          /*new_file_name=*/"TestBookmarks2");
  const base::FilePath primary_account_file_path =
      CreateTempEncryptedFileOrClearTextCopy(
          /*primary_content=*/true,
          /*existing_file_name=*/"bookmarks/model_with_sync_metadata_2.json",
          /*new_file_name=*/"TestBookmarks2");
  const base::FilePath secondary_account_file_path =
      CreateTempEncryptedFileOrClearTextCopy(
          /*primary_content=*/false,
          /*existing_file_name=*/"bookmarks/model_with_sync_metadata_1.json",
          /*new_file_name=*/"TestBookmarks1");

  base::test::TestFuture<StorageFileEncryptionType, std::string>
      save_local_or_syncable_bookmark_future;
  base::test::TestFuture<StorageFileEncryptionType, std::string>
      save_account_bookmark_future;
  scoped_refptr<ModelLoader> loader =
      CreateModelLoader(primary_local_or_syncable_file_path,
                        secondary_local_or_syncable_file_path,
                        primary_account_file_path, secondary_account_file_path,
                        save_local_or_syncable_bookmark_future.GetCallback(),
                        save_account_bookmark_future.GetCallback());

  task_environment.FastForwardUntilNoTasksRemain();

  VerifyPrimaryLoadCorrectlySecondaryHasGivenResult(
      histogram_tester,
      /*secondary_result=*/metrics::BookmarksFileLoadResult::kSuccess);
  histogram_tester.ExpectUniqueSample(
      base::StrCat(
          {kEncryptedBookmarksFileMatchesResultMetricName, ".LocalOrSyncable"}),
      false,
      /*expected_bucket_count=*/1);
  histogram_tester.ExpectUniqueSample(
      base::StrCat(
          {kEncryptedBookmarksFileMatchesResultMetricName, ".Account"}),
      false,
      /*expected_bucket_count=*/1);
  // Verify that saving of the encrypted files is scheduled.
  EXPECT_EQ(
      GetSecondaryStorageFileEncryptionType(),
      save_local_or_syncable_bookmark_future.Get<StorageFileEncryptionType>());
  EXPECT_EQ(GetSecondaryStorageFileEncryptionType(),
            save_account_bookmark_future.Get<StorageFileEncryptionType>());

  EXPECT_THAT(GetClearTextFilePath(primary_local_or_syncable_file_path,
                                   secondary_local_or_syncable_file_path),
              FileAndBackupFileExist());
  EXPECT_THAT(GetClearTextFilePath(primary_account_file_path,
                                   secondary_account_file_path),
              FileAndBackupFileExist());
}

TEST_P(ModelLoaderWithSecondayFileTest,
       LoadBookmarks_ShouldReportContentMatches) {
  base::HistogramTester histogram_tester;
  base::test::TaskEnvironment task_environment{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  const base::FilePath primary_local_or_syncable_file_path =
      CreateTempEncryptedFileOrClearTextCopy(
          /*primary_content=*/true,
          /*existing_file_name=*/"bookmarks/model_with_sync_metadata_1.json",
          /*new_file_name=*/"TestBookmarks1");
  const base::FilePath secondary_local_or_syncable_file_path =
      CreateTempEncryptedFileOrClearTextCopy(
          /*primary_content=*/false,
          /*existing_file_name=*/"bookmarks/model_with_sync_metadata_1.json",
          /*new_file_name=*/"TestBookmarks1");
  const base::FilePath primary_account_file_path =
      CreateTempEncryptedFileOrClearTextCopy(
          /*primary_content=*/true,
          /*existing_file_name=*/"bookmarks/model_with_sync_metadata_2.json",
          /*new_file_name=*/"TestBookmarks2");
  const base::FilePath secondary_account_file_path =
      CreateTempEncryptedFileOrClearTextCopy(
          /*primary_content=*/false,
          /*existing_file_name=*/"bookmarks/model_with_sync_metadata_2.json",
          /*new_file_name=*/"TestBookmarks2");

  base::test::TestFuture<StorageFileEncryptionType, std::string>
      save_local_or_syncable_bookmark_future;
  base::test::TestFuture<StorageFileEncryptionType, std::string>
      save_account_bookmark_future;
  base::test::TestFuture<std::unique_ptr<BookmarkLoadDetails>> details_future;
  scoped_refptr<ModelLoader> loader =
      CreateModelLoader(primary_local_or_syncable_file_path,
                        secondary_local_or_syncable_file_path,
                        primary_account_file_path, secondary_account_file_path,
                        save_local_or_syncable_bookmark_future.GetCallback(),
                        save_account_bookmark_future.GetCallback());

  task_environment.FastForwardUntilNoTasksRemain();

  VerifyPrimaryLoadCorrectlySecondaryHasGivenResult(
      histogram_tester,
      /*secondary_result=*/metrics::BookmarksFileLoadResult::kSuccess);
  histogram_tester.ExpectUniqueSample(
      base::StrCat(
          {kEncryptedBookmarksFileMatchesResultMetricName, ".LocalOrSyncable"}),
      true,
      /*expected_bucket_count=*/1);
  histogram_tester.ExpectUniqueSample(
      base::StrCat(
          {kEncryptedBookmarksFileMatchesResultMetricName, ".Account"}),
      true,
      /*expected_bucket_count=*/1);
  // Verify that the save encrypted file callback hasn't been called.
  EXPECT_FALSE(save_local_or_syncable_bookmark_future.IsReady());
  EXPECT_FALSE(save_account_bookmark_future.IsReady());

  EXPECT_THAT(GetClearTextFilePath(primary_local_or_syncable_file_path,
                                   secondary_local_or_syncable_file_path),
              FileAndBackupFileExist());
  EXPECT_THAT(GetClearTextFilePath(primary_account_file_path,
                                   secondary_account_file_path),
              FileAndBackupFileExist());
}

TEST_P(ModelLoaderWithSecondayFileTest,
       LoadBookmarks_ShouldCreateSecondaryForAccountNodeWhenOnlyOneMissing) {
  base::HistogramTester histogram_tester;
  base::test::TaskEnvironment task_environment{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  const base::FilePath primary_local_or_syncable_file_path =
      CreateTempEncryptedFileOrClearTextCopy(
          /*primary_content=*/true,
          /*existing_file_name=*/"bookmarks/model_with_sync_metadata_1.json",
          /*new_file_name=*/"TestBookmarks1");
  const base::FilePath secondary_local_or_syncable_file_path =
      CreateTempEncryptedFileOrClearTextCopy(
          /*primary_content=*/false,
          /*existing_file_name=*/"bookmarks/model_with_sync_metadata_1.json",
          /*new_file_name=*/"TestBookmarks1");
  const base::FilePath primary_account_file_path =
      CreateTempEncryptedFileOrClearTextCopy(
          /*primary_content=*/true,
          /*existing_file_name=*/"bookmarks/model_with_sync_metadata_2.json",
          /*new_file_name=*/"TestBookmarks2");
  const base::FilePath secondary_account_file_path =
      GetTestDataDir().AppendASCII("bookmarks/missing_file_2.json");

  base::test::TestFuture<StorageFileEncryptionType, std::string>
      save_local_or_syncable_bookmark_future;
  base::test::TestFuture<StorageFileEncryptionType, std::string>
      save_account_bookmark_future;
  scoped_refptr<ModelLoader> loader =
      CreateModelLoader(primary_local_or_syncable_file_path,
                        secondary_local_or_syncable_file_path,
                        primary_account_file_path, secondary_account_file_path,
                        save_local_or_syncable_bookmark_future.GetCallback(),
                        save_account_bookmark_future.GetCallback());

  task_environment.FastForwardUntilNoTasksRemain();

  // Local or syncable reads succeed
  histogram_tester.ExpectUniqueSample(
      base::StrCat({kBookmarksFileLoadResultMetricName, ".LocalOrSyncable",
                    GetSecondaryEncryptionHistogramSuffix()}),
      metrics::BookmarksFileLoadResult::kSuccess,
      /*expected_bucket_count=*/1);
  histogram_tester.ExpectUniqueSample(
      base::StrCat(
          {kEncryptedBookmarksFileMatchesResultMetricName, ".LocalOrSyncable"}),
      true,
      /*expected_bucket_count=*/1);
  EXPECT_FALSE(save_local_or_syncable_bookmark_future.IsReady());

  // Account reads fail
  histogram_tester.ExpectUniqueSample(
      base::StrCat({kBookmarksFileLoadResultMetricName, ".Account",
                    GetSecondaryEncryptionHistogramSuffix()}),
      metrics::BookmarksFileLoadResult::kFileMissing,
      /*expected_bucket_count=*/1);
  EXPECT_EQ(save_account_bookmark_future.Get<StorageFileEncryptionType>(),
            GetSecondaryStorageFileEncryptionType());
}

TEST_P(ModelLoaderWithSecondayFileTest,
       LoadBookmarks_ShouldRecordSizeAndReadTimeForSecondaryFiles) {
  base::HistogramTester histogram_tester;
  base::test::TaskEnvironment task_environment{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  const base::FilePath primary_local_or_syncable_file_path =
      CreateTempEncryptedFileOrClearTextCopy(
          /*primary_content=*/true,
          /*existing_file_name=*/"bookmarks/model_with_sync_metadata_1.json",
          /*new_file_name=*/"TestBookmarks1");
  const base::FilePath secondary_local_or_syncable_file_path =
      CreateTempEncryptedFileOrClearTextCopy(
          /*primary_content=*/false,
          /*existing_file_name=*/"bookmarks/model_with_sync_metadata_1.json",
          /*new_file_name=*/"TestBookmarks1");
  const base::FilePath primary_account_file_path =
      CreateTempEncryptedFileOrClearTextCopy(
          /*primary_content=*/true,
          /*existing_file_name=*/"bookmarks/model_with_sync_metadata_2.json",
          /*new_file_name=*/"TestBookmarks2");
  const base::FilePath secondary_account_file_path =
      CreateTempEncryptedFileOrClearTextCopy(
          /*primary_content=*/false,
          /*existing_file_name=*/"bookmarks/model_with_sync_metadata_2.json",
          /*new_file_name=*/"TestBookmarks2");

  base::test::TestFuture<StorageFileEncryptionType, std::string>
      save_local_or_syncable_bookmark_future;
  base::test::TestFuture<StorageFileEncryptionType, std::string>
      save_account_bookmark_future;
  scoped_refptr<ModelLoader> loader =
      CreateModelLoader(primary_local_or_syncable_file_path,
                        secondary_local_or_syncable_file_path,
                        primary_account_file_path, secondary_account_file_path,
                        save_local_or_syncable_bookmark_future.GetCallback(),
                        save_account_bookmark_future.GetCallback());

  task_environment.FastForwardUntilNoTasksRemain();

  histogram_tester.ExpectTotalCount(
      kBookmarksStorageFileSizeAtStartupMetricName,
      /*expected_count=*/2);
  histogram_tester.ExpectTotalCount(
      base::StrCat({kBookmarksAverageNodeSizeMetricName, ".ClearText"}),
      /*expected_count=*/1);
  histogram_tester.ExpectTotalCount(
      kBookmarksEncryptedStorageFileSizeAtStartupMetricName,
      /*expected_count=*/2);
  histogram_tester.ExpectTotalCount(
      base::StrCat({kBookmarksAverageNodeSizeMetricName, ".Encrypted"}),
      /*expected_count=*/1);
  histogram_tester.ExpectTotalCount(
      base::StrCat({kBookmarksTimeToReadFileMetricName, ".LocalOrSyncable",
                    ".ClearText"}),
      /*expected_count=*/1);
  histogram_tester.ExpectTotalCount(
      base::StrCat({kBookmarksTimeToReadFileMetricName, ".LocalOrSyncable",
                    ".Encrypted"}),
      /*expected_count=*/1);
  histogram_tester.ExpectTotalCount(
      base::StrCat(
          {kBookmarksTimeToReadFileMetricName, ".Account", ".ClearText"}),
      /*expected_count=*/1);
  histogram_tester.ExpectTotalCount(
      base::StrCat(
          {kBookmarksTimeToReadFileMetricName, ".Account", ".Encrypted"}),
      /*expected_count=*/1);
}

TEST_P(ModelLoaderWithSecondayFileTest,
       LoadBookmarks_ShouldNotCreateSecondaryFilesWhenPrimaryFilesAreMissing) {
  base::HistogramTester histogram_tester;
  base::test::TaskEnvironment task_environment{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  const base::FilePath primary_local_or_syncable_file_path =
      GetTestDataDir().AppendASCII("bookmarks/missing_file_1.json");
  const base::FilePath secondary_local_or_syncable_file_path =
      GetTestDataDir().AppendASCII("bookmarks/missing_file_2.json");
  const base::FilePath primary_account_file_path =
      GetTestDataDir().AppendASCII("bookmarks/missing_file_3.json");
  const base::FilePath secondary_account_file_path =
      GetTestDataDir().AppendASCII("bookmarks/missing_file_4.json");

  base::test::TestFuture<StorageFileEncryptionType, std::string>
      save_local_or_syncable_bookmark_future;
  base::test::TestFuture<StorageFileEncryptionType, std::string>
      save_account_bookmark_future;
  scoped_refptr<ModelLoader> loader =
      CreateModelLoader(primary_local_or_syncable_file_path,
                        secondary_local_or_syncable_file_path,
                        primary_account_file_path, secondary_account_file_path,
                        save_local_or_syncable_bookmark_future.GetCallback(),
                        save_account_bookmark_future.GetCallback());

  task_environment.FastForwardUntilNoTasksRemain();

  EXPECT_FALSE(save_local_or_syncable_bookmark_future.IsReady());
  EXPECT_FALSE(save_account_bookmark_future.IsReady());

  // We tried to read the primary files but fail.
  histogram_tester.ExpectUniqueSample(
      base::StrCat({kBookmarksFileLoadResultMetricName, ".LocalOrSyncable",
                    GetPrimaryEncryptionHistogramSuffix()}),
      metrics::BookmarksFileLoadResult::kFileMissing,
      /*expected_bucket_count=*/1);
  histogram_tester.ExpectUniqueSample(
      base::StrCat({kBookmarksFileLoadResultMetricName, ".Account",
                    GetPrimaryEncryptionHistogramSuffix()}),
      metrics::BookmarksFileLoadResult::kFileMissing,
      /*expected_bucket_count=*/1);

  // We never try to read the secondary file.
  histogram_tester.ExpectTotalCount(
      base::StrCat({kBookmarksFileLoadResultMetricName, ".LocalOrSyncable",
                    GetSecondaryEncryptionHistogramSuffix()}),
      /*expected_count=*/0);
  histogram_tester.ExpectTotalCount(
      base::StrCat({kBookmarksFileLoadResultMetricName, ".Account",
                    GetSecondaryEncryptionHistogramSuffix()}),
      /*expected_count=*/0);
}

INSTANTIATE_TEST_SUITE_P(
    ModelLoaderWithSecondayFileTest,
    ModelLoaderWithSecondayFileTest,
    testing::Values(BookmarkEncryptionStage::kWriteBothReadOnlyClear,
                    BookmarkEncryptionStage::kWriteBothReadPreferEncrypted));

TEST(ModelLoaderTest, LoadBookmarks_ShouldReportDecryptionFailed) {
  base::test::ScopedFeatureList features;
  test::InitFeaturesForBookmarkTestEncryptionStage(
      features, BookmarkEncryptionStage::kWriteBothReadOnlyClear);
  base::HistogramTester histogram_tester;
  base::test::TaskEnvironment task_environment{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  scoped_refptr<base::RefCountedData<const os_crypt_async::Encryptor>>
      encryptor = base::MakeRefCounted<
          base::RefCountedData<const os_crypt_async::Encryptor>>(
          std::in_place, os_crypt_async::GetTestEncryptorForTesting());
  const base::FilePath local_or_syncable_file_path = CreateCopyWithBackup(
      GetTestDataDir().AppendASCII("bookmarks/model_with_sync_metadata_1.json"),
      "Bookmarks");
  const base::FilePath account_file_path = CreateCopyWithBackup(
      GetTestDataDir().AppendASCII("bookmarks/model_with_sync_metadata_2.json"),
      "AccountBookmarks");
  // These files aren't encrypted, so they will will fail decryption.
  const base::FilePath encrypted_local_or_syncable_file_path =
      GetTestDataDir().AppendASCII("bookmarks/model_with_sync_metadata_1.json");
  const base::FilePath encrypted_account_file_path =
      GetTestDataDir().AppendASCII("bookmarks/model_with_sync_metadata_2.json");

  base::test::TestFuture<StorageFileEncryptionType, std::string>
      save_local_or_syncable_bookmark_future;
  base::test::TestFuture<StorageFileEncryptionType, std::string>
      save_account_bookmark_future;
  base::test::TestFuture<std::unique_ptr<BookmarkLoadDetails>> details_future;
  scoped_refptr<ModelLoader> loader = ModelLoader::Create(
      encryptor, local_or_syncable_file_path,
      encrypted_local_or_syncable_file_path, account_file_path,
      encrypted_account_file_path, LoadManagedNodeCallback(),
      save_local_or_syncable_bookmark_future.GetCallback(),
      save_account_bookmark_future.GetCallback(), details_future.GetCallback());

  task_environment.FastForwardUntilNoTasksRemain();

  VerifyPrimaryLoadCorrectlySecondaryHasGivenResult(
      histogram_tester, /*primary_histogram_suffix=*/".ClearText",
      /*secondary_histogram_suffix=*/".Encrypted",
      /*secondary_result=*/metrics::BookmarksFileLoadResult::kDecryptionFailed);
  // Verify that saving of the encrypted files is scheduled.
  EXPECT_EQ(
      save_local_or_syncable_bookmark_future.Get<StorageFileEncryptionType>(),
      StorageFileEncryptionType::kEncrypted);
  EXPECT_EQ(save_account_bookmark_future.Get<StorageFileEncryptionType>(),
            StorageFileEncryptionType::kEncrypted);

  EXPECT_THAT(local_or_syncable_file_path, FileAndBackupFileExist());
  EXPECT_THAT(account_file_path, FileAndBackupFileExist());
}

class ModelLoaderWithEncryptionFileAsPrimaryTest
    : public testing::TestWithParam<BookmarkEncryptionStage> {
 protected:
  ModelLoaderWithEncryptionFileAsPrimaryTest() {
    test::InitFeaturesForBookmarkTestEncryptionStage(feature_list_, GetParam());
  }
  base::test::ScopedFeatureList feature_list_;
};

TEST_P(ModelLoaderWithEncryptionFileAsPrimaryTest,
       LoadBookmarks_ShouldLoadDetailsCorrectlyFromEncryptedFiles) {
  base::HistogramTester histogram_tester;
  base::test::TaskEnvironment task_environment{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  scoped_refptr<base::RefCountedData<const os_crypt_async::Encryptor>>
      encryptor = base::MakeRefCounted<
          base::RefCountedData<const os_crypt_async::Encryptor>>(
          std::in_place, os_crypt_async::GetTestEncryptorForTesting());
  const base::FilePath local_or_syncable_file_path = CreateCopyWithBackup(
      GetTestDataDir().AppendASCII("bookmarks/model_with_sync_metadata_1.json"),
      "Bookmarks");
  const base::FilePath encrypted_local_or_syncable_file_path =
      CreateTempEncryptedFile(GetTestDataDir().AppendASCII(
                                  "bookmarks/model_with_sync_metadata_1.json"),
                              "EncryptedBookmarks", encryptor)
          .value();
  const base::FilePath account_file_path = CreateCopyWithBackup(
      GetTestDataDir().AppendASCII("bookmarks/model_with_sync_metadata_2.json"),
      "AccountBookmarks");
  const base::FilePath encrypted_account_file_path =
      CreateTempEncryptedFile(GetTestDataDir().AppendASCII(
                                  "bookmarks/model_with_sync_metadata_2.json"),
                              "EncryptedAccountBookmarks", encryptor)
          .value();

  base::test::TestFuture<StorageFileEncryptionType, std::string>
      save_local_or_syncable_bookmark_future;
  base::test::TestFuture<StorageFileEncryptionType, std::string>
      save_account_bookmark_future;
  base::test::TestFuture<std::unique_ptr<BookmarkLoadDetails>> details_future;
  scoped_refptr<ModelLoader> loader = ModelLoader::Create(
      encryptor, local_or_syncable_file_path,
      encrypted_local_or_syncable_file_path, account_file_path,
      encrypted_account_file_path, LoadManagedNodeCallback(),
      save_local_or_syncable_bookmark_future.GetCallback(),
      save_account_bookmark_future.GetCallback(), details_future.GetCallback());

  task_environment.FastForwardUntilNoTasksRemain();

  // Clear text file is successfully used as primary file fallback.
  const std::unique_ptr<BookmarkLoadDetails> details = details_future.Take();
  ASSERT_NE(nullptr, details);
  EXPECT_EQ(1u, details->bb_node()->children().size());
  EXPECT_EQ(1u, details->other_folder_node()->children().size());
  EXPECT_EQ(1u, details->mobile_folder_node()->children().size());
  EXPECT_EQ(1u, details->account_bb_node()->children().size());
  EXPECT_EQ(1u, details->account_other_folder_node()->children().size());
  EXPECT_EQ(1u, details->account_mobile_folder_node()->children().size());
}

TEST_P(ModelLoaderWithEncryptionFileAsPrimaryTest,
       LoadBookmarks_ShouldSaveEncryptedFileIfMissingAndFallBackToClearText) {
  base::HistogramTester histogram_tester;
  base::test::TaskEnvironment task_environment{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  scoped_refptr<base::RefCountedData<const os_crypt_async::Encryptor>>
      encryptor = base::MakeRefCounted<
          base::RefCountedData<const os_crypt_async::Encryptor>>(
          std::in_place, os_crypt_async::GetTestEncryptorForTesting());
  const base::FilePath local_or_syncable_file_path = CreateCopyWithBackup(
      GetTestDataDir().AppendASCII("bookmarks/model_with_sync_metadata_1.json"),
      "Bookmarks");
  const base::FilePath account_file_path = CreateCopyWithBackup(
      GetTestDataDir().AppendASCII("bookmarks/model_with_sync_metadata_2.json"),
      "AccountBookmarks");
  const base::FilePath encrypted_local_or_syncable_file_path =
      GetTestDataDir().AppendASCII("bookmarks/missing_file_1.json");
  const base::FilePath encrypted_account_file_path =
      GetTestDataDir().AppendASCII("bookmarks/missing_file_2.json");

  base::test::TestFuture<StorageFileEncryptionType, std::string>
      save_local_or_syncable_bookmark_future;
  base::test::TestFuture<StorageFileEncryptionType, std::string>
      save_account_bookmark_future;
  base::test::TestFuture<std::unique_ptr<BookmarkLoadDetails>> details_future;
  scoped_refptr<ModelLoader> loader = ModelLoader::Create(
      encryptor, local_or_syncable_file_path,
      encrypted_local_or_syncable_file_path, account_file_path,
      encrypted_account_file_path, LoadManagedNodeCallback(),
      save_local_or_syncable_bookmark_future.GetCallback(),
      save_account_bookmark_future.GetCallback(), details_future.GetCallback());

  task_environment.FastForwardUntilNoTasksRemain();

  // Clear text file is successfully used as primary file fallback.
  const std::unique_ptr<BookmarkLoadDetails> details = details_future.Take();
  ASSERT_NE(nullptr, details);
  EXPECT_EQ(1u, details->bb_node()->children().size());
  EXPECT_EQ(1u, details->other_folder_node()->children().size());
  EXPECT_EQ(1u, details->mobile_folder_node()->children().size());
  EXPECT_EQ(1u, details->account_bb_node()->children().size());
  EXPECT_EQ(1u, details->account_other_folder_node()->children().size());
  EXPECT_EQ(1u, details->account_mobile_folder_node()->children().size());

  histogram_tester.ExpectUniqueSample(
      base::StrCat(
          {kFallbackToClearTextFileOnLoadResultMetricName, ".LocalOrSyncable"}),
      metrics::BookmarksFileLoadResult::kSuccess,
      /*expected_bucket_count=*/1);
  histogram_tester.ExpectUniqueSample(
      base::StrCat(
          {kFallbackToClearTextFileOnLoadResultMetricName, ".Account"}),
      metrics::BookmarksFileLoadResult::kSuccess,
      /*expected_bucket_count=*/1);
  // Encrypted file isn't read.
  histogram_tester.ExpectTotalCount(
      base::StrCat({kBookmarksFileLoadResultMetricName, ".LocalOrSyncable",
                    ".Encrypted"}),
      /*expected_count=*/0);
  histogram_tester.ExpectTotalCount(
      base::StrCat(
          {kBookmarksFileLoadResultMetricName, ".Account", ".Encrypted"}),
      /*expected_count=*/0);
  // Verify that the save encrypted file callback is called for both files.
  EXPECT_EQ(
      StorageFileEncryptionType::kEncrypted,
      save_local_or_syncable_bookmark_future.Get<StorageFileEncryptionType>());
  EXPECT_EQ(StorageFileEncryptionType::kEncrypted,
            save_account_bookmark_future.Get<StorageFileEncryptionType>());

  EXPECT_THAT(local_or_syncable_file_path, FileAndBackupFileExist());
  EXPECT_THAT(account_file_path, FileAndBackupFileExist());
}

TEST_P(ModelLoaderWithEncryptionFileAsPrimaryTest,
       LoadBookmarks_ShouldNotSaveEncryptedFileEvenIfClearTextFallbackFails) {
  base::HistogramTester histogram_tester;
  base::test::TaskEnvironment task_environment{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  scoped_refptr<base::RefCountedData<const os_crypt_async::Encryptor>>
      encryptor = base::MakeRefCounted<
          base::RefCountedData<const os_crypt_async::Encryptor>>(
          std::in_place, os_crypt_async::GetTestEncryptorForTesting());
  const base::FilePath local_or_syncable_file_path = CreateCopyWithBackup(
      GetTestDataDir().AppendASCII("bookmarks/model_invalid_json.json"),
      "Bookmarks");
  const base::FilePath account_file_path = CreateCopyWithBackup(
      GetTestDataDir().AppendASCII("bookmarks/model_invalid_json.json"),
      "AccountBookmarks");
  const base::FilePath encrypted_local_or_syncable_file_path =
      GetTestDataDir().AppendASCII("bookmarks/missing_file_1.json");
  const base::FilePath encrypted_account_file_path =
      GetTestDataDir().AppendASCII("bookmarks/missing_file_2.json");

  base::test::TestFuture<StorageFileEncryptionType, std::string>
      save_local_or_syncable_bookmark_future;
  base::test::TestFuture<StorageFileEncryptionType, std::string>
      save_account_bookmark_future;
  base::test::TestFuture<std::unique_ptr<BookmarkLoadDetails>> details_future;
  scoped_refptr<ModelLoader> loader = ModelLoader::Create(
      encryptor, local_or_syncable_file_path,
      encrypted_local_or_syncable_file_path, account_file_path,
      encrypted_account_file_path, LoadManagedNodeCallback(),
      save_local_or_syncable_bookmark_future.GetCallback(),
      save_account_bookmark_future.GetCallback(), details_future.GetCallback());

  task_environment.FastForwardUntilNoTasksRemain();

  // Clear text file fails to load.
  const std::unique_ptr<BookmarkLoadDetails> details = details_future.Take();
  ASSERT_NE(nullptr, details);
  EXPECT_EQ(0u, details->bb_node()->children().size());
  EXPECT_EQ(0u, details->other_folder_node()->children().size());
  EXPECT_EQ(0u, details->mobile_folder_node()->children().size());
  EXPECT_EQ(nullptr, details->account_bb_node());
  EXPECT_EQ(nullptr, details->account_other_folder_node());
  EXPECT_EQ(nullptr, details->account_mobile_folder_node());

  histogram_tester.ExpectUniqueSample(
      base::StrCat(
          {kFallbackToClearTextFileOnLoadResultMetricName, ".LocalOrSyncable"}),
      metrics::BookmarksFileLoadResult::kJSONParsingFailed,
      /*expected_bucket_count=*/1);
  histogram_tester.ExpectUniqueSample(
      base::StrCat(
          {kFallbackToClearTextFileOnLoadResultMetricName, ".Account"}),
      metrics::BookmarksFileLoadResult::kJSONParsingFailed,
      /*expected_bucket_count=*/1);
  // Encrypted file isn't read.
  histogram_tester.ExpectTotalCount(
      base::StrCat({kBookmarksFileLoadResultMetricName, ".LocalOrSyncable",
                    ".Encrypted"}),
      /*expected_count=*/0);
  histogram_tester.ExpectTotalCount(
      base::StrCat(
          {kBookmarksFileLoadResultMetricName, ".Account", ".Encrypted"}),
      /*expected_count=*/0);
  // Verify that the save encrypted file callback is not called for either file.
  EXPECT_FALSE(save_local_or_syncable_bookmark_future.IsReady());
  EXPECT_FALSE(save_account_bookmark_future.IsReady());

  EXPECT_THAT(local_or_syncable_file_path, FileAndBackupFileExist());
  EXPECT_THAT(account_file_path, FileAndBackupFileExist());
}

INSTANTIATE_TEST_SUITE_P(
    ModelLoaderWithEncryptionFileAsPrimaryTest,
    ModelLoaderWithEncryptionFileAsPrimaryTest,
    testing::Values(
        BookmarkEncryptionStage::kWriteBothReadPreferEncrypted,
        BookmarkEncryptionStage::kWriteOnlyEncryptedReadPreferEncrypted));

TEST(ModelLoaderWithEncryptionWriteOnly,
     LoadBookmarks_ShouldNeverLoadOrSaveClearTextFile) {
  base::test::ScopedFeatureList features;
  test::InitFeaturesForBookmarkTestEncryptionStage(
      features,
      BookmarkEncryptionStage::kWriteOnlyEncryptedReadPreferEncrypted);
  scoped_refptr<base::RefCountedData<const os_crypt_async::Encryptor>>
      encryptor = base::MakeRefCounted<
          base::RefCountedData<const os_crypt_async::Encryptor>>(
          std::in_place, os_crypt_async::GetTestEncryptorForTesting());
  base::HistogramTester histogram_tester;
  base::test::TaskEnvironment task_environment{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  const base::FilePath local_or_syncable_file_path =
      GetTestDataDir().AppendASCII("bookmarks/missing_file_1.json");
  const base::FilePath encrypted_local_or_syncable_file_path =
      CreateTempEncryptedFile(GetTestDataDir().AppendASCII(
                                  "bookmarks/model_with_sync_metadata_1.json"),
                              "TestEncryptedBookmarks1", encryptor)
          .value();
  const base::FilePath account_file_path =
      GetTestDataDir().AppendASCII("bookmarks/missing_file_2.json");
  const base::FilePath encrypted_account_file_path =
      CreateTempEncryptedFile(GetTestDataDir().AppendASCII(
                                  "bookmarks/model_with_sync_metadata_2.json"),
                              "TestEncryptedBookmarks2", encryptor)
          .value();

  base::test::TestFuture<StorageFileEncryptionType, std::string>
      save_local_or_syncable_bookmark_future;
  base::test::TestFuture<StorageFileEncryptionType, std::string>
      save_account_bookmark_future;
  base::test::TestFuture<std::unique_ptr<BookmarkLoadDetails>> details_future;
  scoped_refptr<ModelLoader> loader = ModelLoader::Create(
      encryptor, local_or_syncable_file_path,
      encrypted_local_or_syncable_file_path, account_file_path,
      encrypted_account_file_path, LoadManagedNodeCallback(),
      save_local_or_syncable_bookmark_future.GetCallback(),
      save_account_bookmark_future.GetCallback(),
      /*callback=*/details_future.GetCallback());

  task_environment.FastForwardUntilNoTasksRemain();

  // Encrypted file read succeeds for both local or syncable and account files.
  const std::unique_ptr<BookmarkLoadDetails> details = details_future.Take();
  ASSERT_NE(nullptr, details);
  EXPECT_EQ(1u, details->bb_node()->children().size());
  EXPECT_EQ(1u, details->other_folder_node()->children().size());
  EXPECT_EQ(1u, details->mobile_folder_node()->children().size());
  EXPECT_EQ(1u, details->account_bb_node()->children().size());
  EXPECT_EQ(1u, details->account_other_folder_node()->children().size());
  EXPECT_EQ(1u, details->account_mobile_folder_node()->children().size());
  histogram_tester.ExpectUniqueSample(
      base::StrCat({kBookmarksFileLoadResultMetricName, ".LocalOrSyncable",
                    ".Encrypted"}),
      metrics::BookmarksFileLoadResult::kSuccess,
      /*expected_bucket_count=*/1);
  histogram_tester.ExpectUniqueSample(
      base::StrCat(
          {kBookmarksFileLoadResultMetricName, ".Account", ".Encrypted"}),
      metrics::BookmarksFileLoadResult::kSuccess,
      /*expected_bucket_count=*/1);

  // No clear text file save are requested.
  EXPECT_FALSE(save_local_or_syncable_bookmark_future.IsReady());
  EXPECT_FALSE(save_account_bookmark_future.IsReady());
  // No clear text file load results are logged.
  histogram_tester.ExpectTotalCount(
      base::StrCat({kBookmarksFileLoadResultMetricName, ".LocalOrSyncable",
                    ".ClearText"}),
      /*expected_count=*/0);
  histogram_tester.ExpectTotalCount(
      base::StrCat(
          {kBookmarksFileLoadResultMetricName, ".Account", ".ClearText"}),
      /*expected_count=*/0);
  // No comparison with clear text file are logged.
  histogram_tester.ExpectTotalCount(
      base::StrCat(
          {kEncryptedBookmarksFileMatchesResultMetricName, ".LocalOrSyncable"}),
      /*expected_count=*/0);
  histogram_tester.ExpectTotalCount(
      base::StrCat(
          {kEncryptedBookmarksFileMatchesResultMetricName, ".Account"}),
      /*expected_count=*/0);
}

TEST(ModelLoaderWithEncryptionWriteOnly,
     LoadBookmarks_ShouldRecordOnlyEncryptedFileSizes) {
  base::test::ScopedFeatureList features;
  test::InitFeaturesForBookmarkTestEncryptionStage(
      features,
      BookmarkEncryptionStage::kWriteOnlyEncryptedReadPreferEncrypted);
  scoped_refptr<base::RefCountedData<const os_crypt_async::Encryptor>>
      encryptor = base::MakeRefCounted<
          base::RefCountedData<const os_crypt_async::Encryptor>>(
          std::in_place, os_crypt_async::GetTestEncryptorForTesting());
  base::HistogramTester histogram_tester;
  base::test::TaskEnvironment task_environment{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  const base::FilePath local_or_syncable_file_path =
      GetTestDataDir().AppendASCII("bookmarks/missing_file_1.json");
  const base::FilePath encrypted_local_or_syncable_file_path =
      CreateTempEncryptedFile(GetTestDataDir().AppendASCII(
                                  "bookmarks/model_with_sync_metadata_1.json"),
                              "TestEncryptedBookmarks1", encryptor)
          .value();
  const base::FilePath account_file_path =
      GetTestDataDir().AppendASCII("bookmarks/missing_file_2.json");
  const base::FilePath encrypted_account_file_path =
      CreateTempEncryptedFile(GetTestDataDir().AppendASCII(
                                  "bookmarks/model_with_sync_metadata_2.json"),
                              "TestEncryptedBookmarks2", encryptor)
          .value();

  base::test::TestFuture<StorageFileEncryptionType, std::string>
      save_local_or_syncable_bookmark_future;
  base::test::TestFuture<StorageFileEncryptionType, std::string>
      save_account_bookmark_future;
  scoped_refptr<ModelLoader> loader = ModelLoader::Create(
      encryptor, local_or_syncable_file_path,
      encrypted_local_or_syncable_file_path, account_file_path,
      encrypted_account_file_path, LoadManagedNodeCallback(),
      save_local_or_syncable_bookmark_future.GetCallback(),
      save_account_bookmark_future.GetCallback(),
      /*callback=*/base::DoNothing());

  task_environment.FastForwardUntilNoTasksRemain();

  // Only record encrypted file sizes
  histogram_tester.ExpectTotalCount(
      kBookmarksStorageFileSizeAtStartupMetricName,
      /*expected_count=*/0);
  histogram_tester.ExpectTotalCount(
      base::StrCat({kBookmarksAverageNodeSizeMetricName, ".ClearText"}),
      /*expected_count=*/0);
  histogram_tester.ExpectTotalCount(
      kBookmarksEncryptedStorageFileSizeAtStartupMetricName,
      /*expected_count=*/2);
  histogram_tester.ExpectTotalCount(
      base::StrCat({kBookmarksAverageNodeSizeMetricName, ".Encrypted"}),
      /*expected_count=*/1);
}

TEST(ModelLoaderWithEncryptionWriteOnly,
     LoadBookmarks_ShouldDeleteClearTextFilesWhenEncryptedReadSucceeds) {
  base::test::ScopedFeatureList features;
  test::InitFeaturesForBookmarkTestEncryptionStage(
      features,
      BookmarkEncryptionStage::kWriteOnlyEncryptedReadPreferEncrypted);
  scoped_refptr<base::RefCountedData<const os_crypt_async::Encryptor>>
      encryptor = base::MakeRefCounted<
          base::RefCountedData<const os_crypt_async::Encryptor>>(
          std::in_place, os_crypt_async::GetTestEncryptorForTesting());
  base::HistogramTester histogram_tester;
  base::test::TaskEnvironment task_environment{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  const base::FilePath local_or_syncable_file_path = CreateCopyWithBackup(
      GetTestDataDir().AppendASCII("bookmarks/model_with_sync_metadata_1.json"),
      "TestBookmarks1");
  const base::FilePath encrypted_local_or_syncable_file_path =
      CreateTempEncryptedFile(GetTestDataDir().AppendASCII(
                                  "bookmarks/model_with_sync_metadata_1.json"),
                              "TestEncryptedBookmarks1", encryptor)
          .value();
  const base::FilePath account_file_path = CreateCopyWithBackup(
      GetTestDataDir().AppendASCII("bookmarks/model_with_sync_metadata_2.json"),
      "TestBookmarks2");
  const base::FilePath encrypted_account_file_path =
      CreateTempEncryptedFile(GetTestDataDir().AppendASCII(
                                  "bookmarks/model_with_sync_metadata_2.json"),
                              "TestEncryptedBookmarks2", encryptor)
          .value();

  base::test::TestFuture<StorageFileEncryptionType, std::string>
      save_local_or_syncable_bookmark_future;
  base::test::TestFuture<StorageFileEncryptionType, std::string>
      save_account_bookmark_future;
  scoped_refptr<ModelLoader> loader = ModelLoader::Create(
      encryptor, local_or_syncable_file_path,
      encrypted_local_or_syncable_file_path, account_file_path,
      encrypted_account_file_path, LoadManagedNodeCallback(),
      save_local_or_syncable_bookmark_future.GetCallback(),
      save_account_bookmark_future.GetCallback(),
      /*callback=*/base::DoNothing());
  task_environment.FastForwardUntilNoTasksRemain();

  // Clear text files are deleted.
  EXPECT_FALSE(base::PathExists(local_or_syncable_file_path));
  EXPECT_FALSE(base::PathExists(account_file_path));
  EXPECT_FALSE(base::PathExists(
      local_or_syncable_file_path.ReplaceExtension(kBackupExtension)));
  EXPECT_FALSE(
      base::PathExists(account_file_path.ReplaceExtension(kBackupExtension)));
  histogram_tester.ExpectUniqueSample(
      base::StrCat({kBookmarksDeleteFileMetricName, ".LocalOrSyncable"}),
      /*sample=*/true,
      /*expected_bucket_count=*/1);
  histogram_tester.ExpectUniqueSample(
      base::StrCat({kBookmarksDeleteFileMetricName, ".Account"}),
      /*sample=*/true,
      /*expected_bucket_count=*/1);
  // Encrypted files still exist.
  EXPECT_TRUE(base::PathExists(encrypted_local_or_syncable_file_path));
  EXPECT_TRUE(base::PathExists(encrypted_account_file_path));
}

}  // namespace

}  // namespace bookmarks
