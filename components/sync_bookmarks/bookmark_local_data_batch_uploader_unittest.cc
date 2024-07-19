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
#include "components/sync/base/features.h"
#include "components/sync/service/local_data_description.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace sync_bookmarks {
namespace {

using ::testing::ElementsAre;
using ::testing::IsEmpty;

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

  EXPECT_EQ(description.Get().item_count, 0u);
  EXPECT_EQ(description.Get().domain_count, 0u);
  EXPECT_THAT(description.Get().domains, IsEmpty());
}

TEST_F(BookmarkLocalDataBatchUploaderTest,
       LocalDescriptionEmptyIfModelNotLoaded) {
  BookmarkLocalDataBatchUploader uploader(bookmark_model());
  base::test::TestFuture<syncer::LocalDataDescription> description;

  uploader.GetLocalDataDescription(description.GetCallback());

  EXPECT_EQ(description.Get().item_count, 0u);
  EXPECT_EQ(description.Get().domain_count, 0u);
  EXPECT_THAT(description.Get().domains, IsEmpty());
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

  EXPECT_EQ(description.Get().item_count, 0u);
  EXPECT_EQ(description.Get().domain_count, 0u);
  EXPECT_THAT(description.Get().domains, IsEmpty());
}

TEST_F(BookmarkLocalDataBatchUploaderTest, LocalDescriptionOnlyHasLocalData) {
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

  EXPECT_EQ(description.Get().item_count, 1u);
  EXPECT_EQ(description.Get().domain_count, 1u);
  EXPECT_THAT(description.Get().domains, ElementsAre("local.com"));
}

TEST_F(BookmarkLocalDataBatchUploaderTest, LocalDescriptionHasNoFolders) {
  bookmark_model()->LoadEmptyForTest();
  bookmark_model()->CreateAccountPermanentFolders();
  bookmark_model()->AddURL(bookmark_model()->bookmark_bar_node(), /*index=*/0,
                           u"url1", GURL("http://url1.com"));
  const bookmarks::BookmarkNode* folder = bookmark_model()->AddFolder(
      bookmark_model()->bookmark_bar_node(), /*index=*/1, u"folder");
  bookmark_model()->AddURL(folder, /*index=*/0, u"url2",
                           GURL("http://url2.com"));
  BookmarkLocalDataBatchUploader uploader(bookmark_model());
  base::test::TestFuture<syncer::LocalDataDescription> description;

  uploader.GetLocalDataDescription(description.GetCallback());

  EXPECT_EQ(description.Get().item_count, 2u);
  EXPECT_EQ(description.Get().domain_count, 2u);
  EXPECT_THAT(description.Get().domains, ElementsAre("url1.com", "url2.com"));
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
