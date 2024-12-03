// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_bookmarks/bookmark_local_data_batch_uploader.h"

#include <memory>
#include <utility>

#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "components/bookmarks/test/test_bookmark_client.h"
#include "components/strings/grit/components_strings.h"
#include "components/sync/base/features.h"
#include "components/sync/service/local_data_description.h"
#include "components/sync_bookmarks/switches.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

namespace sync_bookmarks {
namespace {

using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::ExplainMatchResult;
using ::testing::Field;
using ::testing::IsEmpty;

// Checks whether the item matches a LocalDataItemModel for a bookmark with the
// given title.
MATCHER_P2(MatchesFolderDataItem, title, bookmark_count, "") {
  return ExplainMatchResult(
             Field(&syncer::LocalDataItemModel::title, Eq(title)), arg,
             result_listener) &&
         ExplainMatchResult(Field(&syncer::LocalDataItemModel::subtitle,
                                  Eq(l10n_util::GetPluralStringFUTF8(
                                      IDS_BULK_UPLOAD_BOOKMARK_FOLDER_SUBTITLE,
                                      bookmark_count))),
                            arg, result_listener);
}

// Checks whether the item matches a LocalDataItemModel for a bookmark with the
// given title.
MATCHER_P(MatchesBookmarkDataItem, title, "") {
  return ExplainMatchResult(
             Field(&syncer::LocalDataItemModel::title, Eq(title)), arg,
             result_listener) &&
         ExplainMatchResult(
             Field(&syncer::LocalDataItemModel::subtitle, IsEmpty()), arg,
             result_listener);
}

MATCHER_P2(MatchesTitleAndUrl, title, url, "") {
  if (!arg->is_url()) {
    *result_listener << "Expected URL bookmark but got folder.";
    return false;
  }
  if (arg->GetTitle() != title) {
    *result_listener << "Expected URL title \"" << title << "\" but got \""
                     << arg->GetTitle() << "\"";
    return false;
  }
  if (arg->url() != url) {
    *result_listener << "Expected URL \"" << url << "\" but got \""
                     << arg->url() << "\"";
    return false;
  }
  return true;
}

MATCHER(IsEmptyDescription, "") {
  return ExplainMatchResult(
             Field(&syncer::LocalDataDescription::local_data_models, IsEmpty()),
             arg, result_listener) &&
         ExplainMatchResult(
             Field(&syncer::LocalDataDescription::item_count, Eq(0u)), arg,
             result_listener) &&
         ExplainMatchResult(
             Field(&syncer::LocalDataDescription::domains, IsEmpty()), arg,
             result_listener) &&
         ExplainMatchResult(
             Field(&syncer::LocalDataDescription::domain_count, Eq(0u)), arg,
             result_listener);
}

class BookmarkLocalDataBatchUploaderTest : public ::testing::Test {
 public:
  BookmarkLocalDataBatchUploaderTest() = default;

  ~BookmarkLocalDataBatchUploaderTest() override = default;

  bookmarks::BookmarkModel* bookmark_model() { return bookmark_model_.get(); }

 private:
  base::test::ScopedFeatureList feature_list_{
      syncer::kSyncEnableBookmarksInTransportMode};
  const std::unique_ptr<bookmarks::BookmarkModel> bookmark_model_ =
      std::make_unique<bookmarks::BookmarkModel>(
          std::make_unique<bookmarks::TestBookmarkClient>());
};

TEST_F(BookmarkLocalDataBatchUploaderTest, LocalDescriptionEmptyIfNullModel) {
  BookmarkLocalDataBatchUploader uploader(nullptr);
  base::test::TestFuture<syncer::LocalDataDescription> description;

  uploader.GetLocalDataDescription(description.GetCallback());

  EXPECT_THAT(description.Get(), IsEmptyDescription());
}

TEST_F(BookmarkLocalDataBatchUploaderTest,
       LocalDescriptionEmptyIfModelNotLoaded) {
  BookmarkLocalDataBatchUploader uploader(bookmark_model());
  base::test::TestFuture<syncer::LocalDataDescription> description;

  uploader.GetLocalDataDescription(description.GetCallback());

  EXPECT_THAT(description.Get(), IsEmptyDescription());
}

TEST_F(BookmarkLocalDataBatchUploaderTest,
       LocalDescriptionEmptyIfTransportModeOff) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      syncer::kSyncEnableBookmarksInTransportMode);
  bookmark_model()->LoadEmptyForTest();
  BookmarkLocalDataBatchUploader uploader(bookmark_model());
  base::test::TestFuture<syncer::LocalDataDescription> description;

  uploader.GetLocalDataDescription(description.GetCallback());

  EXPECT_THAT(description.Get(), IsEmptyDescription());
}

TEST_F(BookmarkLocalDataBatchUploaderTest,
       LocalDescriptionEmptyIfNoAccountFolders) {
  bookmark_model()->LoadEmptyForTest();
  ASSERT_FALSE(bookmark_model()->account_bookmark_bar_node());
  bookmark_model()->AddURL(bookmark_model()->bookmark_bar_node(), /*index=*/0,
                           u"Local", GURL("http://local.com/"));
  BookmarkLocalDataBatchUploader uploader(bookmark_model());
  base::test::TestFuture<syncer::LocalDataDescription> description;

  uploader.GetLocalDataDescription(description.GetCallback());

  EXPECT_THAT(description.Get(), IsEmptyDescription());
}

TEST_F(BookmarkLocalDataBatchUploaderTest, LocalDescriptionOnlyHasLocalData) {
  bookmark_model()->LoadEmptyForTest();
  bookmark_model()->CreateAccountPermanentFolders();
  const bookmarks::BookmarkNode* local_node = bookmark_model()->AddURL(
      bookmark_model()->bookmark_bar_node(), /*index=*/0, u"Local",
      GURL("http://local.com/"));
  bookmark_model()->AddURL(bookmark_model()->account_bookmark_bar_node(),
                           /*index=*/0, u"Account",
                           GURL("http://account.com/"));
  BookmarkLocalDataBatchUploader uploader(bookmark_model());
  base::test::TestFuture<syncer::LocalDataDescription> description;

  uploader.GetLocalDataDescription(description.GetCallback());

  EXPECT_EQ(description.Get().local_data_models.size(), 1u);
  auto item = description.Get().local_data_models[0];
  EXPECT_EQ(std::get<int64_t>(item.id), local_node->id());
  EXPECT_EQ(item.title, "Local");
  EXPECT_THAT(item.subtitle, IsEmpty());
  EXPECT_EQ(item.icon_url, GURL());

  EXPECT_EQ(description.Get().item_count, 1u);
  EXPECT_EQ(description.Get().domain_count, 1u);
  EXPECT_THAT(description.Get().domains, ElementsAre("local.com"));
}

TEST_F(BookmarkLocalDataBatchUploaderTest,
       LocalDescriptionEmptyItemsWhenFeatureDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      switches::kSyncBookmarksBatchUploadSelectedItems);
  bookmark_model()->LoadEmptyForTest();
  bookmark_model()->CreateAccountPermanentFolders();
  bookmark_model()->AddURL(bookmark_model()->bookmark_bar_node(), /*index=*/0,
                           u"Local", GURL("http://local.com/"));
  bookmark_model()->AddURL(bookmark_model()->account_bookmark_bar_node(),
                           /*index=*/0, u"Account",
                           GURL("http://account.com/"));
  BookmarkLocalDataBatchUploader uploader(bookmark_model());
  base::test::TestFuture<syncer::LocalDataDescription> description;

  uploader.GetLocalDataDescription(description.GetCallback());

  EXPECT_THAT(description.Get().local_data_models, IsEmpty());

  EXPECT_EQ(description.Get().item_count, 1u);
  EXPECT_EQ(description.Get().domain_count, 1u);
  EXPECT_THAT(description.Get().domains, ElementsAre("local.com"));
}

TEST_F(BookmarkLocalDataBatchUploaderTest,
       LocalDescriptionTopLevelEmptyFolder) {
  bookmark_model()->LoadEmptyForTest();
  bookmark_model()->CreateAccountPermanentFolders();
  const bookmarks::BookmarkNode* folder = bookmark_model()->AddFolder(
      bookmark_model()->bookmark_bar_node(), /*index=*/0, u"folder");
  BookmarkLocalDataBatchUploader uploader(bookmark_model());
  base::test::TestFuture<syncer::LocalDataDescription> description;

  uploader.GetLocalDataDescription(description.GetCallback());

  // The full list includes folders.
  EXPECT_EQ(description.Get().local_data_models.size(), 1u);
  auto folder_item = description.Get().local_data_models[0];
  EXPECT_EQ(std::get<int64_t>(folder_item.id), folder->id());
  EXPECT_EQ(folder_item.title, "folder");
  EXPECT_EQ(folder_item.subtitle, "0 bookmarks");

  // The overview does not include folders.
  EXPECT_EQ(description.Get().item_count, 0u);
  EXPECT_THAT(description.Get().domains, IsEmpty());
  EXPECT_EQ(description.Get().domain_count, 0u);
}

TEST_F(BookmarkLocalDataBatchUploaderTest, LocalDescriptionFolderNesting) {
  bookmark_model()->LoadEmptyForTest();
  bookmark_model()->CreateAccountPermanentFolders();

  // Create the following structure:
  // bookmark_bar
  //   l1_folder
  //     l2_folder
  //       l3_url
  //     l2_url
  //   l1_url
  const bookmarks::BookmarkNode* l1_folder = bookmark_model()->AddFolder(
      bookmark_model()->bookmark_bar_node(), /*index=*/0, u"l1_folder");
  const bookmarks::BookmarkNode* l2_folder =
      bookmark_model()->AddFolder(l1_folder, /*index=*/0, u"l2_folder");
  bookmark_model()->AddURL(l2_folder, /*index=*/0, u"l3_url",
                           GURL("http://l3.com/"));
  bookmark_model()->AddURL(l1_folder, /*index=*/1, u"l2_url",
                           GURL("http://l2.com/"));
  bookmark_model()->AddURL(bookmark_model()->bookmark_bar_node(),
                           /*index=*/1, u"l1_url", GURL("http://l1.com/"));

  BookmarkLocalDataBatchUploader uploader(bookmark_model());
  base::test::TestFuture<syncer::LocalDataDescription> description;

  uploader.GetLocalDataDescription(description.GetCallback());

  // The full list includes only the top-level items. The bookmark count
  // includes the URLs in the subtree (but not the folder).
  EXPECT_THAT(description.Get().local_data_models,
              ElementsAre(MatchesFolderDataItem("l1_folder", 2),
                          MatchesBookmarkDataItem("l1_url")));

  // The overview includes all URLs in the subtree, but not the folders.
  EXPECT_EQ(description.Get().item_count, 3u);
  EXPECT_EQ(description.Get().domain_count, 3u);
  EXPECT_THAT(description.Get().domains,
              ElementsAre("l1.com", "l2.com", "l3.com"));
}

TEST_F(BookmarkLocalDataBatchUploaderTest, LocalDescriptionHasSortedDomains) {
  bookmark_model()->LoadEmptyForTest();
  bookmark_model()->CreateAccountPermanentFolders();
  bookmark_model()->AddURL(bookmark_model()->bookmark_bar_node(), /*index=*/0,
                           u"Z", GURL("https://a.com"));
  bookmark_model()->AddURL(bookmark_model()->bookmark_bar_node(), /*index=*/0,
                           u"A", GURL("http://b.com"));
  BookmarkLocalDataBatchUploader uploader(bookmark_model());
  base::test::TestFuture<syncer::LocalDataDescription> description;

  uploader.GetLocalDataDescription(description.GetCallback());

  // TODO(crbug.com/381814677): implement and check desired sorting order for
  // individual items.

  // Sorting is *not* by bookmark name, nor by full URL (http://b.com is
  // < https://a.com). It's by domain (a.com < b.com).
  EXPECT_EQ(description.Get().item_count, 2u);
  EXPECT_EQ(description.Get().domain_count, 2u);
  EXPECT_THAT(description.Get().domains, ElementsAre("a.com", "b.com"));
}

TEST_F(BookmarkLocalDataBatchUploaderTest, LocalDescriptionHasNoManagedUrls) {
  // Create a new model with the managed node enabled.
  auto client = std::make_unique<bookmarks::TestBookmarkClient>();
  bookmarks::BookmarkNode* managed_node = client->EnableManagedNode();
  auto model = std::make_unique<bookmarks::BookmarkModel>(std::move(client));
  model->LoadEmptyForTest();
  model->CreateAccountPermanentFolders();
  model->AddURL(managed_node, /*index=*/0, u"Managed",
                GURL("http://managed.com"));
  BookmarkLocalDataBatchUploader uploader(model.get());
  base::test::TestFuture<syncer::LocalDataDescription> description;

  uploader.GetLocalDataDescription(description.GetCallback());

  EXPECT_THAT(description.Get().local_data_models, IsEmpty());

  EXPECT_EQ(description.Get().item_count, 0u);
  EXPECT_EQ(description.Get().domain_count, 0u);
  EXPECT_THAT(description.Get().domains, IsEmpty());
}

TEST_F(BookmarkLocalDataBatchUploaderTest, MigrationNoOpsIfNullModel) {
  BookmarkLocalDataBatchUploader uploader(nullptr);

  uploader.TriggerLocalDataMigration();

  // Should not crash.
}

TEST_F(BookmarkLocalDataBatchUploaderTest, MigrationNoOpsIfModelNotLoaded) {
  BookmarkLocalDataBatchUploader uploader(bookmark_model());

  uploader.TriggerLocalDataMigration();

  // Should not crash.
}

TEST_F(BookmarkLocalDataBatchUploaderTest, MigrationNoOpsIfTransportModeOff) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      syncer::kSyncEnableBookmarksInTransportMode);
  bookmark_model()->LoadEmptyForTest();
  bookmark_model()->AddURL(bookmark_model()->bookmark_bar_node(),
                           /*index=*/0, u"Local", GURL("http://local.com/"));
  BookmarkLocalDataBatchUploader uploader(bookmark_model());

  uploader.TriggerLocalDataMigration();

  EXPECT_THAT(bookmark_model()->bookmark_bar_node()->children(),
              ElementsAre(MatchesTitleAndUrl(u"Local", "http://local.com/")));
  EXPECT_THAT(bookmark_model()->mobile_node()->children(), IsEmpty());
  EXPECT_THAT(bookmark_model()->other_node()->children(), IsEmpty());
}

TEST_F(BookmarkLocalDataBatchUploaderTest,
       MigrationNoOpsIfAccountNodesMissing) {
  bookmark_model()->LoadEmptyForTest();
  ASSERT_FALSE(bookmark_model()->account_bookmark_bar_node());
  bookmark_model()->AddURL(bookmark_model()->bookmark_bar_node(),
                           /*index=*/0, u"Local", GURL("http://local.com/"));
  BookmarkLocalDataBatchUploader uploader(bookmark_model());

  uploader.TriggerLocalDataMigration();

  EXPECT_THAT(bookmark_model()->bookmark_bar_node()->children(),
              ElementsAre(MatchesTitleAndUrl(u"Local", "http://local.com/")));
  EXPECT_THAT(bookmark_model()->mobile_node()->children(), IsEmpty());
  EXPECT_THAT(bookmark_model()->other_node()->children(), IsEmpty());
}

// Note: Most of the merging logic is verified in the unit tests for
// LocalBookmarkModelMerger, this test only checks the communication between the
// 2 layers.
TEST_F(BookmarkLocalDataBatchUploaderTest, MigrationUploadsLocalBookmarks) {
  bookmark_model()->LoadEmptyForTest();
  bookmark_model()->CreateAccountPermanentFolders();
  bookmark_model()->AddURL(bookmark_model()->bookmark_bar_node(),
                           /*index=*/0, u"Local", GURL("http://local.com/"));
  bookmark_model()->AddURL(bookmark_model()->account_bookmark_bar_node(),
                           /*index=*/0, u"Account",
                           GURL("http://account.com/"));
  BookmarkLocalDataBatchUploader uploader(bookmark_model());

  uploader.TriggerLocalDataMigration();

  EXPECT_THAT(bookmark_model()->bookmark_bar_node()->children(), IsEmpty());
  EXPECT_THAT(bookmark_model()->mobile_node()->children(), IsEmpty());
  EXPECT_THAT(bookmark_model()->other_node()->children(), IsEmpty());
  EXPECT_THAT(bookmark_model()->account_bookmark_bar_node()->children(),
              ElementsAre(MatchesTitleAndUrl(u"Account", "http://account.com/"),
                          MatchesTitleAndUrl(u"Local", "http://local.com/")));
  EXPECT_THAT(bookmark_model()->account_mobile_node()->children(), IsEmpty());
  EXPECT_THAT(bookmark_model()->account_other_node()->children(), IsEmpty());
}

}  // namespace
}  // namespace sync_bookmarks
