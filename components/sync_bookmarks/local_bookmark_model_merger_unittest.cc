// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_bookmarks/local_bookmark_model_merger.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/test/scoped_feature_list.h"
#include "base/uuid.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "components/bookmarks/browser/bookmark_test_util.h"
#include "components/bookmarks/test/test_bookmark_client.h"
#include "components/bookmarks/test/test_matchers.h"
#include "components/sync/base/features.h"
#include "components/sync_bookmarks/bookmark_model_view.h"
#include "components/sync_bookmarks/test_bookmark_model_view.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "url/gurl.h"

namespace sync_bookmarks {
namespace {

using bookmarks::test::IsFolder;
using bookmarks::test::IsFolderWithUuid;
using bookmarks::test::IsUrlBookmark;
using bookmarks::test::IsUrlBookmarkWithUuid;
using testing::ElementsAre;
using testing::IsEmpty;
using testing::Ne;

// Test class to build bookmark URLs conveniently and compactly in tests.
class UrlBuilder {
 public:
  UrlBuilder(const std::u16string& title, const GURL& url)
      : title_(title), url_(url) {}
  UrlBuilder(const UrlBuilder&) = default;
  ~UrlBuilder() = default;

  UrlBuilder& SetUuid(const base::Uuid& uuid) {
    uuid_ = uuid;
    return *this;
  }

  void Build(BookmarkModelView* model,
             const bookmarks::BookmarkNode* parent) const {
    model->AddURL(parent, parent->children().size(), title_, url_,
                  /*meta_info=*/nullptr, /*creation_time=*/std::nullopt, uuid_);
  }

 private:
  const std::u16string title_;
  const GURL url_;
  std::optional<base::Uuid> uuid_;
};

// Test class to build bookmark folders and compactly in tests.
class FolderBuilder {
 public:
  using FolderOrUrl = absl::variant<FolderBuilder, UrlBuilder>;

  static void AddChildTo(BookmarkModelView* model,
                         const bookmarks::BookmarkNode* parent,
                         const FolderOrUrl& folder_or_url) {
    if (absl::holds_alternative<UrlBuilder>(folder_or_url)) {
      absl::get<UrlBuilder>(folder_or_url).Build(model, parent);
    } else {
      CHECK(absl::holds_alternative<FolderBuilder>(folder_or_url));
      absl::get<FolderBuilder>(folder_or_url).Build(model, parent);
    }
  }

  static void AddChildrenTo(BookmarkModelView* model,
                            const bookmarks::BookmarkNode* parent,
                            const std::vector<FolderOrUrl>& children) {
    for (const FolderOrUrl& folder_or_url : children) {
      AddChildTo(model, parent, folder_or_url);
    }
  }

  explicit FolderBuilder(const std::u16string& title) : title_(title) {}
  FolderBuilder(const FolderBuilder&) = default;
  ~FolderBuilder() = default;

  FolderBuilder& SetChildren(std::vector<FolderOrUrl> children) {
    children_ = std::move(children);
    return *this;
  }

  FolderBuilder& SetUuid(const base::Uuid& uuid) {
    uuid_ = uuid;
    return *this;
  }

  void Build(BookmarkModelView* model,
             const bookmarks::BookmarkNode* parent) const {
    const bookmarks::BookmarkNode* folder = model->AddFolder(
        parent, parent->children().size(), title_,
        /*meta_info=*/nullptr, /*creation_time=*/std::nullopt, uuid_);
    AddChildrenTo(model, folder, children_);
  }

 private:
  const std::u16string title_;
  std::vector<FolderOrUrl> children_;
  std::optional<base::Uuid> uuid_;
};

std::unique_ptr<TestBookmarkModelView> BuildLocalModel(
    const std::vector<FolderBuilder::FolderOrUrl>& children_of_bookmark_bar) {
  auto model = std::make_unique<TestBookmarkModelView>(
      TestBookmarkModelView::ViewType::kLocalOrSyncableNodes);
  FolderBuilder::AddChildrenTo(model.get(), model->bookmark_bar_node(),
                               children_of_bookmark_bar);
  return model;
}

std::unique_ptr<TestBookmarkModelView> BuildAccountModel(
    const std::vector<FolderBuilder::FolderOrUrl>& children_of_bookmark_bar) {
  auto model = std::make_unique<TestBookmarkModelView>(
      TestBookmarkModelView::ViewType::kAccountNodes);
  model->EnsurePermanentNodesExist();
  FolderBuilder::AddChildrenTo(model.get(), model->bookmark_bar_node(),
                               children_of_bookmark_bar);
  return model;
}

class LocalBookmarkModelMergerTest : public testing::Test {
 protected:
  LocalBookmarkModelMergerTest() = default;
  ~LocalBookmarkModelMergerTest() override = default;

  base::test::ScopedFeatureList feature_list_{
      syncer::kSyncEnableBookmarksInTransportMode};
};

TEST_F(LocalBookmarkModelMergerTest,
       ShouldUploadEntireLocalModelIfAccountModelEmpty) {
  const std::u16string kFolder1Title = u"folder1";
  const std::u16string kFolder2Title = u"folder2";

  const std::u16string kUrl1Title = u"url1";
  const std::u16string kUrl2Title = u"url2";
  const std::u16string kUrl3Title = u"url3";
  const std::u16string kUrl4Title = u"url4";

  const GURL kUrl1("http://www.url1.com/");
  const GURL kUrl2("http://www.url2.com/");
  const GURL kUrl3("http://www.url3.com/");
  const GURL kUrl4("http://www.url4.com/");

  // -------- The local model --------
  // bookmark_bar
  //  |- folder 1
  //    |- url1(http://www.url1.com)
  //    |- url2(http://www.url2.com)
  //  |- folder 2
  //    |- url3(http://www.url3.com)
  //    |- url4(http://www.url4.com)
  std::unique_ptr<BookmarkModelView> local_model =
      BuildLocalModel({FolderBuilder(kFolder1Title)
                           .SetChildren({UrlBuilder(kUrl1Title, kUrl1),
                                         UrlBuilder(kUrl2Title, kUrl2)}),
                       FolderBuilder(kFolder2Title)
                           .SetChildren({UrlBuilder(kUrl3Title, kUrl3),
                                         UrlBuilder(kUrl4Title, kUrl4)})});

  // -------- The account model --------
  // bookmark_bar
  std::unique_ptr<BookmarkModelView> account_model = BuildAccountModel({});

  // -------- Exercise the merge logic --------
  LocalBookmarkModelMerger(local_model.get(), account_model.get()).Merge();

  // -------- The expected merge outcome --------
  // Same as the local model described above.
  EXPECT_THAT(
      account_model->bookmark_bar_node()->children(),
      ElementsAre(IsFolder(kFolder1Title,
                           ElementsAre(IsUrlBookmark(kUrl1Title, kUrl1),
                                       IsUrlBookmark(kUrl2Title, kUrl2))),
                  IsFolder(kFolder2Title,
                           ElementsAre(IsUrlBookmark(kUrl3Title, kUrl3),
                                       IsUrlBookmark(kUrl4Title, kUrl4)))));
}

TEST_F(LocalBookmarkModelMergerTest, ShouldIgnoreManagedNodes) {
  const std::u16string kUrl1Title = u"url1";
  const std::u16string kUrl2Title = u"url2";

  const GURL kUrl1("http://www.url1.com/");
  const GURL kUrl2("http://www.url2.com/");

  auto local_client = std::make_unique<bookmarks::TestBookmarkClient>();
  bookmarks::BookmarkNode* local_managed_node =
      local_client->EnableManagedNode();

  // -------- The local model --------
  // bookmark_bar
  //  |- url1(http://www.url1.com)
  // managed_bookmarks
  //  |- url2(http://www.url2.com)
  TestBookmarkModelView local_model(
      TestBookmarkModelView::ViewType::kLocalOrSyncableNodes,
      std::move(local_client));

  FolderBuilder::AddChildrenTo(&local_model, local_model.bookmark_bar_node(),
                               {UrlBuilder(kUrl1Title, kUrl1)});
  FolderBuilder::AddChildrenTo(&local_model, local_managed_node,
                               {UrlBuilder(kUrl2Title, kUrl2)});

  // -------- The account model --------
  // bookmark_bar
  std::unique_ptr<TestBookmarkModelView> account_model = BuildAccountModel({});

  // -------- Exercise the merge logic --------
  LocalBookmarkModelMerger(&local_model, account_model.get()).Merge();

  // -------- The expected merge outcome --------
  // bookmark_bar
  //  |- url1(http://www.url1.com)
  //
  ASSERT_THAT(account_model->bookmark_bar_node()->children(),
              ElementsAre(IsUrlBookmark(kUrl1Title, kUrl1)));

  // Managed nodes should be excluded from the merge.
  EXPECT_THAT(account_model->underlying_model()->GetNodesByURL(kUrl2),
              IsEmpty());
}

TEST_F(LocalBookmarkModelMergerTest, ShouldUploadLocalUuid) {
  const std::u16string kUrl1Title = u"url1";
  const GURL kUrl1("http://www.url1.com/");
  const base::Uuid kUrl1Uuid = base::Uuid::GenerateRandomV4();

  // -------- The local model --------
  // bookmark_bar
  //  | - bookmark(kUuid/kLocalTitle)
  std::unique_ptr<BookmarkModelView> local_model =
      BuildLocalModel({UrlBuilder(kUrl1Title, kUrl1).SetUuid(kUrl1Uuid)});

  // -------- The account model --------
  // bookmark_bar
  std::unique_ptr<BookmarkModelView> account_model = BuildAccountModel({});

  // -------- Exercise the merge logic --------
  LocalBookmarkModelMerger(local_model.get(), account_model.get()).Merge();

  // -------- The expected merge outcome --------
  // Same as the local model described above, including the UUID.
  EXPECT_THAT(account_model->bookmark_bar_node()->children(),
              ElementsAre(IsUrlBookmarkWithUuid(kUrl1Title, kUrl1, kUrl1Uuid)));
}

TEST_F(LocalBookmarkModelMergerTest, ShouldNotUploadDuplicateBySemantics) {
  const std::u16string kFolder1Title = u"folder1";

  const std::u16string kUrl1Title = u"url1";
  const std::u16string kUrl2Title = u"url2";
  const std::u16string kUrl3Title = u"url3";

  const GURL kUrl1("http://www.url1.com/");
  const GURL kUrl2("http://www.url2.com/");
  const GURL kUrl3("http://www.url3.com/");

  // -------- The local model --------
  // bookmark_bar
  //  |- folder 1
  //    |- url1(http://www.url1.com)
  //    |- url2(http://www.url2.com)
  std::unique_ptr<BookmarkModelView> local_model =
      BuildLocalModel({FolderBuilder(kFolder1Title)
                           .SetChildren({UrlBuilder(kUrl1Title, kUrl1),
                                         UrlBuilder(kUrl2Title, kUrl2)})});

  // -------- The account model --------
  // bookmark_bar
  //  |- folder 1
  //    |- url2(http://www.url2.com)
  //    |- url3(http://www.url3.com)
  std::unique_ptr<BookmarkModelView> account_model =
      BuildAccountModel({FolderBuilder(kFolder1Title)
                             .SetChildren({UrlBuilder(kUrl2Title, kUrl2),
                                           UrlBuilder(kUrl3Title, kUrl3)})});

  // -------- Exercise the merge logic --------
  LocalBookmarkModelMerger(local_model.get(), account_model.get()).Merge();

  // -------- The expected merge outcome --------
  // bookmark_bar
  //  |- folder 1
  //    |- url2(http://www.url2.com)
  //    |- url3(http://www.url3.com)
  //    |- url1(http://www.url1.com)
  EXPECT_THAT(
      account_model->bookmark_bar_node()->children(),
      ElementsAre(IsFolder(kFolder1Title,
                           ElementsAre(IsUrlBookmark(kUrl2Title, kUrl2),
                                       IsUrlBookmark(kUrl3Title, kUrl3),
                                       IsUrlBookmark(kUrl1Title, kUrl1)))));
}

TEST_F(LocalBookmarkModelMergerTest, ShouldMergeLocalAndAccountModels) {
  const std::u16string kFolder1Title = u"folder1";
  const std::u16string kFolder2Title = u"folder2";
  const std::u16string kFolder3Title = u"folder3";

  const std::u16string kUrl1Title = u"url1";
  const std::u16string kUrl2Title = u"url2";
  const std::u16string kUrl3Title = u"url3";
  const std::u16string kUrl4Title = u"url4";

  const GURL kUrl1("http://www.url1.com/");
  const GURL kUrl2("http://www.url2.com/");
  const GURL kUrl3("http://www.url3.com/");
  const GURL kUrl4("http://www.url4.com/");
  const GURL kAnotherUrl2("http://www.another-url2.com/");

  // -------- The local model --------
  // bookmark_bar
  //  |- folder 1
  //    |- url1(http://www.url1.com)
  //    |- url2(http://www.url2.com)
  //  |- folder 2
  //    |- url3(http://www.url3.com)
  //    |- url4(http://www.url4.com)
  std::unique_ptr<BookmarkModelView> local_model =
      BuildLocalModel({FolderBuilder(kFolder1Title)
                           .SetChildren({UrlBuilder(kUrl1Title, kUrl1),
                                         UrlBuilder(kUrl2Title, kUrl2)}),
                       FolderBuilder(kFolder2Title)
                           .SetChildren({UrlBuilder(kUrl3Title, kUrl3),
                                         UrlBuilder(kUrl4Title, kUrl4)})});

  // -------- The account model --------
  // bookmark_bar
  //  |- folder 1
  //    |- url1(http://www.url1.com)
  //    |- url2(http://www.another-url2.com)
  //  |- folder 3
  //    |- url3(http://www.url3.com)
  //    |- url4(http://www.url4.com)
  std::unique_ptr<BookmarkModelView> account_model = BuildAccountModel(
      {FolderBuilder(kFolder1Title)
           .SetChildren({UrlBuilder(kUrl1Title, kUrl1),
                         UrlBuilder(kUrl2Title, kAnotherUrl2)}),
       FolderBuilder(kFolder3Title)
           .SetChildren({UrlBuilder(kUrl3Title, kUrl3),
                         UrlBuilder(kUrl4Title, kUrl4)})});

  // -------- Exercise the merge logic --------
  LocalBookmarkModelMerger(local_model.get(), account_model.get()).Merge();

  // -------- The expected merge outcome --------
  // bookmark_bar
  //  |- folder 1
  //    |- url1(http://www.url1.com)
  //    |- url2(http://www.another-url2.com)
  //    |- url2(http://www.url2.com)
  //  |- folder 3
  //    |- url3(http://www.url3.com)
  //    |- url4(http://www.url4.com)
  //  |- folder 2
  //    |- url3(http://www.url3.com)
  //    |- url4(http://www.url4.com)
  EXPECT_THAT(
      account_model->bookmark_bar_node()->children(),
      ElementsAre(IsFolder(kFolder1Title,
                           ElementsAre(IsUrlBookmark(kUrl1Title, kUrl1),
                                       IsUrlBookmark(kUrl2Title, kAnotherUrl2),
                                       IsUrlBookmark(kUrl2Title, kUrl2))),
                  IsFolder(kFolder3Title,
                           ElementsAre(IsUrlBookmark(kUrl3Title, kUrl3),
                                       IsUrlBookmark(kUrl4Title, kUrl4))),
                  IsFolder(kFolder2Title,
                           ElementsAre(IsUrlBookmark(kUrl3Title, kUrl3),
                                       IsUrlBookmark(kUrl4Title, kUrl4)))));
}

// This tests that truncated titles produced by legacy clients are properly
// matched.
TEST_F(LocalBookmarkModelMergerTest,
       ShouldMergeLocalAndAccountNodesWhenAccountHasLegacyTruncatedTitle) {
  const std::u16string kLocalLongTitle(300, 'A');
  const std::u16string kAccountTruncatedTitle(255, 'A');

  // -------- The local model --------
  std::unique_ptr<BookmarkModelView> local_model =
      BuildLocalModel({FolderBuilder(kLocalLongTitle)});

  // -------- The account model --------
  std::unique_ptr<BookmarkModelView> account_model =
      BuildAccountModel({FolderBuilder(kAccountTruncatedTitle)});

  // -------- Exercise the merge logic --------
  LocalBookmarkModelMerger(local_model.get(), account_model.get()).Merge();

  // Both titles should have matched against each other.
  EXPECT_THAT(account_model->bookmark_bar_node()->children(),
              ElementsAre(IsFolder(kLocalLongTitle, IsEmpty())));
}

// This test checks that local node with truncated title will merge with account
// node which has full title.
TEST_F(LocalBookmarkModelMergerTest,
       ShouldMergeLocalAndAccountNodesWhenLocalHasLegacyTruncatedTitle) {
  const std::u16string kAccountFullTitle(300, 'A');
  const std::u16string kLocalTruncatedTitle(255, 'A');

  // -------- The local model --------
  std::unique_ptr<BookmarkModelView> local_model =
      BuildLocalModel({FolderBuilder(kLocalTruncatedTitle)});

  // -------- The account model --------
  std::unique_ptr<BookmarkModelView> account_model =
      BuildAccountModel({FolderBuilder(kAccountFullTitle)});

  // -------- Exercise the merge logic --------
  LocalBookmarkModelMerger(local_model.get(), account_model.get()).Merge();

  // Both titles should have matched against each other. Although the local
  // title is truncated, for simplicity of the algorithm and considering how
  // rare this scenario is, the local one wins.
  EXPECT_THAT(account_model->bookmark_bar_node()->children(),
              ElementsAre(IsFolder(kLocalTruncatedTitle, IsEmpty())));
}

TEST_F(LocalBookmarkModelMergerTest, ShouldMergeBookmarkByUuid) {
  const std::u16string kLocalTitle = u"Title 1";
  const std::u16string kAccountTitle = u"Title 2";
  const GURL kUrl("http://www.foo.com/");
  const base::Uuid kUuid = base::Uuid::GenerateRandomV4();

  // -------- The local model --------
  // bookmark_bar
  //  | - bookmark(kUuid/kLocalTitle)
  std::unique_ptr<BookmarkModelView> local_model =
      BuildLocalModel({UrlBuilder(kLocalTitle, kUrl).SetUuid(kUuid)});

  // -------- The account model --------
  // bookmark_bar
  //  | - bookmark(kUuid/kAccountTitle)
  std::unique_ptr<BookmarkModelView> account_model =
      BuildAccountModel({UrlBuilder(kAccountTitle, kUrl).SetUuid(kUuid)});

  // -------- Exercise the merge logic --------
  LocalBookmarkModelMerger(local_model.get(), account_model.get()).Merge();

  // -------- The merged model --------
  // bookmark_bar
  //  |- bookmark(kUuid/kLocalTitle)
  EXPECT_THAT(account_model->bookmark_bar_node()->children(),
              ElementsAre(IsUrlBookmarkWithUuid(kLocalTitle, kUrl, kUuid)));
}

TEST_F(LocalBookmarkModelMergerTest,
       ShouldDeduplicateBySemanticsAfterParentMatchedByUuid) {
  const std::u16string kFolder1Title = u"folder1";
  const std::u16string kFolder2Title = u"folder2";
  const std::u16string kFolder3Title = u"folder3";

  const std::u16string kUrl1Title = u"url1";
  const std::u16string kUrl2Title = u"url2";
  const std::u16string kUrl3Title = u"url3";

  const GURL kUrl1("http://www.url1.com/");
  const GURL kUrl2("http://www.url2.com/");
  const GURL kUrl3("http://www.url3.com/");

  const base::Uuid kFolder2Uuid = base::Uuid::GenerateRandomV4();

  // -------- Local bookmarks --------
  // bookmark_bar
  //  | - folder 1 (kFolder1Title)
  //  | - folder 2 (kFolder2Uuid/kFolder2Title)
  //    |- url1(http://www.url1.com)
  //    |- url2(http://www.url2.com)
  std::unique_ptr<BookmarkModelView> local_model =
      BuildLocalModel({FolderBuilder(kFolder1Title),
                       FolderBuilder(kFolder2Title)
                           .SetUuid(kFolder2Uuid)
                           .SetChildren({UrlBuilder(kUrl1Title, kUrl1),
                                         UrlBuilder(kUrl2Title, kUrl2)})});

  // -------- Account bookmarks --------
  // bookmark_bar
  //  | - folder 3 (kFolder3Title)
  //    | - folder 2 (kFolder2Uuid/kFolder2Title)
  //      |- url2(http://www.url1.com)
  //      |- url3(http://www.url3.com)
  std::unique_ptr<BookmarkModelView> account_model = BuildAccountModel(
      {FolderBuilder(kFolder3Title)
           .SetChildren({FolderBuilder(kFolder2Title)
                             .SetUuid(kFolder2Uuid)
                             .SetChildren({UrlBuilder(kUrl2Title, kUrl2),
                                           UrlBuilder(kUrl3Title, kUrl3)})})});

  // -------- The expected merge outcome --------
  // bookmark_bar
  //  | - folder 3 (kFolder3Title)
  //    | - folder 2 (kFolder2Uuid/kTitle2)
  //      |- url2(http://www.url2.com)
  //      |- url3(http://www.url3.com)
  //      |- url1(http://www.url1.com)
  //  | - folder 1 (kTitle1)
  LocalBookmarkModelMerger(local_model.get(), account_model.get()).Merge();

  EXPECT_THAT(
      account_model->bookmark_bar_node()->children(),
      ElementsAre(IsFolder(kFolder3Title,
                           ElementsAre(IsFolder(
                               kFolder2Title,
                               ElementsAre(IsUrlBookmark(kUrl2Title, kUrl2),
                                           IsUrlBookmark(kUrl3Title, kUrl3),
                                           IsUrlBookmark(kUrl1Title, kUrl1))))),
                  IsFolder(kFolder1Title, IsEmpty())));
}

TEST_F(LocalBookmarkModelMergerTest,
       ShouldDeduplicateBySemanticsAfterNestedParentMatchedByUuid) {
  const std::u16string kFolder1Title = u"folder1";
  const std::u16string kFolder2Title = u"folder2";

  const std::u16string kUrl1Title = u"url1";
  const std::u16string kUrl2Title = u"url2";
  const std::u16string kUrl3Title = u"url3";

  const GURL kUrl1("http://www.url1.com/");
  const GURL kUrl2("http://www.url2.com/");
  const GURL kUrl3("http://www.url3.com/");

  const base::Uuid kFolder2Uuid = base::Uuid::GenerateRandomV4();

  // -------- Local bookmarks --------
  // bookmark_bar
  //  | - folder 1 (kFolder1Title)
  //    | - folder 2 (kFolder2Uuid/kFolder2Title)
  //      |- url1(http://www.url1.com)
  //      |- url2(http://www.url2.com)
  std::unique_ptr<BookmarkModelView> local_model = BuildLocalModel(
      {FolderBuilder(kFolder1Title)
           .SetChildren({FolderBuilder(kFolder2Title)
                             .SetUuid(kFolder2Uuid)
                             .SetChildren({UrlBuilder(kUrl1Title, kUrl1),
                                           UrlBuilder(kUrl2Title, kUrl2)})})});

  // -------- Account bookmarks --------
  // bookmark_bar
  //  | - folder 2 (kFolder2Uuid/kFolder2Title)
  //    |- url2(http://www.url2.com)
  //    |- url3(http://www.url3.com)
  std::unique_ptr<BookmarkModelView> account_model =
      BuildAccountModel({FolderBuilder(kFolder2Title)
                             .SetUuid(kFolder2Uuid)
                             .SetChildren({UrlBuilder(kUrl2Title, kUrl2),
                                           UrlBuilder(kUrl3Title, kUrl3)})});

  // -------- The expected merge outcome --------
  // bookmark_bar
  //  | - folder 2 (kFolder2Uuid/kTitle2)
  //    |- url2(http://www.url2.com)
  //    |- url3(http://www.url3.com)
  //    |- url1(http://www.url1.com)
  //  | - folder 1 (kTitle1)
  LocalBookmarkModelMerger(local_model.get(), account_model.get()).Merge();

  EXPECT_THAT(
      account_model->bookmark_bar_node()->children(),
      ElementsAre(IsFolder(kFolder2Title,
                           ElementsAre(IsUrlBookmark(kUrl2Title, kUrl2),
                                       IsUrlBookmark(kUrl3Title, kUrl3),
                                       IsUrlBookmark(kUrl1Title, kUrl1))),
                  IsFolder(kFolder1Title, IsEmpty())));
}

TEST_F(LocalBookmarkModelMergerTest,
       ShouldDeduplicateBySemanticsAfterTwoConsecutiveAncestorsMatchedByUuid) {
  const std::u16string kFolder1Title = u"folder1";
  const std::u16string kFolder2Title = u"folder2";
  const std::u16string kFolder3Title = u"folder3";

  const std::u16string kUrl1Title = u"url1";
  const std::u16string kUrl2Title = u"url2";
  const std::u16string kUrl3Title = u"url3";

  const GURL kUrl1("http://www.url1.com/");
  const GURL kUrl2("http://www.url2.com/");
  const GURL kUrl3("http://www.url3.com/");

  const base::Uuid kFolder2Uuid = base::Uuid::GenerateRandomV4();
  const base::Uuid kFolder3Uuid = base::Uuid::GenerateRandomV4();

  // -------- Local bookmarks --------
  // bookmark_bar
  //  | - folder 1 (kFolder1Title)
  //    | - folder 2 (kFolder2Uuid/kFolder2Title)
  //      | - folder 3 (kFolder3Uuid/kFolder3Title)
  //        |- url1(http://www.url1.com)
  //        |- url2(http://www.url2.com)
  std::unique_ptr<BookmarkModelView> local_model = BuildLocalModel(
      {FolderBuilder(kFolder1Title)
           .SetChildren(
               {FolderBuilder(kFolder2Title)
                    .SetUuid(kFolder2Uuid)
                    .SetChildren(
                        {FolderBuilder(kFolder3Title)
                             .SetUuid(kFolder3Uuid)
                             .SetChildren(
                                 {UrlBuilder(kUrl1Title, kUrl1),
                                  UrlBuilder(kUrl2Title, kUrl2)})})})});

  // -------- Account bookmarks --------
  // bookmark_bar
  //  | - folder 2 (kFolder2Uuid/kFolder2Title)
  //    | - folder 3 (kFolder3Uuid/kFolder3Title)
  //      |- url2(http://www.url2.com)
  //      |- url3(http://www.url3.com)
  std::unique_ptr<BookmarkModelView> account_model = BuildAccountModel(
      {FolderBuilder(kFolder2Title)
           .SetUuid(kFolder2Uuid)
           .SetChildren({FolderBuilder(kFolder3Title)
                             .SetUuid(kFolder3Uuid)
                             .SetChildren({UrlBuilder(kUrl2Title, kUrl2),
                                           UrlBuilder(kUrl3Title, kUrl3)})})});

  // -------- The expected merge outcome --------
  // bookmark_bar
  //  | - folder 2 (kFolder2Uuid/kTitle2)
  //    | - folder 3 (kFolder3Uuid/kTitle3)
  //      |- url2(http://www.url2.com)
  //      |- url3(http://www.url3.com)
  //      |- url1(http://www.url1.com)
  //  | - folder 1 (kTitle1)
  LocalBookmarkModelMerger(local_model.get(), account_model.get()).Merge();

  EXPECT_THAT(
      account_model->bookmark_bar_node()->children(),
      ElementsAre(IsFolder(kFolder2Title,
                           ElementsAre(IsFolder(
                               kFolder3Title,
                               ElementsAre(IsUrlBookmark(kUrl2Title, kUrl2),
                                           IsUrlBookmark(kUrl3Title, kUrl3),
                                           IsUrlBookmark(kUrl1Title, kUrl1))))),
                  IsFolder(kFolder1Title, IsEmpty())));
}

TEST_F(
    LocalBookmarkModelMergerTest,
    ShouldDeduplicateBySemanticsAfterTwoNonConsecutiveAncestorsMatchedByUuid) {
  const std::u16string kFolder1Title = u"folder1";
  const std::u16string kFolder2Title = u"folder2";
  const std::u16string kFolder3Title = u"folder3";
  const std::u16string kFolder4Title = u"folder4";
  const std::u16string kFolder5Title = u"folder5";

  const std::u16string kUrl1Title = u"url1";
  const std::u16string kUrl2Title = u"url2";
  const std::u16string kUrl3Title = u"url3";

  const GURL kUrl1("http://www.url1.com/");
  const GURL kUrl2("http://www.url2.com/");
  const GURL kUrl3("http://www.url3.com/");

  const base::Uuid kFolder2Uuid = base::Uuid::GenerateRandomV4();
  const base::Uuid kFolder5Uuid = base::Uuid::GenerateRandomV4();

  // -------- Local bookmarks --------
  // bookmark_bar
  //  | - folder 1 (kFolder1Title)
  //    | - folder 2 (kFolder2Uuid/kFolder2Title)
  //      | - folder 3 (kFolder3Title)
  //        | - folder 4 (kFolder4Title)
  //          | - folder 5 (kFolder5Uuid/kFolder5Title)
  //            |- url1(http://www.url1.com)
  //            |- url2(http://www.url2.com)
  std::unique_ptr<BookmarkModelView> local_model = BuildLocalModel(
      {FolderBuilder(kFolder1Title)
           .SetChildren(
               {FolderBuilder(kFolder2Title)
                    .SetUuid(kFolder2Uuid)
                    .SetChildren(
                        {FolderBuilder(kFolder3Title)
                             .SetChildren(
                                 {FolderBuilder(kFolder4Title)
                                      .SetChildren(
                                          {FolderBuilder(kFolder5Title)
                                               .SetUuid(kFolder5Uuid)
                                               .SetChildren(
                                                   {UrlBuilder(kUrl1Title,
                                                               kUrl1),
                                                    UrlBuilder(
                                                        kUrl2Title,
                                                        kUrl2)})})})})})});

  // -------- Account bookmarks --------
  // bookmark_bar
  //  | - folder 2 (kFolder2Uuid/kFolder2Title)
  //  | - folder 5 (kFolder5Uuid/kFolder5Title)
  //    |- url2(http://www.url2.com)
  //    |- url3(http://www.url3.com)
  std::unique_ptr<BookmarkModelView> account_model =
      BuildAccountModel({FolderBuilder(kFolder2Title).SetUuid(kFolder2Uuid),
                         FolderBuilder(kFolder5Title)
                             .SetUuid(kFolder5Uuid)
                             .SetChildren({UrlBuilder(kUrl2Title, kUrl2),
                                           UrlBuilder(kUrl3Title, kUrl3)})});

  // -------- The expected merge outcome --------
  // bookmark_bar
  //  | - folder 2 (kFolder2Uuid/kTitle2)
  //    | - folder 3 (kTitle3)
  //      | - folder 4 (kTitle4)
  //  | - folder 5 (kFolder5Uuid/kFolder5Title)
  //      |- url2(http://www.url2.com)
  //      |- url3(http://www.url3.com)
  //      |- url1(http://www.url1.com)
  //  | - folder 1 (kTitle1)
  LocalBookmarkModelMerger(local_model.get(), account_model.get()).Merge();

  EXPECT_THAT(
      account_model->bookmark_bar_node()->children(),
      ElementsAre(IsFolder(kFolder2Title,
                           ElementsAre(IsFolder(
                               kFolder3Title, ElementsAre(IsFolder(
                                                  kFolder4Title, IsEmpty()))))),
                  IsFolder(kFolder5Title,
                           ElementsAre(IsUrlBookmark(kUrl2Title, kUrl2),
                                       IsUrlBookmark(kUrl3Title, kUrl3),
                                       IsUrlBookmark(kUrl1Title, kUrl1))),
                  IsFolder(kFolder1Title, IsEmpty())));
}

TEST_F(LocalBookmarkModelMergerTest,
       ShouldMergeBookmarkByUuidDespiteDifferentParent) {
  const std::u16string kFolderTitle = u"Folder Title";
  const std::u16string kLocalTitle = u"Title 1";
  const std::u16string kAccountTitle = u"Title 2";
  const GURL kUrl("http://www.foo.com/");
  const base::Uuid kUuid = base::Uuid::GenerateRandomV4();

  // -------- The local model --------
  // bookmark_bar
  //  |- bookmark(kUuid/kLocalTitle)
  std::unique_ptr<BookmarkModelView> local_model =
      BuildLocalModel({UrlBuilder(kLocalTitle, kUrl).SetUuid(kUuid)});

  // -------- The account model --------
  // bookmark_bar
  //  | - folder
  //    | - bookmark(kUuid/kAccountTitle)
  std::unique_ptr<BookmarkModelView> account_model = BuildAccountModel(
      {FolderBuilder(kFolderTitle)
           .SetChildren({UrlBuilder(kAccountTitle, kUrl).SetUuid(kUuid)})});

  // -------- Exercise the merge logic --------
  LocalBookmarkModelMerger(local_model.get(), account_model.get()).Merge();

  // -------- The merged model --------
  // bookmark_bar
  //  | - folder
  //    | - bookmark(kUuid/kLocalTitle)
  EXPECT_THAT(
      account_model->bookmark_bar_node()->children(),
      ElementsAre(IsFolder(kFolderTitle, ElementsAre(IsUrlBookmarkWithUuid(
                                             kLocalTitle, kUrl, kUuid)))));
}

TEST_F(LocalBookmarkModelMergerTest,
       ShouldNotMergeBySemanticsIfDifferentParent) {
  const std::u16string kFolder1Title = u"folder1";
  const std::u16string kFolder2Title = u"folder2";

  const std::u16string kUrl1Title = u"url1";
  const std::u16string kUrl2Title = u"url2";

  const GURL kUrl1("http://www.url1.com/");
  const GURL kUrl2("http://www.url2.com/");

  // -------- The local model --------
  // bookmark_bar
  //  |- folder 1
  //    |- folder 2
  //      |- url1(http://www.url1.com)
  std::unique_ptr<BookmarkModelView> local_model = BuildLocalModel(
      {FolderBuilder(kFolder1Title)
           .SetChildren({FolderBuilder(kFolder2Title)
                             .SetChildren({UrlBuilder(kUrl1Title, kUrl1)})})});

  // -------- The account model --------
  // bookmark_bar
  //  |- folder 2
  //    |- url2(http://www.url2.com)
  std::unique_ptr<BookmarkModelView> account_model =
      BuildAccountModel({FolderBuilder(kFolder2Title)
                             .SetChildren({UrlBuilder(kUrl2Title, kUrl2)})});

  // -------- Exercise the merge logic --------
  LocalBookmarkModelMerger(local_model.get(), account_model.get()).Merge();

  // -------- The merged model --------
  // bookmark_bar
  //  |- folder 2
  //    |- url2(http://www.url2.com)
  //  |- folder 1
  //    |- folder 2
  //      |- url1(http://www.url1.com)
  EXPECT_THAT(
      account_model->bookmark_bar_node()->children(),
      ElementsAre(IsFolder(kFolder2Title,
                           ElementsAre(IsUrlBookmark(kUrl2Title, kUrl2))),
                  IsFolder(kFolder1Title,
                           ElementsAre(IsFolder(kFolder2Title,
                                                ElementsAre(IsUrlBookmark(
                                                    kUrl1Title, kUrl1)))))));
}

TEST_F(LocalBookmarkModelMergerTest, ShouldMergeFolderByUuidAndNotSemantics) {
  const std::u16string kTitle1 = u"Title 1";
  const std::u16string kTitle2 = u"Title 2";
  const GURL kUrl("http://www.foo.com/");
  const base::Uuid kUuid1 = base::Uuid::GenerateRandomV4();
  const base::Uuid kUuid2 = base::Uuid::GenerateRandomV4();

  // -------- The local model --------
  // bookmark_bar
  //  | - folder 1 (kUuid1/kTitle1)
  //    | - folder 2 (kUuid2/kTitle2)
  std::unique_ptr<BookmarkModelView> local_model =
      BuildLocalModel({FolderBuilder(kTitle1).SetUuid(kUuid1).SetChildren(
          {FolderBuilder(kTitle2).SetUuid(kUuid2)})});

  // -------- The account model --------
  // bookmark_bar
  //  | - folder (kUuid2/kTitle1)
  std::unique_ptr<BookmarkModelView> account_model =
      BuildAccountModel({FolderBuilder(kTitle1).SetUuid(kUuid2)});

  // -------- Exercise the merge logic --------
  LocalBookmarkModelMerger(local_model.get(), account_model.get()).Merge();

  // -------- The merged model --------
  // bookmark_bar
  //  | - folder 2 (kUuid2/kTitle2)
  //  | - folder 1 (kUuid1/kTitle1)
  //
  // The node should have been merged with its UUID match, even if the other
  // candidate matches by semantics.
  EXPECT_THAT(account_model->bookmark_bar_node()->children(),
              ElementsAre(IsFolderWithUuid(kTitle2, kUuid2, IsEmpty()),
                          IsFolderWithUuid(kTitle1, kUuid1, IsEmpty())));
}

TEST_F(
    LocalBookmarkModelMergerTest,
    ShouldIgnoreFolderSemanticsMatchAndLaterMatchByUuidWithSemanticsNodeFirst) {
  const std::u16string kLocalOnlyTitle = u"LocalOnlyTitle";
  const std::u16string kMatchingTitle = u"MatchingTitle";
  const base::Uuid kUuid1 = base::Uuid::GenerateRandomV4();
  const base::Uuid kUuid2 = base::Uuid::GenerateRandomV4();
  const GURL kUrl("http://foo.com/");
  const std::u16string kUrlTitle = u"Bookmark Title";

  // -------- The local model --------
  // bookmark_bar
  //  | - folder (kUuid1/kMatchingTitle)
  //  | - folder (kUuid2/kLocalOnlyTitle)
  //    | - bookmark
  std::unique_ptr<BookmarkModelView> local_model =
      BuildLocalModel({FolderBuilder(kMatchingTitle).SetUuid(kUuid1),
                       FolderBuilder(kLocalOnlyTitle)
                           .SetUuid(kUuid2)
                           .SetChildren({UrlBuilder(kUrlTitle, kUrl)})});

  // -------- The account model --------
  // bookmark_bar
  //  | - folder (kUuid2/kMatchingTitle)
  std::unique_ptr<BookmarkModelView> account_model =
      BuildAccountModel({FolderBuilder(kMatchingTitle).SetUuid(kUuid2)});

  // -------- Exercise the merge logic --------
  LocalBookmarkModelMerger(local_model.get(), account_model.get()).Merge();

  // -------- The merged model --------
  // bookmark_bar
  //  | - folder (kUuid2/kLocalOnlyTitle)
  //    | - bookmark
  //  | - folder (kUuid1/kMatchingTitle)
  //
  // The node should have been merged with its UUID match, even if the other
  // candidate matches by semantics.
  EXPECT_THAT(
      account_model->bookmark_bar_node()->children(),
      ElementsAre(IsFolderWithUuid(kLocalOnlyTitle, kUuid2,
                                   ElementsAre(IsUrlBookmark(kUrlTitle, kUrl))),
                  IsFolderWithUuid(kMatchingTitle, kUuid1, IsEmpty())));
}

TEST_F(LocalBookmarkModelMergerTest,
       ShouldIgnoreFolderSemanticsMatchAndLaterMatchByUuidWithUuidNodeFirst) {
  const std::u16string kLocalOnlyTitle = u"LocalOnlyTitle";
  const std::u16string kMatchingTitle = u"MatchingTitle";
  const base::Uuid kUuid1 = base::Uuid::GenerateRandomV4();
  const base::Uuid kUuid2 = base::Uuid::GenerateRandomV4();
  const GURL kUrl("http://foo.com/");
  const std::u16string kUrlTitle = u"Bookmark Title";

  // -------- The local model --------
  // bookmark_bar
  //  | - folder (kUuid2/kLocalOnlyTitle)
  //    | - bookmark
  //  | - folder (kUuid1/kMatchingTitle)
  std::unique_ptr<BookmarkModelView> local_model =
      BuildLocalModel({FolderBuilder(kLocalOnlyTitle)
                           .SetUuid(kUuid2)
                           .SetChildren({UrlBuilder(kUrlTitle, kUrl)}),
                       FolderBuilder(kMatchingTitle).SetUuid(kUuid1)});

  // -------- The account model --------
  // bookmark_bar
  //  | - folder (kUuid2/kMatchingTitle)
  std::unique_ptr<BookmarkModelView> account_model =
      BuildAccountModel({FolderBuilder(kMatchingTitle).SetUuid(kUuid2)});

  // -------- Exercise the merge logic --------
  LocalBookmarkModelMerger(local_model.get(), account_model.get()).Merge();

  // -------- The merged model --------
  // bookmark_bar
  //  | - folder (kUuid2/kLocalOnlyTitle)
  //    | - bookmark
  //  | - folder (kUuid1/kMatchingTitle)
  //
  // The node should have been merged with its UUID match, even if the other
  // candidate matches by semantics.
  EXPECT_THAT(
      account_model->bookmark_bar_node()->children(),
      ElementsAre(IsFolderWithUuid(kLocalOnlyTitle, kUuid2,
                                   ElementsAre(IsUrlBookmark(kUrlTitle, kUrl))),
                  IsFolderWithUuid(kMatchingTitle, kUuid1, IsEmpty())));
}

TEST_F(LocalBookmarkModelMergerTest,
       ShouldReplaceBookmarkUuidWithConflictingURLs) {
  const std::u16string kTitle = u"Title";
  const GURL kUrl1("http://www.foo.com/");
  const GURL kUrl2("http://www.bar.com/");
  const base::Uuid kUuid = base::Uuid::GenerateRandomV4();

  // -------- The local model --------
  // bookmark_bar
  //  | - bookmark (kUuid/kUrl1)
  std::unique_ptr<BookmarkModelView> local_model =
      BuildLocalModel({UrlBuilder(kTitle, kUrl1).SetUuid(kUuid)});

  // -------- The account model --------
  // bookmark_bar
  //  | - bookmark (kUuid/kUrl2)
  std::unique_ptr<BookmarkModelView> account_model =
      BuildAccountModel({UrlBuilder(kTitle, kUrl2).SetUuid(kUuid)});

  // -------- Exercise the merge logic --------
  LocalBookmarkModelMerger(local_model.get(), account_model.get()).Merge();

  // -------- The merged model --------
  // bookmark_bar
  //  | - bookmark (kUuid/kUrl2)
  //  | - bookmark ([new UUID]/kUrl1)
  //
  // The conflicting node UUID should have been replaced.
  EXPECT_THAT(account_model->bookmark_bar_node()->children(),
              ElementsAre(IsUrlBookmarkWithUuid(kTitle, kUrl2, kUuid),
                          IsUrlBookmarkWithUuid(kTitle, kUrl1, Ne(kUuid))));
}

TEST_F(LocalBookmarkModelMergerTest,
       ShouldReplaceBookmarkUuidWithConflictingTypes) {
  const GURL kUrl1("http://www.foo.com/");
  const std::u16string kTitle = u"Title";
  const base::Uuid kUuid = base::Uuid::GenerateRandomV4();

  // -------- The local model --------
  // bookmark_bar
  //  | - bookmark (kUuid/kUrl1)
  std::unique_ptr<BookmarkModelView> local_model =
      BuildLocalModel({UrlBuilder(kTitle, kUrl1).SetUuid(kUuid)});

  // -------- The account model --------
  // bookmark_bar
  //  | - folder(kUuid)
  std::unique_ptr<BookmarkModelView> account_model =
      BuildAccountModel({FolderBuilder(kTitle).SetUuid(kUuid)});

  // -------- Exercise the merge logic --------
  LocalBookmarkModelMerger(local_model.get(), account_model.get()).Merge();

  // -------- The merged model --------
  // bookmark_bar
  //  | - folder (kUuid)
  //  | - bookmark ([new UUID])
  //
  // The conflicting node UUID should have been replaced.
  EXPECT_THAT(account_model->bookmark_bar_node()->children(),
              ElementsAre(IsFolderWithUuid(kTitle, kUuid, IsEmpty()),
                          IsUrlBookmarkWithUuid(kTitle, kUrl1, Ne(kUuid))));
}

TEST_F(LocalBookmarkModelMergerTest,
       ShouldReplaceBookmarkUuidWithConflictingTypesAndLocalChildren) {
  const std::u16string kFolderTitle = u"Folder Title";
  const std::u16string kUrl1Title = u"url1";
  const std::u16string kUrl2Title = u"url2";
  const GURL kUrl1("http://www.url1.com/");
  const GURL kUrl2("http://www.url2.com/");
  const base::Uuid kUuid = base::Uuid::GenerateRandomV4();

  // -------- The local model --------
  // bookmark_bar
  //  | - folder (kUuid)
  //    | - bookmark (kUrl1)
  std::unique_ptr<BookmarkModelView> local_model =
      BuildLocalModel({FolderBuilder(kFolderTitle)
                           .SetUuid(kUuid)
                           .SetChildren({UrlBuilder(kUrl1Title, kUrl1)})});

  // -------- The account model --------
  // bookmark_bar
  //  | - bookmark (kUuid/kUrl2)
  std::unique_ptr<BookmarkModelView> account_model =
      BuildAccountModel({UrlBuilder(kUrl2Title, kUrl2).SetUuid(kUuid)});

  // -------- Exercise the merge logic --------
  LocalBookmarkModelMerger(local_model.get(), account_model.get()).Merge();

  // -------- The merged model --------
  // bookmark_bar
  //  | - bookmark (kUuid/kUrl2)
  //  | - folder ([new UUID])
  //    | - bookmark (kUrl1)
  //
  // The conflicting node UUID should have been replaced.
  EXPECT_THAT(account_model->bookmark_bar_node()->children(),
              ElementsAre(IsUrlBookmarkWithUuid(kUrl2Title, kUrl2, kUuid),
                          IsFolderWithUuid(
                              kFolderTitle, Ne(kUuid),
                              ElementsAre(IsUrlBookmark(kUrl1Title, kUrl1)))));
}

}  // namespace

}  // namespace sync_bookmarks
