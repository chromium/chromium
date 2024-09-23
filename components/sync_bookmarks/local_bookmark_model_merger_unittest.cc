// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_bookmarks/local_bookmark_model_merger.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/strings/utf_ostream_operators.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/uuid.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "components/bookmarks/test/test_bookmark_client.h"
#include "components/sync/base/features.h"
#include "components/sync_bookmarks/bookmark_model_view.h"
#include "components/sync_bookmarks/test_bookmark_model_view.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "url/gurl.h"

namespace sync_bookmarks {
namespace {

using testing::ElementsAre;
using testing::IsEmpty;
using testing::Ne;

MATCHER_P2(MatchesUrl, title, url, "") {
  if (!arg->is_url()) {
    *result_listener << "Expected URL bookmark but got folder.";
    return false;
  }
  if (arg->GetTitle() != base::ASCIIToUTF16(title)) {
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

MATCHER_P2(MatchesFolder, title, children_matcher, "") {
  if (!arg->is_folder()) {
    *result_listener << "Expected folder but got URL.";
    return false;
  }
  if (arg->GetTitle() != base::ASCIIToUTF16(title)) {
    *result_listener << "Expected folder title \"" << title << "\" but got \""
                     << arg->GetTitle() << "\"";
    return false;
  }
  return testing::ExplainMatchResult(children_matcher, arg->children(),
                                     result_listener);
}

MATCHER_P(HasUuid, uuid, "") {
  return testing::ExplainMatchResult(uuid, arg->uuid(), result_listener);
}

MATCHER_P3(MatchesUrlWithUuid, title, url, uuid, "") {
  return testing::ExplainMatchResult(MatchesUrl(title, url), arg,
                                     result_listener) &&
         testing::ExplainMatchResult(HasUuid(uuid), arg, result_listener);
}

MATCHER_P3(MatchesFolderWithUuid, title, uuid, children_matcher, "") {
  return testing::ExplainMatchResult(MatchesFolder(title, children_matcher),
                                     arg, result_listener) &&
         testing::ExplainMatchResult(HasUuid(uuid), arg, result_listener);
}

// Test class to build bookmark URLs conveniently and compactly in tests.
class UrlBuilder {
 public:
  UrlBuilder(const std::string& title, const GURL& url)
      : title_(title), url_(url) {}
  UrlBuilder(const UrlBuilder&) = default;
  ~UrlBuilder() = default;

  UrlBuilder& SetUuid(const base::Uuid& uuid) {
    uuid_ = uuid;
    return *this;
  }

  void Build(BookmarkModelView* model,
             const bookmarks::BookmarkNode* parent) const {
    model->AddURL(parent, parent->children().size(), base::UTF8ToUTF16(title_),
                  url_, /*meta_info=*/nullptr, /*creation_time=*/std::nullopt,
                  uuid_);
  }

 private:
  const std::string title_;
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

  explicit FolderBuilder(const std::string& title) : title_(title) {}
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
        parent, parent->children().size(), base::UTF8ToUTF16(title_),
        /*meta_info=*/nullptr, /*creation_time=*/std::nullopt, uuid_);
    AddChildrenTo(model, folder, children_);
  }

 private:
  const std::string title_;
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
  const std::string kFolder1Title = "folder1";
  const std::string kFolder2Title = "folder2";

  const std::string kUrl1Title = "url1";
  const std::string kUrl2Title = "url2";
  const std::string kUrl3Title = "url3";
  const std::string kUrl4Title = "url4";

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
      ElementsAre(MatchesFolder(kFolder1Title,
                                ElementsAre(MatchesUrl(kUrl1Title, kUrl1),
                                            MatchesUrl(kUrl2Title, kUrl2))),
                  MatchesFolder(kFolder2Title,
                                ElementsAre(MatchesUrl(kUrl3Title, kUrl3),
                                            MatchesUrl(kUrl4Title, kUrl4)))));
}

TEST_F(LocalBookmarkModelMergerTest, ShouldIgnoreManagedNodes) {
  const std::string kUrl1Title = "url1";
  const std::string kUrl2Title = "url2";

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
              ElementsAre(MatchesUrl(kUrl1Title, kUrl1)));

  // Managed nodes should be excluded from the merge.
  EXPECT_THAT(account_model->underlying_model()->GetNodesByURL(kUrl2),
              IsEmpty());
}

TEST_F(LocalBookmarkModelMergerTest, ShouldUploadLocalUuid) {
  const std::string kUrl1Title = "url1";
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
              ElementsAre(MatchesUrlWithUuid(kUrl1Title, kUrl1, kUrl1Uuid)));
}

TEST_F(LocalBookmarkModelMergerTest, ShouldNotUploadDuplicateBySemantics) {
  const std::string kFolder1Title = "folder1";

  const std::string kUrl1Title = "url1";
  const std::string kUrl2Title = "url2";
  const std::string kUrl3Title = "url3";

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
  EXPECT_THAT(account_model->bookmark_bar_node()->children(),
              ElementsAre(MatchesFolder(
                  kFolder1Title, ElementsAre(MatchesUrl(kUrl2Title, kUrl2),
                                             MatchesUrl(kUrl3Title, kUrl3),
                                             MatchesUrl(kUrl1Title, kUrl1)))));
}

TEST_F(LocalBookmarkModelMergerTest, ShouldMergeLocalAndAccountModels) {
  const std::string kFolder1Title = "folder1";
  const std::string kFolder2Title = "folder2";
  const std::string kFolder3Title = "folder3";

  const std::string kUrl1Title = "url1";
  const std::string kUrl2Title = "url2";
  const std::string kUrl3Title = "url3";
  const std::string kUrl4Title = "url4";

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
      ElementsAre(
          MatchesFolder(kFolder1Title,
                        ElementsAre(MatchesUrl(kUrl1Title, kUrl1),
                                    MatchesUrl(kUrl2Title, kAnotherUrl2),
                                    MatchesUrl(kUrl2Title, kUrl2))),
          MatchesFolder(kFolder3Title,
                        ElementsAre(MatchesUrl(kUrl3Title, kUrl3),
                                    MatchesUrl(kUrl4Title, kUrl4))),
          MatchesFolder(kFolder2Title,
                        ElementsAre(MatchesUrl(kUrl3Title, kUrl3),
                                    MatchesUrl(kUrl4Title, kUrl4)))));
}

// This tests that truncated titles produced by legacy clients are properly
// matched.
TEST_F(LocalBookmarkModelMergerTest,
       ShouldMergeLocalAndAccountNodesWhenAccountHasLegacyTruncatedTitle) {
  const std::string kLocalLongTitle(300, 'A');
  const std::string kAccountTruncatedTitle(255, 'A');

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
              ElementsAre(MatchesFolder(kLocalLongTitle, IsEmpty())));
}

// This test checks that local node with truncated title will merge with account
// node which has full title.
TEST_F(LocalBookmarkModelMergerTest,
       ShouldMergeLocalAndAccountNodesWhenLocalHasLegacyTruncatedTitle) {
  const std::string kAccountFullTitle(300, 'A');
  const std::string kLocalTruncatedTitle(255, 'A');

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
              ElementsAre(MatchesFolder(kLocalTruncatedTitle, IsEmpty())));
}

TEST_F(LocalBookmarkModelMergerTest, ShouldMergeBookmarkByUuid) {
  const std::string kLocalTitle = "Title 1";
  const std::string kAccountTitle = "Title 2";
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
              ElementsAre(MatchesUrlWithUuid(kLocalTitle, kUrl, kUuid)));
}

TEST_F(LocalBookmarkModelMergerTest,
       ShouldMergeBookmarkByUuidDespiteDifferentParent) {
  const std::string kFolderTitle = "Folder Title";
  const std::string kLocalTitle = "Title 1";
  const std::string kAccountTitle = "Title 2";
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
      ElementsAre(MatchesFolder(kFolderTitle, ElementsAre(MatchesUrlWithUuid(
                                                  kLocalTitle, kUrl, kUuid)))));
}

TEST_F(LocalBookmarkModelMergerTest,
       ShouldNotMergeBySemanticsIfDifferentParent) {
  const std::string kFolder1Title = "folder1";
  const std::string kFolder2Title = "folder2";

  const std::string kUrl1Title = "url1";
  const std::string kUrl2Title = "url2";

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
      ElementsAre(MatchesFolder(kFolder2Title,
                                ElementsAre(MatchesUrl(kUrl2Title, kUrl2))),
                  MatchesFolder(kFolder1Title,
                                ElementsAre(MatchesFolder(
                                    kFolder2Title, ElementsAre(MatchesUrl(
                                                       kUrl1Title, kUrl1)))))));
}

TEST_F(LocalBookmarkModelMergerTest, ShouldMergeFolderByUuidAndNotSemantics) {
  const std::string kTitle1 = "Title 1";
  const std::string kTitle2 = "Title 2";
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
              ElementsAre(MatchesFolderWithUuid(kTitle2, kUuid2, IsEmpty()),
                          MatchesFolderWithUuid(kTitle1, kUuid1, IsEmpty())));
}

TEST_F(
    LocalBookmarkModelMergerTest,
    ShouldIgnoreFolderSemanticsMatchAndLaterMatchByUuidWithSemanticsNodeFirst) {
  const std::string kLocalOnlyTitle = "LocalOnlyTitle";
  const std::string kMatchingTitle = "MatchingTitle";
  const base::Uuid kUuid1 = base::Uuid::GenerateRandomV4();
  const base::Uuid kUuid2 = base::Uuid::GenerateRandomV4();
  const GURL kUrl("http://foo.com/");
  const std::string kUrlTitle = "Bookmark Title";

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
      ElementsAre(
          MatchesFolderWithUuid(kLocalOnlyTitle, kUuid2,
                                ElementsAre(MatchesUrl(kUrlTitle, kUrl))),
          MatchesFolderWithUuid(kMatchingTitle, kUuid1, IsEmpty())));
}

TEST_F(LocalBookmarkModelMergerTest,
       ShouldIgnoreFolderSemanticsMatchAndLaterMatchByUuidWithUuidNodeFirst) {
  const std::string kLocalOnlyTitle = "LocalOnlyTitle";
  const std::string kMatchingTitle = "MatchingTitle";
  const base::Uuid kUuid1 = base::Uuid::GenerateRandomV4();
  const base::Uuid kUuid2 = base::Uuid::GenerateRandomV4();
  const GURL kUrl("http://foo.com/");
  const std::string kUrlTitle = "Bookmark Title";

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
      ElementsAre(
          MatchesFolderWithUuid(kLocalOnlyTitle, kUuid2,
                                ElementsAre(MatchesUrl(kUrlTitle, kUrl))),
          MatchesFolderWithUuid(kMatchingTitle, kUuid1, IsEmpty())));
}

TEST_F(LocalBookmarkModelMergerTest,
       ShouldReplaceBookmarkUuidWithConflictingURLs) {
  const std::string kTitle = "Title";
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
              ElementsAre(MatchesUrlWithUuid(kTitle, kUrl2, kUuid),
                          MatchesUrlWithUuid(kTitle, kUrl1, Ne(kUuid))));
}

TEST_F(LocalBookmarkModelMergerTest,
       ShouldReplaceBookmarkUuidWithConflictingTypes) {
  const GURL kUrl1("http://www.foo.com/");
  const std::string kTitle = "Title";
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
              ElementsAre(MatchesFolderWithUuid(kTitle, kUuid, IsEmpty()),
                          MatchesUrlWithUuid(kTitle, kUrl1, Ne(kUuid))));
}

TEST_F(LocalBookmarkModelMergerTest,
       ShouldReplaceBookmarkUuidWithConflictingTypesAndLocalChildren) {
  const std::string kFolderTitle = "Folder Title";
  const std::string kUrl1Title = "url1";
  const std::string kUrl2Title = "url2";
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
              ElementsAre(MatchesUrlWithUuid(kUrl2Title, kUrl2, kUuid),
                          MatchesFolderWithUuid(
                              kFolderTitle, Ne(kUuid),
                              ElementsAre(MatchesUrl(kUrl1Title, kUrl1)))));
}

}  // namespace

}  // namespace sync_bookmarks
