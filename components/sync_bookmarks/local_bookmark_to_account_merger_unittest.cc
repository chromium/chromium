// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_bookmarks/local_bookmark_to_account_merger.h"

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

  void Build(bookmarks::BookmarkModel* model,
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

  static void AddChildTo(bookmarks::BookmarkModel* model,
                         const bookmarks::BookmarkNode* parent,
                         const FolderOrUrl& folder_or_url) {
    if (absl::holds_alternative<UrlBuilder>(folder_or_url)) {
      absl::get<UrlBuilder>(folder_or_url).Build(model, parent);
    } else {
      CHECK(absl::holds_alternative<FolderBuilder>(folder_or_url));
      absl::get<FolderBuilder>(folder_or_url).Build(model, parent);
    }
  }

  static void AddChildrenTo(bookmarks::BookmarkModel* model,
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

  void Build(bookmarks::BookmarkModel* model,
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

class LocalBookmarkToAccountMergerTest : public testing::Test {
 protected:
  LocalBookmarkToAccountMergerTest() {
    model_->CreateAccountPermanentFolders();

    // TODO(crbug.com/332532186): Disallow deletions by default by installing a
    // mock observer.
  }

  ~LocalBookmarkToAccountMergerTest() override = default;

  void AddLocalNodes(
      const std::vector<FolderBuilder::FolderOrUrl>& children_of_bookmark_bar) {
    FolderBuilder::AddChildrenTo(model_.get(), model_->bookmark_bar_node(),
                                 children_of_bookmark_bar);
  }

  void AddAccountNodes(
      const std::vector<FolderBuilder::FolderOrUrl>& children_of_bookmark_bar) {
    FolderBuilder::AddChildrenTo(model_.get(),
                                 model_->account_bookmark_bar_node(),
                                 children_of_bookmark_bar);
  }

  base::test::ScopedFeatureList feature_list_{
      syncer::kSyncEnableBookmarksInTransportMode};
  const std::unique_ptr<bookmarks::BookmarkModel> model_ =
      bookmarks::TestBookmarkClient::CreateModel();
};

TEST_F(LocalBookmarkToAccountMergerTest,
       ShouldUploadLocalNodesIfNoAccountNodes) {
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

  // -------- Local bookmarks --------
  // bookmark_bar
  //  |- folder 1
  //    |- url1(http://www.url1.com)
  //    |- url2(http://www.url2.com)
  //  |- folder 2
  //    |- url3(http://www.url3.com)
  //    |- url4(http://www.url4.com)
  AddLocalNodes({FolderBuilder(kFolder1Title)
                     .SetChildren({UrlBuilder(kUrl1Title, kUrl1),
                                   UrlBuilder(kUrl2Title, kUrl2)}),
                 FolderBuilder(kFolder2Title)
                     .SetChildren({UrlBuilder(kUrl3Title, kUrl3),
                                   UrlBuilder(kUrl4Title, kUrl4)})});

  // -------- Account bookmarks --------
  // bookmark_bar
  AddAccountNodes({});

  // -------- Exercise the merge logic --------
  LocalBookmarkToAccountMerger(model_.get()).MoveAndMerge();

  EXPECT_THAT(model_->bookmark_bar_node()->children(), IsEmpty());

  // -------- The expected merge outcome --------
  // Same as the local model described above.
  EXPECT_THAT(
      model_->account_bookmark_bar_node()->children(),
      ElementsAre(IsFolder(kFolder1Title,
                           ElementsAre(IsUrlBookmark(kUrl1Title, kUrl1),
                                       IsUrlBookmark(kUrl2Title, kUrl2))),
                  IsFolder(kFolder2Title,
                           ElementsAre(IsUrlBookmark(kUrl3Title, kUrl3),
                                       IsUrlBookmark(kUrl4Title, kUrl4)))));
}

TEST_F(LocalBookmarkToAccountMergerTest, ShouldIgnoreManagedNodes) {
  const std::u16string kUrl1Title = u"url1";
  const std::u16string kUrl2Title = u"url2";

  const GURL kUrl1("http://www.url1.com/");
  const GURL kUrl2("http://www.url2.com/");

  auto local_client = std::make_unique<bookmarks::TestBookmarkClient>();
  bookmarks::BookmarkNode* managed_node = local_client->EnableManagedNode();
  std::unique_ptr<bookmarks::BookmarkModel> model =
      bookmarks::TestBookmarkClient::CreateModelWithClient(
          std::move(local_client));
  model->CreateAccountPermanentFolders();

  // -------- Local bookmarks --------
  // bookmark_bar
  //  |- url1(http://www.url1.com)
  // managed_bookmarks
  //  |- url2(http://www.url2.com)
  FolderBuilder::AddChildrenTo(model.get(), model->bookmark_bar_node(),
                               {UrlBuilder(kUrl1Title, kUrl1)});
  FolderBuilder::AddChildrenTo(model.get(), managed_node,
                               {UrlBuilder(kUrl2Title, kUrl2)});
  ASSERT_THAT(managed_node->children(),
              ElementsAre(IsUrlBookmark(kUrl2Title, kUrl2)));

  // -------- Account bookmarks --------
  // bookmark_bar
  FolderBuilder::AddChildrenTo(model.get(), model->account_bookmark_bar_node(),
                               {});

  // -------- Exercise the merge logic --------
  LocalBookmarkToAccountMerger(model.get()).MoveAndMerge();

  EXPECT_THAT(model->bookmark_bar_node()->children(), IsEmpty());

  // -------- The expected merge outcome --------
  // bookmark_bar
  //  |- url1(http://www.url1.com)
  //
  ASSERT_THAT(model->account_bookmark_bar_node()->children(),
              ElementsAre(IsUrlBookmark(kUrl1Title, kUrl1)));

  // Managed nodes should be excluded from the merge and be left unmodified.
  ASSERT_THAT(managed_node->children(),
              ElementsAre(IsUrlBookmark(kUrl2Title, kUrl2)));
  EXPECT_THAT(model->GetNodesByURL(kUrl2),
              ElementsAre(managed_node->children()[0].get()));
}

TEST_F(LocalBookmarkToAccountMergerTest, ShouldUploadLocalUuid) {
  const std::u16string kUrl1Title = u"url1";
  const GURL kUrl1("http://www.url1.com/");
  const base::Uuid kUrl1Uuid = base::Uuid::GenerateRandomV4();

  // -------- Local bookmarks --------
  // bookmark_bar
  //  | - bookmark(kUuid/kLocalTitle)
  AddLocalNodes({UrlBuilder(kUrl1Title, kUrl1).SetUuid(kUrl1Uuid)});

  // -------- Account bookmarks --------
  // bookmark_bar
  AddAccountNodes({});

  // -------- Exercise the merge logic --------
  LocalBookmarkToAccountMerger(model_.get()).MoveAndMerge();

  EXPECT_THAT(model_->bookmark_bar_node()->children(), IsEmpty());

  // -------- The expected merge outcome --------
  // Same as the local model described above, including the UUID.
  EXPECT_THAT(model_->account_bookmark_bar_node()->children(),
              ElementsAre(IsUrlBookmarkWithUuid(kUrl1Title, kUrl1, kUrl1Uuid)));
}

TEST_F(LocalBookmarkToAccountMergerTest, ShouldDeduplicateBySemantics) {
  const std::u16string kFolder1Title = u"folder1";

  const std::u16string kUrl1Title = u"url1";
  const std::u16string kUrl2Title = u"url2";
  const std::u16string kUrl3Title = u"url3";

  const GURL kUrl1("http://www.url1.com/");
  const GURL kUrl2("http://www.url2.com/");
  const GURL kUrl3("http://www.url3.com/");

  // -------- Local bookmarks --------
  // bookmark_bar
  //  |- folder 1
  //    |- url1(http://www.url1.com)
  //    |- url2(http://www.url2.com)
  AddLocalNodes({FolderBuilder(kFolder1Title)
                     .SetChildren({UrlBuilder(kUrl1Title, kUrl1),
                                   UrlBuilder(kUrl2Title, kUrl2)})});

  // -------- Account bookmarks --------
  // bookmark_bar
  //  |- folder 1
  //    |- url2(http://www.url2.com)
  //    |- url3(http://www.url3.com)
  AddAccountNodes({FolderBuilder(kFolder1Title)
                       .SetChildren({UrlBuilder(kUrl2Title, kUrl2),
                                     UrlBuilder(kUrl3Title, kUrl3)})});

  // -------- Exercise the merge logic --------
  LocalBookmarkToAccountMerger(model_.get()).MoveAndMerge();

  EXPECT_THAT(model_->bookmark_bar_node()->children(), IsEmpty());

  // -------- The expected merge outcome --------
  // bookmark_bar
  //  |- folder 1
  //    |- url2(http://www.url2.com)
  //    |- url3(http://www.url3.com)
  //    |- url1(http://www.url1.com)
  EXPECT_THAT(
      model_->account_bookmark_bar_node()->children(),
      ElementsAre(IsFolder(kFolder1Title,
                           ElementsAre(IsUrlBookmark(kUrl2Title, kUrl2),
                                       IsUrlBookmark(kUrl3Title, kUrl3),
                                       IsUrlBookmark(kUrl1Title, kUrl1)))));
}

TEST_F(LocalBookmarkToAccountMergerTest, ShouldNotDeduplicateIfDifferentUrls) {
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

  // -------- Local bookmarks --------
  // bookmark_bar
  //  |- folder 1
  //    |- url1(http://www.url1.com)
  //    |- url2(http://www.url2.com)
  //  |- folder 2
  //    |- url3(http://www.url3.com)
  //    |- url4(http://www.url4.com)
  AddLocalNodes({FolderBuilder(kFolder1Title)
                     .SetChildren({UrlBuilder(kUrl1Title, kUrl1),
                                   UrlBuilder(kUrl2Title, kUrl2)}),
                 FolderBuilder(kFolder2Title)
                     .SetChildren({UrlBuilder(kUrl3Title, kUrl3),
                                   UrlBuilder(kUrl4Title, kUrl4)})});

  // -------- Account bookmarks --------
  // bookmark_bar
  //  |- folder 1
  //    |- url1(http://www.url1.com)
  //    |- url2(http://www.another-url2.com)
  //  |- folder 3
  //    |- url3(http://www.url3.com)
  //    |- url4(http://www.url4.com)
  AddAccountNodes({FolderBuilder(kFolder1Title)
                       .SetChildren({UrlBuilder(kUrl1Title, kUrl1),
                                     UrlBuilder(kUrl2Title, kAnotherUrl2)}),
                   FolderBuilder(kFolder3Title)
                       .SetChildren({UrlBuilder(kUrl3Title, kUrl3),
                                     UrlBuilder(kUrl4Title, kUrl4)})});

  // -------- Exercise the merge logic --------
  LocalBookmarkToAccountMerger(model_.get()).MoveAndMerge();

  EXPECT_THAT(model_->bookmark_bar_node()->children(), IsEmpty());

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
      model_->account_bookmark_bar_node()->children(),
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
TEST_F(LocalBookmarkToAccountMergerTest,
       ShouldDedupLocalAndAccountNodesWhenAccountHasLegacyTruncatedTitle) {
  const std::u16string kLocalLongTitle(300, 'A');
  const std::u16string kAccountTruncatedTitle(255, 'A');

  // -------- Local bookmarks --------
  AddLocalNodes({FolderBuilder(kLocalLongTitle)});

  // -------- Account bookmarks --------
  AddAccountNodes({FolderBuilder(kAccountTruncatedTitle)});

  // -------- Exercise the merge logic --------
  LocalBookmarkToAccountMerger(model_.get()).MoveAndMerge();

  EXPECT_THAT(model_->bookmark_bar_node()->children(), IsEmpty());

  // Both titles should have matched against each other.
  EXPECT_THAT(model_->account_bookmark_bar_node()->children(),
              ElementsAre(IsFolder(kLocalLongTitle, IsEmpty())));
}

// This test checks that local node with truncated title will merge with account
// node which has full title.
TEST_F(LocalBookmarkToAccountMergerTest,
       ShouldMergeLocalAndAccountNodesWhenLocalHasLegacyTruncatedTitle) {
  const std::u16string kAccountFullTitle(300, 'A');
  const std::u16string kLocalTruncatedTitle(255, 'A');

  // -------- Local bookmarks --------
  AddLocalNodes({FolderBuilder(kLocalTruncatedTitle)});

  // -------- Account bookmarks --------
  AddAccountNodes({FolderBuilder(kAccountFullTitle)});

  // -------- Exercise the merge logic --------
  LocalBookmarkToAccountMerger(model_.get()).MoveAndMerge();

  EXPECT_THAT(model_->bookmark_bar_node()->children(), IsEmpty());

  // Both titles should have matched against each other. Although the local
  // title is truncated, for simplicity of the algorithm and considering how
  // rare this scenario is, the local one wins.
  EXPECT_THAT(model_->account_bookmark_bar_node()->children(),
              ElementsAre(IsFolder(kLocalTruncatedTitle, IsEmpty())));
}

TEST_F(LocalBookmarkToAccountMergerTest, ShouldDeduplicateBookmarkByUuid) {
  const std::u16string kLocalTitle = u"Title 1";
  const std::u16string kAccountTitle = u"Title 2";
  const GURL kUrl("http://www.foo.com/");
  const base::Uuid kUuid = base::Uuid::GenerateRandomV4();

  // -------- Local bookmarks --------
  // bookmark_bar
  //  | - bookmark(kUuid/kLocalTitle)
  AddLocalNodes({UrlBuilder(kLocalTitle, kUrl).SetUuid(kUuid)});

  // -------- Account bookmarks --------
  // bookmark_bar
  //  | - bookmark(kUuid/kAccountTitle)
  AddAccountNodes({UrlBuilder(kAccountTitle, kUrl).SetUuid(kUuid)});

  // -------- Exercise the merge logic --------
  LocalBookmarkToAccountMerger(model_.get()).MoveAndMerge();

  EXPECT_THAT(model_->bookmark_bar_node()->children(), IsEmpty());

  // -------- The merged account nodes --------
  // bookmark_bar
  //  |- bookmark(kUuid/kLocalTitle)
  EXPECT_THAT(model_->account_bookmark_bar_node()->children(),
              ElementsAre(IsUrlBookmarkWithUuid(kLocalTitle, kUrl, kUuid)));
}

TEST_F(LocalBookmarkToAccountMergerTest,
       ShouldMergeBookmarkByUuidDespiteDifferentParent) {
  const std::u16string kFolderTitle = u"Folder Title";
  const std::u16string kLocalTitle = u"Title 1";
  const std::u16string kAccountTitle = u"Title 2";
  const GURL kUrl("http://www.foo.com/");
  const base::Uuid kUuid = base::Uuid::GenerateRandomV4();

  // -------- Local bookmarks --------
  // bookmark_bar
  //  |- bookmark(kUuid/kLocalTitle)
  AddLocalNodes({UrlBuilder(kLocalTitle, kUrl).SetUuid(kUuid)});

  // -------- Account bookmarks --------
  // bookmark_bar
  //  | - folder
  //    | - bookmark(kUuid/kAccountTitle)
  AddAccountNodes(
      {FolderBuilder(kFolderTitle)
           .SetChildren({UrlBuilder(kAccountTitle, kUrl).SetUuid(kUuid)})});

  // -------- Exercise the merge logic --------
  LocalBookmarkToAccountMerger(model_.get()).MoveAndMerge();

  EXPECT_THAT(model_->bookmark_bar_node()->children(), IsEmpty());

  // -------- The merged account nodes --------
  // bookmark_bar
  //  | - folder
  //    | - bookmark(kUuid/kLocalTitle)
  EXPECT_THAT(
      model_->account_bookmark_bar_node()->children(),
      ElementsAre(IsFolder(kFolderTitle, ElementsAre(IsUrlBookmarkWithUuid(
                                             kLocalTitle, kUrl, kUuid)))));
}

TEST_F(LocalBookmarkToAccountMergerTest,
       ShouldNotMergeBySemanticsIfDifferentParent) {
  const std::u16string kFolder1Title = u"folder1";
  const std::u16string kFolder2Title = u"folder2";

  const std::u16string kUrl1Title = u"url1";
  const std::u16string kUrl2Title = u"url2";

  const GURL kUrl1("http://www.url1.com/");
  const GURL kUrl2("http://www.url2.com/");

  // -------- Local bookmarks --------
  // bookmark_bar
  //  |- folder 1
  //    |- folder 2
  //      |- url1(http://www.url1.com)
  AddLocalNodes(
      {FolderBuilder(kFolder1Title)
           .SetChildren({FolderBuilder(kFolder2Title)
                             .SetChildren({UrlBuilder(kUrl1Title, kUrl1)})})});

  // -------- Account bookmarks --------
  // bookmark_bar
  //  |- folder 2
  //    |- url2(http://www.url2.com)
  AddAccountNodes({FolderBuilder(kFolder2Title)
                       .SetChildren({UrlBuilder(kUrl2Title, kUrl2)})});

  // -------- Exercise the merge logic --------
  LocalBookmarkToAccountMerger(model_.get()).MoveAndMerge();

  EXPECT_THAT(model_->bookmark_bar_node()->children(), IsEmpty());

  // -------- The merged account nodes --------
  // bookmark_bar
  //  |- folder 2
  //    |- url2(http://www.url2.com)
  //  |- folder 1
  //    |- folder 2
  //      |- url1(http://www.url1.com)
  EXPECT_THAT(
      model_->account_bookmark_bar_node()->children(),
      ElementsAre(IsFolder(kFolder2Title,
                           ElementsAre(IsUrlBookmark(kUrl2Title, kUrl2))),
                  IsFolder(kFolder1Title,
                           ElementsAre(IsFolder(kFolder2Title,
                                                ElementsAre(IsUrlBookmark(
                                                    kUrl1Title, kUrl1)))))));
}

TEST_F(LocalBookmarkToAccountMergerTest,
       ShouldMergeFolderByUuidAndNotSemantics) {
  const std::u16string kTitle1 = u"Title 1";
  const std::u16string kTitle2 = u"Title 2";
  const GURL kUrl("http://www.foo.com/");
  const base::Uuid kUuid1 = base::Uuid::GenerateRandomV4();
  const base::Uuid kUuid2 = base::Uuid::GenerateRandomV4();

  // -------- Local bookmarks --------
  // bookmark_bar
  //  | - folder 1 (kUuid1/kTitle1)
  //    | - folder 2 (kUuid2/kTitle2)
  AddLocalNodes({FolderBuilder(kTitle1).SetUuid(kUuid1).SetChildren(
      {FolderBuilder(kTitle2).SetUuid(kUuid2)})});

  // -------- Account bookmarks --------
  // bookmark_bar
  //  | - folder (kUuid2/kTitle1)
  AddAccountNodes({FolderBuilder(kTitle1).SetUuid(kUuid2)});

  // -------- Exercise the merge logic --------
  LocalBookmarkToAccountMerger(model_.get()).MoveAndMerge();

  EXPECT_THAT(model_->bookmark_bar_node()->children(), IsEmpty());

  // -------- The merged account nodes --------
  // bookmark_bar
  //  | - folder 2 (kUuid2/kTitle2)
  //  | - folder 1 (kUuid1/kTitle1)
  //
  // The node should have been merged with its UUID match, even if the other
  // candidate matches by semantics.
  EXPECT_THAT(model_->account_bookmark_bar_node()->children(),
              ElementsAre(IsFolderWithUuid(kTitle2, kUuid2, IsEmpty()),
                          IsFolderWithUuid(kTitle1, kUuid1, IsEmpty())));
}

TEST_F(
    LocalBookmarkToAccountMergerTest,
    ShouldIgnoreFolderSemanticsMatchAndLaterMatchByUuidWithSemanticsNodeFirst) {
  const std::u16string kLocalOnlyTitle = u"LocalOnlyTitle";
  const std::u16string kMatchingTitle = u"MatchingTitle";
  const base::Uuid kUuid1 = base::Uuid::GenerateRandomV4();
  const base::Uuid kUuid2 = base::Uuid::GenerateRandomV4();
  const GURL kUrl("http://foo.com/");
  const std::u16string kUrlTitle = u"Bookmark Title";

  // -------- Local bookmarks --------
  // bookmark_bar
  //  | - folder (kUuid1/kMatchingTitle)
  //  | - folder (kUuid2/kLocalOnlyTitle)
  //    | - bookmark
  AddLocalNodes({FolderBuilder(kMatchingTitle).SetUuid(kUuid1),
                 FolderBuilder(kLocalOnlyTitle)
                     .SetUuid(kUuid2)
                     .SetChildren({UrlBuilder(kUrlTitle, kUrl)})});

  // -------- Account bookmarks --------
  // bookmark_bar
  //  | - folder (kUuid2/kMatchingTitle)
  AddAccountNodes({FolderBuilder(kMatchingTitle).SetUuid(kUuid2)});

  // -------- Exercise the merge logic --------
  LocalBookmarkToAccountMerger(model_.get()).MoveAndMerge();

  EXPECT_THAT(model_->bookmark_bar_node()->children(), IsEmpty());

  // -------- The merged account nodes --------
  // bookmark_bar
  //  | - folder (kUuid2/kLocalOnlyTitle)
  //    | - bookmark
  //  | - folder (kUuid1/kMatchingTitle)
  //
  // The node should have been merged with its UUID match, even if the other
  // candidate matches by semantics.
  EXPECT_THAT(
      model_->account_bookmark_bar_node()->children(),
      ElementsAre(IsFolderWithUuid(kLocalOnlyTitle, kUuid2,
                                   ElementsAre(IsUrlBookmark(kUrlTitle, kUrl))),
                  IsFolderWithUuid(kMatchingTitle, kUuid1, IsEmpty())));
}

TEST_F(LocalBookmarkToAccountMergerTest,
       ShouldIgnoreFolderSemanticsMatchAndLaterMatchByUuidWithUuidNodeFirst) {
  const std::u16string kLocalOnlyTitle = u"LocalOnlyTitle";
  const std::u16string kMatchingTitle = u"MatchingTitle";
  const base::Uuid kUuid1 = base::Uuid::GenerateRandomV4();
  const base::Uuid kUuid2 = base::Uuid::GenerateRandomV4();
  const GURL kUrl("http://foo.com/");
  const std::u16string kUrlTitle = u"Bookmark Title";

  // -------- Local bookmarks --------
  // bookmark_bar
  //  | - folder (kUuid2/kLocalOnlyTitle)
  //    | - bookmark
  //  | - folder (kUuid1/kMatchingTitle)
  AddLocalNodes({FolderBuilder(kLocalOnlyTitle)
                     .SetUuid(kUuid2)
                     .SetChildren({UrlBuilder(kUrlTitle, kUrl)}),
                 FolderBuilder(kMatchingTitle).SetUuid(kUuid1)});

  // -------- Account bookmarks --------
  // bookmark_bar
  //  | - folder (kUuid2/kMatchingTitle)
  AddAccountNodes({FolderBuilder(kMatchingTitle).SetUuid(kUuid2)});

  // -------- Exercise the merge logic --------
  LocalBookmarkToAccountMerger(model_.get()).MoveAndMerge();

  EXPECT_THAT(model_->bookmark_bar_node()->children(), IsEmpty());

  // -------- The merged account nodes --------
  // bookmark_bar
  //  | - folder (kUuid2/kLocalOnlyTitle)
  //    | - bookmark
  //  | - folder (kUuid1/kMatchingTitle)
  //
  // The node should have been merged with its UUID match, even if the other
  // candidate matches by semantics.
  EXPECT_THAT(
      model_->account_bookmark_bar_node()->children(),
      ElementsAre(IsFolderWithUuid(kLocalOnlyTitle, kUuid2,
                                   ElementsAre(IsUrlBookmark(kUrlTitle, kUrl))),
                  IsFolderWithUuid(kMatchingTitle, kUuid1, IsEmpty())));
}

TEST_F(LocalBookmarkToAccountMergerTest,
       ShouldReplaceBookmarkUuidWithConflictingURLs) {
  const std::u16string kTitle = u"Title";
  const GURL kUrl1("http://www.foo.com/");
  const GURL kUrl2("http://www.bar.com/");
  const base::Uuid kUuid = base::Uuid::GenerateRandomV4();

  // -------- Local bookmarks --------
  // bookmark_bar
  //  | - bookmark (kUuid/kUrl1)
  AddLocalNodes({UrlBuilder(kTitle, kUrl1).SetUuid(kUuid)});

  // -------- Account bookmarks --------
  // bookmark_bar
  //  | - bookmark (kUuid/kUrl2)
  AddAccountNodes({UrlBuilder(kTitle, kUrl2).SetUuid(kUuid)});

  // -------- Exercise the merge logic --------
  LocalBookmarkToAccountMerger(model_.get()).MoveAndMerge();

  EXPECT_THAT(model_->bookmark_bar_node()->children(), IsEmpty());

  // -------- The merged account nodes --------
  // bookmark_bar
  //  | - bookmark (kUuid/kUrl2)
  //  | - bookmark ([new UUID]/kUrl1)
  //
  // The conflicting node UUID should have been replaced.
  EXPECT_THAT(model_->account_bookmark_bar_node()->children(),
              ElementsAre(IsUrlBookmarkWithUuid(kTitle, kUrl2, kUuid),
                          IsUrlBookmarkWithUuid(kTitle, kUrl1, Ne(kUuid))));
}

TEST_F(LocalBookmarkToAccountMergerTest,
       ShouldReplaceBookmarkUuidWithConflictingTypes) {
  const GURL kUrl1("http://www.foo.com/");
  const std::u16string kTitle = u"Title";
  const base::Uuid kUuid = base::Uuid::GenerateRandomV4();

  // -------- Local bookmarks --------
  // bookmark_bar
  //  | - bookmark (kUuid/kUrl1)
  AddLocalNodes({UrlBuilder(kTitle, kUrl1).SetUuid(kUuid)});

  // -------- Account bookmarks --------
  // bookmark_bar
  //  | - folder(kUuid)
  AddAccountNodes({FolderBuilder(kTitle).SetUuid(kUuid)});

  // -------- Exercise the merge logic --------
  LocalBookmarkToAccountMerger(model_.get()).MoveAndMerge();

  EXPECT_THAT(model_->bookmark_bar_node()->children(), IsEmpty());

  // -------- The merged account nodes --------
  // bookmark_bar
  //  | - folder (kUuid)
  //  | - bookmark ([new UUID])
  //
  // The conflicting node UUID should have been replaced.
  EXPECT_THAT(model_->account_bookmark_bar_node()->children(),
              ElementsAre(IsFolderWithUuid(kTitle, kUuid, IsEmpty()),
                          IsUrlBookmarkWithUuid(kTitle, kUrl1, Ne(kUuid))));
}

TEST_F(LocalBookmarkToAccountMergerTest,
       ShouldReplaceBookmarkUuidWithConflictingTypesAndLocalChildren) {
  const std::u16string kFolderTitle = u"Folder Title";
  const std::u16string kUrl1Title = u"url1";
  const std::u16string kUrl2Title = u"url2";
  const GURL kUrl1("http://www.url1.com/");
  const GURL kUrl2("http://www.url2.com/");
  const base::Uuid kUuid = base::Uuid::GenerateRandomV4();

  // -------- Local bookmarks --------
  // bookmark_bar
  //  | - folder (kUuid)
  //    | - bookmark (kUrl1)
  AddLocalNodes({FolderBuilder(kFolderTitle)
                     .SetUuid(kUuid)
                     .SetChildren({UrlBuilder(kUrl1Title, kUrl1)})});

  // -------- Account bookmarks --------
  // bookmark_bar
  //  | - bookmark (kUuid/kUrl2)
  AddAccountNodes({UrlBuilder(kUrl2Title, kUrl2).SetUuid(kUuid)});

  // -------- Exercise the merge logic --------
  LocalBookmarkToAccountMerger(model_.get()).MoveAndMerge();

  EXPECT_THAT(model_->bookmark_bar_node()->children(), IsEmpty());

  // -------- The merged account nodes --------
  // bookmark_bar
  //  | - bookmark (kUuid/kUrl2)
  //  | - folder ([new UUID])
  //    | - bookmark (kUrl1)
  //
  // The conflicting node UUID should have been replaced.
  EXPECT_THAT(model_->account_bookmark_bar_node()->children(),
              ElementsAre(IsUrlBookmarkWithUuid(kUrl2Title, kUrl2, kUuid),
                          IsFolderWithUuid(
                              kFolderTitle, Ne(kUuid),
                              ElementsAre(IsUrlBookmark(kUrl1Title, kUrl1)))));
}

}  // namespace

}  // namespace sync_bookmarks
