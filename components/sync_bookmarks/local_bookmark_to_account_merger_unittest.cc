// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_bookmarks/local_bookmark_to_account_merger.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include "base/test/scoped_feature_list.h"
#include "base/uuid.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "components/bookmarks/browser/bookmark_test_util.h"
#include "components/bookmarks/test/mock_bookmark_model_observer.h"
#include "components/bookmarks/test/test_bookmark_client.h"
#include "components/bookmarks/test/test_matchers.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/sync_bookmarks/switches.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
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

// Test class to build bookmark folders conveniently and compactly in tests.
class FolderBuilder {
 public:
  using FolderOrUrl = std::variant<FolderBuilder, UrlBuilder>;

  static void AddChildTo(bookmarks::BookmarkModel* model,
                         const bookmarks::BookmarkNode* parent,
                         const FolderOrUrl& folder_or_url) {
    if (std::holds_alternative<UrlBuilder>(folder_or_url)) {
      std::get<UrlBuilder>(folder_or_url).Build(model, parent);
    } else {
      CHECK(std::holds_alternative<FolderBuilder>(folder_or_url));
      std::get<FolderBuilder>(folder_or_url).Build(model, parent);
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
    model_->AddObserver(&observer_);
    model_->CreateAccountPermanentFolders();
  }

  ~LocalBookmarkToAccountMergerTest() override {
    model_->RemoveObserver(&observer_);
  }

  void AddLocalNodes(
      const std::vector<FolderBuilder::FolderOrUrl>& children_of_bookmark_bar,
      const std::vector<FolderBuilder::FolderOrUrl>& children_of_mobile_node =
          {},
      const std::vector<FolderBuilder::FolderOrUrl>& children_of_other_node =
          {}) {
    FolderBuilder::AddChildrenTo(model_.get(), model_->bookmark_bar_node(),
                                 children_of_bookmark_bar);
    FolderBuilder::AddChildrenTo(model_.get(), model_->mobile_node(),
                                 children_of_mobile_node);
    FolderBuilder::AddChildrenTo(model_.get(), model_->other_node(),
                                 children_of_other_node);
  }

  void AddAccountNodes(
      const std::vector<FolderBuilder::FolderOrUrl>& children_of_bookmark_bar) {
    FolderBuilder::AddChildrenTo(model_.get(),
                                 model_->account_bookmark_bar_node(),
                                 children_of_bookmark_bar);
  }

  base::test::ScopedFeatureList feature_list_{
      switches::kSyncEnableBookmarksInTransportMode};
  const std::unique_ptr<bookmarks::BookmarkModel> model_ =
      bookmarks::TestBookmarkClient::CreateModel();
  testing::NiceMock<bookmarks::MockBookmarkModelObserver> observer_;
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

  // -------- The expected merge outcome --------
  // Same as the local model described above.
  EXPECT_CALL(observer_, BookmarkNodeAdded).Times(0);
  EXPECT_CALL(observer_, BookmarkNodeMoved).Times(2);
  EXPECT_CALL(observer_, BookmarkNodeRemoved).Times(0);
  EXPECT_CALL(observer_, BookmarkNodeChanged).Times(0);

  LocalBookmarkToAccountMerger(model_.get()).MoveAndMergeAllNodes();

  EXPECT_THAT(model_->bookmark_bar_node()->children(), IsEmpty());
  EXPECT_THAT(
      model_->account_bookmark_bar_node()->children(),
      ElementsAre(IsFolder(kFolder1Title,
                           ElementsAre(IsUrlBookmark(kUrl1Title, kUrl1),
                                       IsUrlBookmark(kUrl2Title, kUrl2))),
                  IsFolder(kFolder2Title,
                           ElementsAre(IsUrlBookmark(kUrl3Title, kUrl3),
                                       IsUrlBookmark(kUrl4Title, kUrl4)))));
}

TEST_F(LocalBookmarkToAccountMergerTest,
       ShouldUploadLocalNodesUnderAllPermanentNodes) {
  const std::u16string kUrl1Title = u"url1";
  const std::u16string kUrl2Title = u"url2";
  const std::u16string kUrl3Title = u"url3";

  const GURL kUrl1("http://www.url1.com/");
  const GURL kUrl2("http://www.url2.com/");
  const GURL kUrl3("http://www.url3.com/");

  // -------- Local bookmarks --------
  // bookmark_bar
  //  |- url1(http://www.url1.com)
  // mobile_node
  //  |- url2(http://www.url2.com)
  // other_node
  //  |- url3(http://www.url3.com)
  AddLocalNodes(/*children_of_bookmark_bar=*/{UrlBuilder(kUrl1Title, kUrl1)},
                /*children_of_mobile_node=*/{UrlBuilder(kUrl2Title, kUrl2)},
                /*children_of_other_node=*/{UrlBuilder(kUrl3Title, kUrl3)});

  // -------- Account bookmarks --------
  // bookmark_bar
  // mobile_node
  // other_node
  AddAccountNodes({});

  // -------- The expected merge outcome --------
  // Same as the local model described above.
  EXPECT_CALL(observer_, BookmarkNodeAdded).Times(0);
  EXPECT_CALL(observer_, BookmarkNodeMoved).Times(3);
  EXPECT_CALL(observer_, BookmarkNodeRemoved).Times(0);
  EXPECT_CALL(observer_, BookmarkNodeChanged).Times(0);

  LocalBookmarkToAccountMerger(model_.get()).MoveAndMergeAllNodes();

  EXPECT_THAT(model_->bookmark_bar_node()->children(), IsEmpty());
  EXPECT_THAT(model_->mobile_node()->children(), IsEmpty());
  EXPECT_THAT(model_->other_node()->children(), IsEmpty());
  EXPECT_THAT(model_->account_bookmark_bar_node()->children(),
              ElementsAre(IsUrlBookmark(kUrl1Title, kUrl1)));
  EXPECT_THAT(model_->account_mobile_node()->children(),
              ElementsAre(IsUrlBookmark(kUrl2Title, kUrl2)));
  EXPECT_THAT(model_->account_other_node()->children(),
              ElementsAre(IsUrlBookmark(kUrl3Title, kUrl3)));
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

  // -------- The expected merge outcome --------
  // bookmark_bar
  //  |- url1(http://www.url1.com)
  //
  LocalBookmarkToAccountMerger(model.get()).MoveAndMergeAllNodes();

  ASSERT_THAT(model->bookmark_bar_node()->children(), IsEmpty());
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

  // -------- The expected merge outcome --------
  // Same as the local model described above, including the UUID.
  EXPECT_CALL(observer_, BookmarkNodeAdded).Times(0);
  EXPECT_CALL(observer_, BookmarkNodeMoved).Times(1);
  EXPECT_CALL(observer_, BookmarkNodeRemoved).Times(0);
  EXPECT_CALL(observer_, BookmarkNodeChanged).Times(0);

  LocalBookmarkToAccountMerger(model_.get()).MoveAndMergeAllNodes();

  EXPECT_THAT(model_->bookmark_bar_node()->children(), IsEmpty());
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

  // -------- The expected merge outcome --------
  // bookmark_bar
  //  |- folder 1
  //    |- url2(http://www.url2.com)
  //    |- url3(http://www.url3.com)
  //    |- url1(http://www.url1.com)
  EXPECT_CALL(observer_, BookmarkNodeAdded).Times(0);
  EXPECT_CALL(observer_, BookmarkNodeMoved).Times(1);
  EXPECT_CALL(observer_, BookmarkNodeRemoved).Times(2);
  EXPECT_CALL(observer_, BookmarkNodeChanged).Times(0);

  LocalBookmarkToAccountMerger(model_.get()).MoveAndMergeAllNodes();

  EXPECT_THAT(model_->bookmark_bar_node()->children(), IsEmpty());
  EXPECT_THAT(
      model_->account_bookmark_bar_node()->children(),
      ElementsAre(IsFolder(kFolder1Title,
                           ElementsAre(IsUrlBookmark(kUrl2Title, kUrl2),
                                       IsUrlBookmark(kUrl3Title, kUrl3),
                                       IsUrlBookmark(kUrl1Title, kUrl1)))));
}

TEST_F(LocalBookmarkToAccountMergerTest,
       ShouldDeduplicateBySemanticsWhenSelected) {
  const std::u16string kFolder1Title = u"folder1";

  const std::u16string kUrl1Title = u"url1";
  const std::u16string kUrl2Title = u"url2";
  const std::u16string kUrl3Title = u"url3";

  const GURL kUrl1("http://www.url1.com/");
  const GURL kUrl2("http://www.url2.com/");
  const GURL kUrl3("http://www.url3.com/");

  // -------- Local bookmarks --------
  // bookmark_bar
  //  |- url1(http://www.url1.com)
  //  |- url2(http://www.url2.com)
  AddLocalNodes({UrlBuilder(kUrl1Title, kUrl1), UrlBuilder(kUrl2Title, kUrl2)});

  // -------- Account bookmarks --------
  // bookmark_bar
  //  |- url2(http://www.url2.com)
  //  |- url3(http://www.url3.com)
  AddAccountNodes(
      {UrlBuilder(kUrl2Title, kUrl2), UrlBuilder(kUrl3Title, kUrl3)});

  // -------- The expected merge outcome --------
  // Move(url2)
  //
  // ---- Local bookmarks ----
  //  |- url1(http://www.url1.com)
  //
  // ---- Account bookmarks ----
  // bookmark_bar
  //  |- url2(http://www.url2.com)
  //  |- url3(http://www.url3.com)
  EXPECT_CALL(observer_, BookmarkNodeAdded).Times(0);
  EXPECT_CALL(observer_, BookmarkNodeMoved).Times(0);
  EXPECT_CALL(observer_, BookmarkNodeRemoved).Times(1);
  EXPECT_CALL(observer_, BookmarkNodeChanged).Times(0);

  LocalBookmarkToAccountMerger(model_.get())
      .MoveAndMergeSpecificSubtrees(
          {model_->bookmark_bar_node()->children()[1]->id()});

  EXPECT_THAT(model_->bookmark_bar_node()->children(),
              ElementsAre(IsUrlBookmark(kUrl1Title, kUrl1)));
  EXPECT_THAT(model_->account_bookmark_bar_node()->children(),
              ElementsAre(IsUrlBookmark(kUrl2Title, kUrl2),
                          IsUrlBookmark(kUrl3Title, kUrl3)));
}

TEST_F(LocalBookmarkToAccountMergerTest, ShouldNotDeduplicateIfDifferentUrls) {
  const std::u16string kFolder1Title = u"folder1";
  const std::u16string kFolder2Title = u"folder2";
  const std::u16string kFolder3Title = u"folder3";
  const std::u16string kFolder4Title = u"folder4";

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
  //  |- folder 3
  AddLocalNodes({FolderBuilder(kFolder1Title)
                     .SetChildren({UrlBuilder(kUrl1Title, kUrl1),
                                   UrlBuilder(kUrl2Title, kUrl2)}),
                 FolderBuilder(kFolder2Title)
                     .SetChildren({UrlBuilder(kUrl3Title, kUrl3),
                                   UrlBuilder(kUrl4Title, kUrl4)}),
                 FolderBuilder(kFolder3Title)});

  // -------- Account bookmarks --------
  // bookmark_bar
  //  |- folder 1
  //    |- url1(http://www.url1.com)
  //    |- url2(http://www.another-url2.com)
  //  |- folder 4
  //    |- url3(http://www.url3.com)
  //    |- url4(http://www.url4.com)
  AddAccountNodes({FolderBuilder(kFolder1Title)
                       .SetChildren({UrlBuilder(kUrl1Title, kUrl1),
                                     UrlBuilder(kUrl2Title, kAnotherUrl2)}),
                   FolderBuilder(kFolder4Title)
                       .SetChildren({UrlBuilder(kUrl3Title, kUrl3),
                                     UrlBuilder(kUrl4Title, kUrl4)})});

  // -------- The expected merge outcome --------
  // bookmark_bar
  //  |- folder 1
  //    |- url1(http://www.url1.com)
  //    |- url2(http://www.another-url2.com)
  //    |- url2(http://www.url2.com)
  //  |- folder 4
  //    |- url3(http://www.url3.com)
  //    |- url4(http://www.url4.com)
  //  |- folder 2
  //    |- url3(http://www.url3.com)
  //    |- url4(http://www.url4.com)
  //  |- folder 3
  EXPECT_CALL(observer_, BookmarkNodeAdded).Times(0);
  EXPECT_CALL(observer_, BookmarkNodeMoved).Times(3);
  EXPECT_CALL(observer_, BookmarkNodeRemoved).Times(2);
  EXPECT_CALL(observer_, BookmarkNodeChanged).Times(0);

  LocalBookmarkToAccountMerger(model_.get()).MoveAndMergeAllNodes();

  EXPECT_THAT(model_->bookmark_bar_node()->children(), IsEmpty());
  EXPECT_THAT(
      model_->account_bookmark_bar_node()->children(),
      ElementsAre(IsFolder(kFolder1Title,
                           ElementsAre(IsUrlBookmark(kUrl1Title, kUrl1),
                                       IsUrlBookmark(kUrl2Title, kAnotherUrl2),
                                       IsUrlBookmark(kUrl2Title, kUrl2))),
                  IsFolder(kFolder4Title,
                           ElementsAre(IsUrlBookmark(kUrl3Title, kUrl3),
                                       IsUrlBookmark(kUrl4Title, kUrl4))),
                  IsFolder(kFolder2Title,
                           ElementsAre(IsUrlBookmark(kUrl3Title, kUrl3),
                                       IsUrlBookmark(kUrl4Title, kUrl4))),
                  IsFolder(kFolder3Title, IsEmpty())));
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

  // -------- The expected merge outcome --------
  EXPECT_CALL(observer_, BookmarkNodeAdded).Times(0);
  EXPECT_CALL(observer_, BookmarkNodeMoved).Times(0);
  EXPECT_CALL(observer_, BookmarkNodeRemoved).Times(1);
  EXPECT_CALL(observer_, BookmarkNodeChanged).Times(1);

  LocalBookmarkToAccountMerger(model_.get()).MoveAndMergeAllNodes();

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

  // -------- The expected merge outcome --------
  EXPECT_CALL(observer_, BookmarkNodeAdded).Times(0);
  EXPECT_CALL(observer_, BookmarkNodeMoved).Times(0);
  EXPECT_CALL(observer_, BookmarkNodeRemoved).Times(1);
  EXPECT_CALL(observer_, BookmarkNodeChanged).Times(1);

  LocalBookmarkToAccountMerger(model_.get()).MoveAndMergeAllNodes();

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

  // -------- The expected merge outcome --------
  // bookmark_bar
  //  |- bookmark(kUuid/kLocalTitle)
  EXPECT_CALL(observer_, BookmarkNodeAdded).Times(0);
  EXPECT_CALL(observer_, BookmarkNodeMoved).Times(0);
  EXPECT_CALL(observer_, BookmarkNodeRemoved).Times(1);
  EXPECT_CALL(observer_, BookmarkNodeChanged).Times(1);

  LocalBookmarkToAccountMerger(model_.get()).MoveAndMergeAllNodes();

  EXPECT_THAT(model_->bookmark_bar_node()->children(), IsEmpty());
  EXPECT_THAT(model_->account_bookmark_bar_node()->children(),
              ElementsAre(IsUrlBookmarkWithUuid(kLocalTitle, kUrl, kUuid)));
}

TEST_F(LocalBookmarkToAccountMergerTest,
       ShouldDeduplicateBookmarkByUuidWhenSelected) {
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

  // -------- The expected merge outcome --------
  // bookmark_bar
  //  |- bookmark(kUuid/kLocalTitle)
  EXPECT_CALL(observer_, BookmarkNodeAdded).Times(0);
  EXPECT_CALL(observer_, BookmarkNodeMoved).Times(0);
  EXPECT_CALL(observer_, BookmarkNodeRemoved).Times(1);
  EXPECT_CALL(observer_, BookmarkNodeChanged).Times(1);

  LocalBookmarkToAccountMerger(model_.get())
      .MoveAndMergeSpecificSubtrees(
          {model_->bookmark_bar_node()->children()[0]->id()});

  EXPECT_THAT(model_->bookmark_bar_node()->children(), IsEmpty());
  EXPECT_THAT(model_->account_bookmark_bar_node()->children(),
              ElementsAre(IsUrlBookmarkWithUuid(kLocalTitle, kUrl, kUuid)));
}

TEST_F(LocalBookmarkToAccountMergerTest,
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
  AddLocalNodes({FolderBuilder(kFolder1Title),
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
  AddAccountNodes(
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
  EXPECT_CALL(observer_, BookmarkNodeAdded).Times(0);
  EXPECT_CALL(observer_, BookmarkNodeMoved).Times(2);
  EXPECT_CALL(observer_, BookmarkNodeRemoved).Times(2);
  EXPECT_CALL(observer_, BookmarkNodeChanged).Times(0);

  LocalBookmarkToAccountMerger(model_.get()).MoveAndMergeAllNodes();

  EXPECT_THAT(model_->bookmark_bar_node()->children(), IsEmpty());
  EXPECT_THAT(
      model_->account_bookmark_bar_node()->children(),
      ElementsAre(IsFolder(kFolder3Title,
                           ElementsAre(IsFolder(
                               kFolder2Title,
                               ElementsAre(IsUrlBookmark(kUrl2Title, kUrl2),
                                           IsUrlBookmark(kUrl3Title, kUrl3),
                                           IsUrlBookmark(kUrl1Title, kUrl1))))),
                  IsFolder(kFolder1Title, IsEmpty())));
}

TEST_F(LocalBookmarkToAccountMergerTest,
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
  AddLocalNodes(
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
  AddAccountNodes({FolderBuilder(kFolder2Title)
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
  EXPECT_CALL(observer_, BookmarkNodeAdded).Times(0);
  EXPECT_CALL(observer_, BookmarkNodeMoved).Times(2);
  EXPECT_CALL(observer_, BookmarkNodeRemoved).Times(2);
  EXPECT_CALL(observer_, BookmarkNodeChanged).Times(0);

  LocalBookmarkToAccountMerger(model_.get()).MoveAndMergeAllNodes();

  EXPECT_THAT(model_->bookmark_bar_node()->children(), IsEmpty());
  EXPECT_THAT(
      model_->account_bookmark_bar_node()->children(),
      ElementsAre(IsFolder(kFolder2Title,
                           ElementsAre(IsUrlBookmark(kUrl2Title, kUrl2),
                                       IsUrlBookmark(kUrl3Title, kUrl3),
                                       IsUrlBookmark(kUrl1Title, kUrl1))),
                  IsFolder(kFolder1Title, IsEmpty())));
}

TEST_F(LocalBookmarkToAccountMergerTest,
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
  AddLocalNodes(
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
  AddAccountNodes(
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
  EXPECT_CALL(observer_, BookmarkNodeAdded).Times(0);
  EXPECT_CALL(observer_, BookmarkNodeMoved).Times(2);
  EXPECT_CALL(observer_, BookmarkNodeRemoved).Times(3);
  EXPECT_CALL(observer_, BookmarkNodeChanged).Times(0);

  LocalBookmarkToAccountMerger(model_.get()).MoveAndMergeAllNodes();

  EXPECT_THAT(model_->bookmark_bar_node()->children(), IsEmpty());
  EXPECT_THAT(
      model_->account_bookmark_bar_node()->children(),
      ElementsAre(IsFolder(kFolder2Title,
                           ElementsAre(IsFolder(
                               kFolder3Title,
                               ElementsAre(IsUrlBookmark(kUrl2Title, kUrl2),
                                           IsUrlBookmark(kUrl3Title, kUrl3),
                                           IsUrlBookmark(kUrl1Title, kUrl1))))),
                  IsFolder(kFolder1Title, IsEmpty())));
}

TEST_F(
    LocalBookmarkToAccountMergerTest,
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
  AddLocalNodes(
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
  AddAccountNodes({FolderBuilder(kFolder2Title).SetUuid(kFolder2Uuid),
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
  EXPECT_CALL(observer_, BookmarkNodeAdded).Times(0);
  EXPECT_CALL(observer_, BookmarkNodeMoved).Times(3);
  EXPECT_CALL(observer_, BookmarkNodeRemoved).Times(3);
  EXPECT_CALL(observer_, BookmarkNodeChanged).Times(0);

  LocalBookmarkToAccountMerger(model_.get()).MoveAndMergeAllNodes();

  EXPECT_THAT(model_->bookmark_bar_node()->children(), IsEmpty());
  EXPECT_THAT(
      model_->account_bookmark_bar_node()->children(),
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

  // -------- The merged account nodes --------
  // bookmark_bar
  //  | - folder
  //    | - bookmark(kUuid/kLocalTitle)
  EXPECT_CALL(observer_, BookmarkNodeAdded).Times(0);
  EXPECT_CALL(observer_, BookmarkNodeMoved).Times(0);
  EXPECT_CALL(observer_, BookmarkNodeRemoved).Times(1);
  EXPECT_CALL(observer_, BookmarkNodeChanged).Times(1);

  LocalBookmarkToAccountMerger(model_.get()).MoveAndMergeAllNodes();

  EXPECT_THAT(model_->bookmark_bar_node()->children(), IsEmpty());
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

  // -------- The merged account nodes --------
  // bookmark_bar
  //  |- folder 2
  //    |- url2(http://www.url2.com)
  //  |- folder 1
  //    |- folder 2
  //      |- url1(http://www.url1.com)
  EXPECT_CALL(observer_, BookmarkNodeAdded).Times(0);
  EXPECT_CALL(observer_, BookmarkNodeMoved).Times(1);
  EXPECT_CALL(observer_, BookmarkNodeRemoved).Times(0);
  EXPECT_CALL(observer_, BookmarkNodeChanged).Times(0);

  LocalBookmarkToAccountMerger(model_.get()).MoveAndMergeAllNodes();

  EXPECT_THAT(model_->bookmark_bar_node()->children(), IsEmpty());
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

  // -------- The merged account nodes --------
  // bookmark_bar
  //  | - folder 2 (kUuid2/kTitle2)
  //  | - folder 1 (kUuid1/kTitle1)
  //
  // The node should have been merged with its UUID match, even if the other
  // candidate matches by semantics.
  EXPECT_CALL(observer_, BookmarkNodeAdded).Times(0);
  EXPECT_CALL(observer_, BookmarkNodeMoved).Times(1);
  EXPECT_CALL(observer_, BookmarkNodeRemoved).Times(1);
  EXPECT_CALL(observer_, BookmarkNodeChanged).Times(1);

  LocalBookmarkToAccountMerger(model_.get()).MoveAndMergeAllNodes();

  EXPECT_THAT(model_->bookmark_bar_node()->children(), IsEmpty());
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

  // -------- The merged account nodes --------
  // bookmark_bar
  //  | - folder (kUuid2/kLocalOnlyTitle)
  //    | - bookmark
  //  | - folder (kUuid1/kMatchingTitle)
  //
  // The node should have been merged with its UUID match, even if the other
  // candidate matches by semantics.
  EXPECT_CALL(observer_, BookmarkNodeAdded).Times(0);
  EXPECT_CALL(observer_, BookmarkNodeMoved).Times(2);
  EXPECT_CALL(observer_, BookmarkNodeRemoved).Times(1);
  EXPECT_CALL(observer_, BookmarkNodeChanged).Times(1);

  LocalBookmarkToAccountMerger(model_.get()).MoveAndMergeAllNodes();

  EXPECT_THAT(model_->bookmark_bar_node()->children(), IsEmpty());
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

  // -------- The merged account nodes --------
  // bookmark_bar
  //  | - folder (kUuid2/kLocalOnlyTitle)
  //    | - bookmark
  //  | - folder (kUuid1/kMatchingTitle)
  //
  // The node should have been merged with its UUID match, even if the other
  // candidate matches by semantics.
  EXPECT_CALL(observer_, BookmarkNodeAdded).Times(0);
  EXPECT_CALL(observer_, BookmarkNodeMoved).Times(2);
  EXPECT_CALL(observer_, BookmarkNodeRemoved).Times(1);
  EXPECT_CALL(observer_, BookmarkNodeChanged).Times(1);

  LocalBookmarkToAccountMerger(model_.get()).MoveAndMergeAllNodes();

  EXPECT_THAT(model_->bookmark_bar_node()->children(), IsEmpty());
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

  // -------- The merged account nodes --------
  // bookmark_bar
  //  | - bookmark (kUuid/kUrl2)
  //  | - bookmark ([new UUID]/kUrl1)
  //
  // The conflicting node UUID should have been replaced.
  EXPECT_CALL(observer_, BookmarkNodeAdded).Times(0);
  EXPECT_CALL(observer_, BookmarkNodeMoved).Times(1);
  EXPECT_CALL(observer_, BookmarkNodeRemoved).Times(0);
  EXPECT_CALL(observer_, BookmarkNodeChanged).Times(0);

  LocalBookmarkToAccountMerger(model_.get()).MoveAndMergeAllNodes();

  EXPECT_THAT(model_->bookmark_bar_node()->children(), IsEmpty());
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

  // -------- The merged account nodes --------
  // bookmark_bar
  //  | - folder (kUuid)
  //  | - bookmark ([new UUID])
  //
  // The conflicting node UUID should have been replaced.
  EXPECT_CALL(observer_, BookmarkNodeAdded).Times(0);
  EXPECT_CALL(observer_, BookmarkNodeMoved).Times(1);
  EXPECT_CALL(observer_, BookmarkNodeRemoved).Times(0);
  EXPECT_CALL(observer_, BookmarkNodeChanged).Times(0);

  LocalBookmarkToAccountMerger(model_.get()).MoveAndMergeAllNodes();

  EXPECT_THAT(model_->bookmark_bar_node()->children(), IsEmpty());
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

  // -------- The merged account nodes --------
  // bookmark_bar
  //  | - bookmark (kUuid/kUrl2)
  //  | - folder ([new UUID])
  //    | - bookmark (kUrl1)
  //
  // The conflicting node UUID should have been replaced.
  EXPECT_CALL(observer_, BookmarkNodeAdded).Times(0);
  EXPECT_CALL(observer_, BookmarkNodeMoved).Times(1);
  EXPECT_CALL(observer_, BookmarkNodeRemoved).Times(0);
  EXPECT_CALL(observer_, BookmarkNodeChanged).Times(0);

  LocalBookmarkToAccountMerger(model_.get()).MoveAndMergeAllNodes();

  EXPECT_THAT(model_->bookmark_bar_node()->children(), IsEmpty());
  EXPECT_THAT(model_->account_bookmark_bar_node()->children(),
              ElementsAre(IsUrlBookmarkWithUuid(kUrl2Title, kUrl2, kUuid),
                          IsFolderWithUuid(
                              kFolderTitle, Ne(kUuid),
                              ElementsAre(IsUrlBookmark(kUrl1Title, kUrl1)))));
}

TEST_F(LocalBookmarkToAccountMergerTest, ShouldRemoveOneChildAtArbitraryIndex) {
  const std::u16string kUrl1Title = u"url1";
  const std::u16string kUrl2Title = u"url2";
  const std::u16string kUrl3Title = u"url3";
  const std::u16string kUrl4Title = u"url4";

  const GURL kUrl1("http://www.url1.com/");
  const GURL kUrl2("http://www.url2.com/");
  const GURL kUrl3("http://www.url3.com/");
  const GURL kUrl4("http://www.url4.com/");

  AddLocalNodes({UrlBuilder(kUrl1Title, kUrl1), UrlBuilder(kUrl2Title, kUrl2),
                 UrlBuilder(kUrl3Title, kUrl3), UrlBuilder(kUrl4Title, kUrl4)});

  LocalBookmarkToAccountMerger(model_.get())
      .RemoveChildrenAtForTesting(
          /*parent=*/model_->bookmark_bar_node(),
          /*indices_to_remove=*/{2}, FROM_HERE);

  EXPECT_THAT(model_->bookmark_bar_node()->children(),
              ElementsAre(IsUrlBookmark(kUrl1Title, kUrl1),
                          IsUrlBookmark(kUrl2Title, kUrl2),
                          IsUrlBookmark(kUrl4Title, kUrl4)));

  LocalBookmarkToAccountMerger(model_.get())
      .RemoveChildrenAtForTesting(
          /*parent=*/model_->bookmark_bar_node(),
          /*indices_to_remove=*/{0}, FROM_HERE);

  EXPECT_THAT(model_->bookmark_bar_node()->children(),
              ElementsAre(IsUrlBookmark(kUrl2Title, kUrl2),
                          IsUrlBookmark(kUrl4Title, kUrl4)));

  LocalBookmarkToAccountMerger(model_.get())
      .RemoveChildrenAtForTesting(
          /*parent=*/model_->bookmark_bar_node(),
          /*indices_to_remove=*/{1}, FROM_HERE);

  EXPECT_THAT(model_->bookmark_bar_node()->children(),
              ElementsAre(IsUrlBookmark(kUrl2Title, kUrl2)));
}

TEST_F(LocalBookmarkToAccountMergerTest,
       ShouldRemoveMultipleChildrenAtArbitraryIndices) {
  const std::u16string kUrl1Title = u"url1";
  const std::u16string kUrl2Title = u"url2";
  const std::u16string kUrl3Title = u"url3";
  const std::u16string kUrl4Title = u"url4";
  const std::u16string kUrl5Title = u"url5";
  const std::u16string kUrl6Title = u"url6";

  const GURL kUrl1("http://www.url1.com/");
  const GURL kUrl2("http://www.url2.com/");
  const GURL kUrl3("http://www.url3.com/");
  const GURL kUrl4("http://www.url4.com/");
  const GURL kUrl5("http://www.url5.com/");
  const GURL kUrl6("http://www.url6.com/");

  AddLocalNodes({UrlBuilder(kUrl1Title, kUrl1), UrlBuilder(kUrl2Title, kUrl2),
                 UrlBuilder(kUrl3Title, kUrl3), UrlBuilder(kUrl4Title, kUrl4),
                 UrlBuilder(kUrl5Title, kUrl5), UrlBuilder(kUrl6Title, kUrl6)});

  LocalBookmarkToAccountMerger(model_.get())
      .RemoveChildrenAtForTesting(
          /*parent=*/model_->bookmark_bar_node(),
          /*indices_to_remove=*/{0, 2, 3, 5}, FROM_HERE);

  EXPECT_THAT(model_->bookmark_bar_node()->children(),
              ElementsAre(IsUrlBookmark(kUrl2Title, kUrl2),
                          IsUrlBookmark(kUrl5Title, kUrl5)));
}

TEST_F(LocalBookmarkToAccountMergerTest, ShouldRemoveAllChildren) {
  const std::u16string kUrl1Title = u"url1";
  const std::u16string kUrl2Title = u"url2";
  const std::u16string kUrl3Title = u"url3";
  const std::u16string kUrl4Title = u"url4";

  const GURL kUrl1("http://www.url1.com/");
  const GURL kUrl2("http://www.url2.com/");
  const GURL kUrl3("http://www.url3.com/");
  const GURL kUrl4("http://www.url4.com/");

  AddLocalNodes({UrlBuilder(kUrl1Title, kUrl1), UrlBuilder(kUrl2Title, kUrl2),
                 UrlBuilder(kUrl3Title, kUrl3), UrlBuilder(kUrl4Title, kUrl4)});

  // When deleting all children, no reordering is expected.
  EXPECT_CALL(observer_, BookmarkNodeChildrenReordered).Times(0);

  LocalBookmarkToAccountMerger(model_.get())
      .RemoveChildrenAtForTesting(
          /*parent=*/model_->bookmark_bar_node(),
          /*indices_to_remove=*/{0, 1, 2, 3}, FROM_HERE);

  EXPECT_THAT(model_->bookmark_bar_node()->children(), IsEmpty());
}

TEST_F(LocalBookmarkToAccountMergerTest, ShouldDoNothingIfNoNodesSelected) {
  const std::u16string kUrl1Title = u"url1";
  const GURL kUrl1("http://www.url1.com/");

  // -------- Local bookmarks --------
  // bookmark_bar
  //  | - bookmark(kUuid/kLocalTitle)
  AddLocalNodes({UrlBuilder(kUrl1Title, kUrl1)});

  // -------- Account bookmarks --------
  // bookmark_bar
  AddAccountNodes({});

  // -------- The expected merge outcome --------
  // Unchanged
  EXPECT_CALL(observer_, BookmarkNodeAdded).Times(0);
  EXPECT_CALL(observer_, BookmarkNodeMoved).Times(0);
  EXPECT_CALL(observer_, BookmarkNodeRemoved).Times(0);
  EXPECT_CALL(observer_, BookmarkNodeChanged).Times(0);

  LocalBookmarkToAccountMerger(model_.get()).MoveAndMergeSpecificSubtrees({});

  EXPECT_THAT(model_->bookmark_bar_node()->children(),
              ElementsAre(IsUrlBookmark(kUrl1Title, kUrl1)));
  EXPECT_THAT(model_->account_bookmark_bar_node()->children(), IsEmpty());
}

TEST_F(LocalBookmarkToAccountMergerTest,
       ShouldDoNothingIfNonExistentIdSelected) {
  const std::u16string kUrl1Title = u"url1";
  const GURL kUrl1("http://www.url1.com/");

  // -------- Local bookmarks --------
  // bookmark_bar
  //  | - bookmark(kUuid/kLocalTitle)
  AddLocalNodes({UrlBuilder(kUrl1Title, kUrl1)});

  // -------- Account bookmarks --------
  // bookmark_bar
  AddAccountNodes({});

  // -------- The expected merge outcome --------
  // Unchanged
  EXPECT_CALL(observer_, BookmarkNodeAdded).Times(0);
  EXPECT_CALL(observer_, BookmarkNodeMoved).Times(0);
  EXPECT_CALL(observer_, BookmarkNodeRemoved).Times(0);
  EXPECT_CALL(observer_, BookmarkNodeChanged).Times(0);

  LocalBookmarkToAccountMerger(model_.get())
      .MoveAndMergeSpecificSubtrees({123456789});

  EXPECT_THAT(model_->bookmark_bar_node()->children(),
              ElementsAre(IsUrlBookmark(kUrl1Title, kUrl1)));
  EXPECT_THAT(model_->account_bookmark_bar_node()->children(), IsEmpty());
}

TEST_F(LocalBookmarkToAccountMergerTest,
       ShouldDoNothingIfAccountNodeIdSelected) {
  const std::u16string kUrl1Title = u"url1";
  const GURL kUrl1("http://www.url1.com/");
  const std::u16string kUrl2Title = u"url2";
  const GURL kUrl2("http://www.url2.com/");

  // -------- Local bookmarks --------
  // bookmark_bar
  //  | - bookmark(kUuid/kLocalTitle)
  AddLocalNodes({UrlBuilder(kUrl1Title, kUrl1)});

  // -------- Account bookmarks --------
  // bookmark_bar
  AddAccountNodes({UrlBuilder(kUrl2Title, kUrl2)});

  // -------- The expected merge outcome --------
  // Unchanged
  EXPECT_CALL(observer_, BookmarkNodeAdded).Times(0);
  EXPECT_CALL(observer_, BookmarkNodeMoved).Times(0);
  EXPECT_CALL(observer_, BookmarkNodeRemoved).Times(0);
  EXPECT_CALL(observer_, BookmarkNodeChanged).Times(0);

  LocalBookmarkToAccountMerger(model_.get())
      .MoveAndMergeSpecificSubtrees(
          {model_->account_bookmark_bar_node()->children()[0]->id()});

  EXPECT_THAT(model_->bookmark_bar_node()->children(),
              ElementsAre(IsUrlBookmark(kUrl1Title, kUrl1)));
  EXPECT_THAT(model_->account_bookmark_bar_node()->children(),
              ElementsAre(IsUrlBookmark(kUrl2Title, kUrl2)));
}

TEST_F(LocalBookmarkToAccountMergerTest,
       ShouldMergeSelectedSubsetOfLocalNodes) {
  const std::u16string kUrl1Title = u"url1";
  const GURL kUrl1("http://www.url1.com/");
  const std::u16string kUrl2Title = u"url2";
  const GURL kUrl2("http://www.url2.com/");
  const std::u16string kUrl3Title = u"url3";
  const GURL kUrl3("http://www.url3.com/");
  const std::u16string kUrl4Title = u"url4";
  const GURL kUrl4("http://www.url4.com/");
  const std::u16string kUrl5Title = u"url5";
  const GURL kUrl5("http://www.url5.com/");

  // -------- Local bookmarks --------
  // bookmark_bar
  //  | - bookmark(kUrl1Title)
  //    | - bookmark(kUrl2Title)
  AddLocalNodes({UrlBuilder(kUrl1Title, kUrl1), UrlBuilder(kUrl2Title, kUrl2),
                 UrlBuilder(kUrl3Title, kUrl3), UrlBuilder(kUrl4Title, kUrl4),
                 UrlBuilder(kUrl5Title, kUrl5)});

  // -------- Account bookmarks --------
  // bookmark_bar
  AddAccountNodes({});

  // -------- The expected merge outcome --------
  // Move(2, 4)
  //
  // ---- Local bookmarks ----
  // bookmark_bar
  //  | - bookmark(kUrl1Title)
  //  | - bookmark(kUrl3Title)
  //  | - bookmark(kUrl5Title)
  //
  // ---- Account bookmarks ----
  // bookmark_bar
  //  | - bookmark(kUrl2Title)
  //  | - bookmark(kUrl4Title)
  EXPECT_CALL(observer_, BookmarkNodeAdded).Times(0);
  EXPECT_CALL(observer_, BookmarkNodeMoved).Times(2);
  EXPECT_CALL(observer_, BookmarkNodeRemoved).Times(0);
  EXPECT_CALL(observer_, BookmarkNodeChanged).Times(0);

  LocalBookmarkToAccountMerger(model_.get())
      .MoveAndMergeSpecificSubtrees(
          {model_->bookmark_bar_node()->children()[1]->id(),
           model_->bookmark_bar_node()->children()[3]->id()});

  EXPECT_THAT(model_->bookmark_bar_node()->children(),
              ElementsAre(IsUrlBookmark(kUrl1Title, kUrl1),
                          IsUrlBookmark(kUrl3Title, kUrl3),
                          IsUrlBookmark(kUrl5Title, kUrl5)));
  EXPECT_THAT(model_->account_bookmark_bar_node()->children(),
              ElementsAre(IsUrlBookmark(kUrl2Title, kUrl2),
                          IsUrlBookmark(kUrl4Title, kUrl4)));
}

TEST_F(LocalBookmarkToAccountMergerTest, ShouldDoNothingIfChildNodeSelected) {
  const std::u16string kFolderTitle = u"Folder Title";
  const std::u16string kUrl1Title = u"url1";
  const GURL kUrl1("http://www.url1.com/");

  // -------- Local bookmarks --------
  // bookmark_bar
  //  | - folder (kFolderTitle)
  //    | - bookmark (kUrl1Title)
  AddLocalNodes({FolderBuilder(kFolderTitle)
                     .SetChildren({UrlBuilder(kUrl1Title, kUrl1)})});

  // -------- Account bookmarks --------
  // bookmark_bar
  AddAccountNodes({});

  // -------- The expected merge outcome --------
  // Move(kUrl1Title)
  //
  // Unchanged (the child is not moved).
  EXPECT_CALL(observer_, BookmarkNodeAdded).Times(0);
  EXPECT_CALL(observer_, BookmarkNodeMoved).Times(0);
  EXPECT_CALL(observer_, BookmarkNodeRemoved).Times(0);
  EXPECT_CALL(observer_, BookmarkNodeChanged).Times(0);

  LocalBookmarkToAccountMerger(model_.get())
      .MoveAndMergeSpecificSubtrees(
          {model_->bookmark_bar_node()->children()[0]->children()[0]->id()});

  EXPECT_THAT(model_->bookmark_bar_node()->children(),
              ElementsAre(IsFolder(kFolderTitle, ElementsAre(IsUrlBookmark(
                                                     kUrl1Title, kUrl1)))));
  EXPECT_THAT(model_->account_bookmark_bar_node()->children(), IsEmpty());
}

TEST_F(LocalBookmarkToAccountMergerTest,
       ShouldApplySemanticDedupeWhenUUIDMatchForNotSelectedNode) {
  const std::u16string kUrl1Title = u"url1";
  const GURL kUrl1("http://www.url1.com/");
  const std::u16string kUrl2Title = u"url2";
  const GURL kUrl2("http://www.url2.com/");
  const std::u16string kUrl3Title = u"url3";
  const GURL kUrl3("http://www.url3.com/");
  const base::Uuid kUuid1 = base::Uuid::GenerateRandomV4();
  const base::Uuid kUuid2 = base::Uuid::GenerateRandomV4();
  const base::Uuid kUuid3 = base::Uuid::GenerateRandomV4();

  // -------- Local bookmarks --------
  // bookmark_bar
  //  | - bookmark(kUuid1, kUrl1Title, kUrl1)
  //  | - bookmark(kUuid2, kUrl2Title, kUrl2)
  AddLocalNodes({UrlBuilder(kUrl1Title, kUrl1).SetUuid(kUuid1),
                 UrlBuilder(kUrl2Title, kUrl2).SetUuid(kUuid2)});

  // -------- Account bookmarks --------
  // bookmark_bar
  //  | - bookmark(kUuid2, kUrl1Title, kUrl1)
  //  | - bookmark(kUuid3, kUrl3Title, kUrl3)
  AddAccountNodes({UrlBuilder(kUrl1Title, kUrl1).SetUuid(kUuid2),
                   UrlBuilder(kUrl3Title, kUrl3).SetUuid(kUuid3)});

  // -------- The expected merge outcome --------
  // Move(kUrl1Title)
  // In this case there is a UUID match between the local node with title 2
  // (which isn't selected), and the account node with title 1. The semantic
  // match is still applied for local and account nodes with title 1.
  //
  // ---- Local bookmarks ----
  // bookmark_bar
  //  | - bookmark(kUuid2, kUrl2Title, kUrl2)
  //
  // ---- Account bookmarks ----
  // bookmark_bar
  //  | - bookmark(kUuid2, kUrl1Title, kUrl1)
  //  | - bookmark(kUuid3, kUrl3Title, kUrl3)
  EXPECT_CALL(observer_, BookmarkNodeAdded).Times(0);
  EXPECT_CALL(observer_, BookmarkNodeMoved).Times(0);
  EXPECT_CALL(observer_, BookmarkNodeRemoved).Times(1);
  EXPECT_CALL(observer_, BookmarkNodeChanged).Times(0);

  LocalBookmarkToAccountMerger(model_.get())
      .MoveAndMergeSpecificSubtrees(
          {model_->bookmark_bar_node()->children()[0]->id()});

  EXPECT_THAT(model_->bookmark_bar_node()->children(),
              ElementsAre(IsUrlBookmark(kUrl2Title, kUrl2)));
  EXPECT_THAT(model_->account_bookmark_bar_node()->children(),
              ElementsAre(IsUrlBookmarkWithUuid(kUrl1Title, kUrl1, kUuid2),
                          IsUrlBookmarkWithUuid(kUrl3Title, kUrl3, kUuid3)));
}

TEST_F(LocalBookmarkToAccountMergerTest, ShouldIgnoreManagedNodesWhenSelected) {
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

  // -------- The expected merge outcome --------
  // Unchanged
  LocalBookmarkToAccountMerger(model.get())
      .MoveAndMergeSpecificSubtrees({managed_node->children()[0]->id()});

  ASSERT_THAT(model->bookmark_bar_node()->children(),
              ElementsAre(IsUrlBookmark(kUrl1Title, kUrl1)));
  ASSERT_THAT(model->account_bookmark_bar_node()->children(), IsEmpty());

  // Managed nodes should be excluded from the merge and be left unmodified.
  ASSERT_THAT(managed_node->children(),
              ElementsAre(IsUrlBookmark(kUrl2Title, kUrl2)));
  EXPECT_THAT(model->GetNodesByURL(kUrl2),
              ElementsAre(managed_node->children()[0].get()));
}

}  // namespace

}  // namespace sync_bookmarks
