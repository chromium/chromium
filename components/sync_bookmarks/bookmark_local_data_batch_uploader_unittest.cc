// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_bookmarks/bookmark_local_data_batch_uploader.h"

#include <memory>
#include <utility>
#include <variant>

#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "components/bookmarks/common/bookmark_pref_names.h"
#include "components/bookmarks/test/test_bookmark_client.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/strings/grit/components_strings.h"
#include "components/sync/service/local_data_description.h"
#include "components/sync/test/test_matchers.h"
#include "components/sync_bookmarks/switches.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

namespace sync_bookmarks {
namespace {

using ::syncer::IsEmptyLocalDataDescription;
using ::syncer::MatchesLocalDataDescription;
using ::syncer::MatchesLocalDataItemModel;
using ::testing::_;
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
  BookmarkLocalDataBatchUploaderTest() {
    pref_service_.registry()->RegisterBooleanPref(
        bookmarks::prefs::kEditBookmarksEnabled, true);
  }

  ~BookmarkLocalDataBatchUploaderTest() override = default;

  bookmarks::BookmarkModel* bookmark_model() { return bookmark_model_.get(); }
  PrefService* pref_service() { return &pref_service_; }

 private:
  base::test::ScopedFeatureList feature_list_{
      switches::kSyncEnableBookmarksInTransportMode};
  const std::unique_ptr<bookmarks::BookmarkModel> bookmark_model_ =
      std::make_unique<bookmarks::BookmarkModel>(
          std::make_unique<bookmarks::TestBookmarkClient>());
  TestingPrefServiceSimple pref_service_;
};

TEST_F(BookmarkLocalDataBatchUploaderTest, LocalDescriptionEmptyIfNullModel) {
  BookmarkLocalDataBatchUploader uploader(nullptr, pref_service());
  base::test::TestFuture<syncer::LocalDataDescription> description;

  uploader.GetLocalDataDescription(description.GetCallback());

  EXPECT_THAT(description.Get(), IsEmptyLocalDataDescription());
}

TEST_F(BookmarkLocalDataBatchUploaderTest,
       LocalDescriptionEmptyIfModelNotLoaded) {
  BookmarkLocalDataBatchUploader uploader(bookmark_model(), pref_service());
  base::test::TestFuture<syncer::LocalDataDescription> description;

  uploader.GetLocalDataDescription(description.GetCallback());

  EXPECT_THAT(description.Get(), IsEmptyLocalDataDescription());
}

TEST_F(BookmarkLocalDataBatchUploaderTest,
       LocalDescriptionEmptyIfTransportModeOff) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      switches::kSyncEnableBookmarksInTransportMode);
  bookmark_model()->LoadEmptyForTest();
  BookmarkLocalDataBatchUploader uploader(bookmark_model(), pref_service());
  base::test::TestFuture<syncer::LocalDataDescription> description;

  uploader.GetLocalDataDescription(description.GetCallback());

  EXPECT_THAT(description.Get(), IsEmptyLocalDataDescription());
}

TEST_F(BookmarkLocalDataBatchUploaderTest,
       LocalDescriptionEmptyIfNoAccountFolders) {
  bookmark_model()->LoadEmptyForTest();
  ASSERT_FALSE(bookmark_model()->account_bookmark_bar_node());
  bookmark_model()->AddURL(bookmark_model()->bookmark_bar_node(), /*index=*/0,
                           u"Local", GURL("http://local.com/"));
  BookmarkLocalDataBatchUploader uploader(bookmark_model(), pref_service());
  base::test::TestFuture<syncer::LocalDataDescription> description;

  uploader.GetLocalDataDescription(description.GetCallback());

  EXPECT_THAT(description.Get(), IsEmptyLocalDataDescription());
}

TEST_F(BookmarkLocalDataBatchUploaderTest,
       LocalDescriptionEmptyIfEditBookmarksDislabed) {
  pref_service()->SetBoolean(bookmarks::prefs::kEditBookmarksEnabled, false);
  bookmark_model()->LoadEmptyForTest();
  bookmark_model()->CreateAccountPermanentFolders();
  bookmark_model()->AddURL(bookmark_model()->bookmark_bar_node(), /*index=*/0,
                           u"Local", GURL("http://local.com/"));
  BookmarkLocalDataBatchUploader uploader(bookmark_model(), pref_service());
  base::test::TestFuture<syncer::LocalDataDescription> description;

  uploader.GetLocalDataDescription(description.GetCallback());

  EXPECT_THAT(description.Get(), IsEmptyLocalDataDescription());
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
  BookmarkLocalDataBatchUploader uploader(bookmark_model(), pref_service());
  base::test::TestFuture<syncer::LocalDataDescription> description;

  uploader.GetLocalDataDescription(description.GetCallback());

  EXPECT_THAT(description.Get(),
              MatchesLocalDataDescription(
                  syncer::DataType::BOOKMARKS,
                  ElementsAre(MatchesLocalDataItemModel(
                      local_node->id(),
                      syncer::LocalDataItemModel::PageUrlIcon(
                          GURL("http://local.com/")),
                      /*title=*/"Local", /*subtitle=*/IsEmpty())),
                  /*item_count=*/1u, /*domains=*/ElementsAre("local.com"),
                  /*domain_count=*/1u));
}

TEST_F(BookmarkLocalDataBatchUploaderTest,
       LocalDescriptionEmptyItemsWhenSelectedItemsFeatureDisabled) {
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
  BookmarkLocalDataBatchUploader uploader(bookmark_model(), pref_service());
  base::test::TestFuture<syncer::LocalDataDescription> description;

  uploader.GetLocalDataDescription(description.GetCallback());

  EXPECT_THAT(description.Get(),
              MatchesLocalDataDescription(_, /*local_data_models=*/IsEmpty(),
                                          /*item_count=*/1u,
                                          /*domains=*/ElementsAre("local.com"),
                                          /*domain_count=*/1u));
}

TEST_F(BookmarkLocalDataBatchUploaderTest,
       LocalDescriptionTopLevelEmptyFolder) {
  bookmark_model()->LoadEmptyForTest();
  bookmark_model()->CreateAccountPermanentFolders();
  const bookmarks::BookmarkNode* folder = bookmark_model()->AddFolder(
      bookmark_model()->bookmark_bar_node(), /*index=*/0, u"folder");
  BookmarkLocalDataBatchUploader uploader(bookmark_model(), pref_service());
  base::test::TestFuture<syncer::LocalDataDescription> description;

  uploader.GetLocalDataDescription(description.GetCallback());

  EXPECT_THAT(description.Get(),
              MatchesLocalDataDescription(
                  syncer::DataType::BOOKMARKS,
                  ElementsAre(MatchesLocalDataItemModel(
                      folder->id(), syncer::LocalDataItemModel::FolderIcon(),
                      /*title=*/"folder",
                      /*subtitle=*/"0 bookmarks")),
                  /*item_count=*/0u, /*domains=*/IsEmpty(),
                  /*domain_count=*/0u));
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
  const bookmarks::BookmarkNode* l1_bookmark =
      bookmark_model()->AddURL(bookmark_model()->bookmark_bar_node(),
                               /*index=*/1, u"l1_url", GURL("http://l1.com/"));

  BookmarkLocalDataBatchUploader uploader(bookmark_model(), pref_service());
  base::test::TestFuture<syncer::LocalDataDescription> description;

  uploader.GetLocalDataDescription(description.GetCallback());

  EXPECT_THAT(
      description.Get(),
      MatchesLocalDataDescription(
          syncer::DataType::BOOKMARKS,
          // The full list includes only the top-level items. The bookmark count
          // includes the URLs in the subtree (but not the folder).
          ElementsAre(MatchesLocalDataItemModel(
                          l1_folder->id(),
                          syncer::LocalDataItemModel::FolderIcon(), "l1_folder",
                          l10n_util::GetPluralStringFUTF8(
                              IDS_BULK_UPLOAD_BOOKMARK_FOLDER_SUBTITLE, 2)),
                      MatchesLocalDataItemModel(
                          l1_bookmark->id(),
                          syncer::LocalDataItemModel::PageUrlIcon(
                              GURL("http://l1.com/")),
                          /*title=*/"l1_url", /*subtitle=*/IsEmpty())),
          /*item_count=*/3u,
          /*domains=*/ElementsAre("l1.com", "l2.com", "l3.com"),
          /*domain_count=*/3u));
}

TEST_F(BookmarkLocalDataBatchUploaderTest, LocalDescriptionHasSortedDomains) {
  bookmark_model()->LoadEmptyForTest();
  bookmark_model()->CreateAccountPermanentFolders();
  const bookmarks::BookmarkNode* bookmark_a =
      bookmark_model()->AddURL(bookmark_model()->bookmark_bar_node(),
                               /*index=*/0, u"Z", GURL("https://a.com"));
  const bookmarks::BookmarkNode* bookmark_b =
      bookmark_model()->AddURL(bookmark_model()->bookmark_bar_node(),
                               /*index=*/0, u"A", GURL("http://b.com"));
  BookmarkLocalDataBatchUploader uploader(bookmark_model(), pref_service());
  base::test::TestFuture<syncer::LocalDataDescription> description;

  uploader.GetLocalDataDescription(description.GetCallback());

  EXPECT_THAT(
      description.Get(),
      MatchesLocalDataDescription(
          syncer::DataType::BOOKMARKS,
          // Ordered by recency.
          ElementsAre(
              MatchesLocalDataItemModel(
                  bookmark_b->id(),
                  syncer::LocalDataItemModel::PageUrlIcon(GURL("http://b.com")),
                  /*title=*/"A", /*subtitle=*/IsEmpty()),
              MatchesLocalDataItemModel(bookmark_a->id(),
                                        syncer::LocalDataItemModel::PageUrlIcon(
                                            GURL("https://a.com")),
                                        /*title=*/"Z", /*subtitle=*/IsEmpty())),
          /*item_count=*/2u,
          // Sorting is *not* by bookmark name, nor by full URL (http://b.com is
          // < https://a.com). It's by domain (a.com < b.com).
          /*domains=*/ElementsAre("a.com", "b.com"),
          /*domain_count=*/2u));
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
  BookmarkLocalDataBatchUploader uploader(model.get(), pref_service());
  base::test::TestFuture<syncer::LocalDataDescription> description;

  uploader.GetLocalDataDescription(description.GetCallback());

  EXPECT_THAT(description.Get(), IsEmptyLocalDataDescription());
}

TEST_F(BookmarkLocalDataBatchUploaderTest, MigrationNoOpsIfNullModel) {
  BookmarkLocalDataBatchUploader uploader(nullptr, pref_service());

  uploader.TriggerLocalDataMigration();

  // Should not crash.
}

TEST_F(BookmarkLocalDataBatchUploaderTest, MigrationNoOpsIfModelNotLoaded) {
  BookmarkLocalDataBatchUploader uploader(bookmark_model(), pref_service());

  uploader.TriggerLocalDataMigration();

  // Should not crash.
}

TEST_F(BookmarkLocalDataBatchUploaderTest, MigrationNoOpsIfTransportModeOff) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      switches::kSyncEnableBookmarksInTransportMode);
  bookmark_model()->LoadEmptyForTest();
  bookmark_model()->AddURL(bookmark_model()->bookmark_bar_node(),
                           /*index=*/0, u"Local", GURL("http://local.com/"));
  BookmarkLocalDataBatchUploader uploader(bookmark_model(), pref_service());

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
  BookmarkLocalDataBatchUploader uploader(bookmark_model(), pref_service());

  uploader.TriggerLocalDataMigration();

  EXPECT_THAT(bookmark_model()->bookmark_bar_node()->children(),
              ElementsAre(MatchesTitleAndUrl(u"Local", "http://local.com/")));
  EXPECT_THAT(bookmark_model()->mobile_node()->children(), IsEmpty());
  EXPECT_THAT(bookmark_model()->other_node()->children(), IsEmpty());
}

TEST_F(BookmarkLocalDataBatchUploaderTest,
       FullMigrationNoOpsIfEditBookmarksDisalbed) {
  pref_service()->SetBoolean(bookmarks::prefs::kEditBookmarksEnabled, false);
  bookmark_model()->LoadEmptyForTest();
  bookmark_model()->CreateAccountPermanentFolders();
  bookmark_model()->AddURL(bookmark_model()->bookmark_bar_node(),
                           /*index=*/0, u"Local", GURL("http://local.com/"));
  ASSERT_THAT(bookmark_model()->account_bookmark_bar_node()->children(),
              IsEmpty());
  BookmarkLocalDataBatchUploader uploader(bookmark_model(), pref_service());

  uploader.TriggerLocalDataMigration();

  EXPECT_THAT(bookmark_model()->bookmark_bar_node()->children(),
              ElementsAre(MatchesTitleAndUrl(u"Local", "http://local.com/")));
  EXPECT_THAT(bookmark_model()->mobile_node()->children(), IsEmpty());
  EXPECT_THAT(bookmark_model()->other_node()->children(), IsEmpty());
  EXPECT_THAT(bookmark_model()->account_bookmark_bar_node()->children(),
              IsEmpty());
}

TEST_F(BookmarkLocalDataBatchUploaderTest,
       PartialMigrationNoOpsIfEditBookmarksDisalbed) {
  pref_service()->SetBoolean(bookmarks::prefs::kEditBookmarksEnabled, false);
  bookmark_model()->LoadEmptyForTest();
  bookmark_model()->CreateAccountPermanentFolders();
  const bookmarks::BookmarkNode* local_node = bookmark_model()->AddURL(
      bookmark_model()->bookmark_bar_node(),
      /*index=*/0, u"Local", GURL("http://local.com/"));
  ASSERT_THAT(bookmark_model()->account_bookmark_bar_node()->children(),
              IsEmpty());
  BookmarkLocalDataBatchUploader uploader(bookmark_model(), pref_service());

  uploader.TriggerLocalDataMigrationForItems(
      {syncer::LocalDataItemModel::DataId(local_node->id())});

  EXPECT_THAT(bookmark_model()->bookmark_bar_node()->children(),
              ElementsAre(MatchesTitleAndUrl(u"Local", "http://local.com/")));
  EXPECT_THAT(bookmark_model()->mobile_node()->children(), IsEmpty());
  EXPECT_THAT(bookmark_model()->other_node()->children(), IsEmpty());
  EXPECT_THAT(bookmark_model()->account_bookmark_bar_node()->children(),
              IsEmpty());
}

// Note: Most of the merging logic is verified in the unit tests for
// LocalBookmarkModelMerger, this test only checks the communication between the
// 2 layers.
TEST_F(BookmarkLocalDataBatchUploaderTest, FullMigrationUploadsLocalBookmarks) {
  bookmark_model()->LoadEmptyForTest();
  bookmark_model()->CreateAccountPermanentFolders();
  bookmark_model()->AddURL(bookmark_model()->bookmark_bar_node(),
                           /*index=*/0, u"Local", GURL("http://local.com/"));
  bookmark_model()->AddURL(bookmark_model()->account_bookmark_bar_node(),
                           /*index=*/0, u"Account",
                           GURL("http://account.com/"));
  BookmarkLocalDataBatchUploader uploader(bookmark_model(), pref_service());

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

// Note: Most of the merging logic is verified in the unit tests for
// LocalBookmarkModelMerger, this test only checks the communication between the
// 2 layers.
TEST_F(BookmarkLocalDataBatchUploaderTest,
       PartialMigrationUploadsSelectedLocalBookmarks) {
  bookmark_model()->LoadEmptyForTest();
  bookmark_model()->CreateAccountPermanentFolders();
  const bookmarks::BookmarkNode* local_node1 = bookmark_model()->AddURL(
      bookmark_model()->bookmark_bar_node(),
      /*index=*/0, u"Local", GURL("http://local1.com/"));
  bookmark_model()->AddURL(bookmark_model()->bookmark_bar_node(),
                           /*index=*/1, u"Local", GURL("http://local2.com/"));
  bookmark_model()->AddURL(bookmark_model()->account_bookmark_bar_node(),
                           /*index=*/0, u"Account",
                           GURL("http://account.com/"));
  BookmarkLocalDataBatchUploader uploader(bookmark_model(), pref_service());

  uploader.TriggerLocalDataMigrationForItems(
      {syncer::LocalDataItemModel::DataId(local_node1->id())});

  EXPECT_THAT(bookmark_model()->bookmark_bar_node()->children(),
              ElementsAre(MatchesTitleAndUrl(u"Local", "http://local2.com/")));
  EXPECT_THAT(bookmark_model()->mobile_node()->children(), IsEmpty());
  EXPECT_THAT(bookmark_model()->other_node()->children(), IsEmpty());
  EXPECT_THAT(bookmark_model()->account_bookmark_bar_node()->children(),
              ElementsAre(MatchesTitleAndUrl(u"Account", "http://account.com/"),
                          MatchesTitleAndUrl(u"Local", "http://local1.com/")));
  EXPECT_THAT(bookmark_model()->account_mobile_node()->children(), IsEmpty());
  EXPECT_THAT(bookmark_model()->account_other_node()->children(), IsEmpty());
}

}  // namespace
}  // namespace sync_bookmarks
