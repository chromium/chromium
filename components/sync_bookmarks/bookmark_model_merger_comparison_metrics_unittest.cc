// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_bookmarks/bookmark_model_merger_comparison_metrics.h"

#include <memory>
#include <ostream>
#include <string>
#include <utility>
#include <vector>

#include "base/strings/utf_string_conversions.h"
#include "base/uuid.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "components/bookmarks/browser/bookmark_test_util.h"
#include "components/bookmarks/browser/bookmark_uuids.h"
#include "components/bookmarks/test/test_bookmark_client.h"
#include "components/sync_bookmarks/bookmark_model_merger.h"
#include "components/sync_bookmarks/bookmark_model_view.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace sync_bookmarks {
namespace metrics {

void PrintTo(const UrlAndTitle& value, std::ostream* os) {
  *os << "{\"" << value.url << "\", \"" << base::UTF16ToUTF8(value.title)
      << "\"}";
}

void PrintTo(const UrlAndUuid& value, std::ostream* os) {
  *os << "{\"" << value.url << "\", \"" << value.uuid << "\"}";
}

namespace {

using RemoteTreeNode = BookmarkModelMerger::RemoteTreeNode;

using testing::UnorderedElementsAre;

// Constants forked from bookmark_model_merger.cc.
const char kBookmarkBarTag[] = "bookmark_bar";
const char kMobileBookmarksTag[] = "synced_bookmarks";
const char kOtherBookmarksTag[] = "other_bookmarks";

// Test class to build bookmark URLs conveniently and compactly in tests.
class UrlBookmarkBuilder {
 public:
  UrlBookmarkBuilder(const std::u16string& title, const GURL& url)
      : title_(title), url_(url), uuid_(base::Uuid::GenerateRandomV4()) {}
  UrlBookmarkBuilder(const UrlBookmarkBuilder&) = default;
  ~UrlBookmarkBuilder() = default;

  UrlBookmarkBuilder& SetUuid(const base::Uuid& uuid) {
    uuid_ = uuid;
    return *this;
  }

  void BuildLocal(bookmarks::BookmarkModel* model,
                  const bookmarks::BookmarkNode* parent) const {
    model->AddURL(parent, parent->children().size(), title_, url_,
                  /*meta_info=*/nullptr, /*creation_time=*/std::nullopt, uuid_);
  }

  RemoteTreeNode BuildRemoteNode(const base::Uuid& parent_uuid) const {
    syncer::UpdateResponseData data;
    sync_pb::BookmarkSpecifics* bookmark_specifics =
        data.entity.specifics.mutable_bookmark();
    bookmark_specifics->set_full_title(base::UTF16ToUTF8(title_));
    bookmark_specifics->set_type(sync_pb::BookmarkSpecifics::URL);
    bookmark_specifics->set_url(url_.spec());
    bookmark_specifics->set_guid(uuid_.AsLowercaseString());
    return RemoteTreeNode::BuildForTesting(std::move(data), {});
  }

 private:
  const std::u16string title_;
  const GURL url_;
  base::Uuid uuid_;
};

// Test class to build bookmark folders conveniently and compactly in tests.
class FolderBuilder {
 public:
  using FolderOrUrl = std::variant<FolderBuilder, UrlBookmarkBuilder>;

  static void AddLocalChildTo(bookmarks::BookmarkModel* model,
                              const bookmarks::BookmarkNode* parent,
                              const FolderOrUrl& folder_or_url) {
    if (std::holds_alternative<UrlBookmarkBuilder>(folder_or_url)) {
      std::get<UrlBookmarkBuilder>(folder_or_url).BuildLocal(model, parent);
    } else {
      CHECK(std::holds_alternative<FolderBuilder>(folder_or_url));
      std::get<FolderBuilder>(folder_or_url).BuildLocal(model, parent);
    }
  }

  static void AddLocalChildrenTo(bookmarks::BookmarkModel* model,
                                 const bookmarks::BookmarkNode* parent,
                                 const std::vector<FolderOrUrl>& children) {
    for (const FolderOrUrl& folder_or_url : children) {
      AddLocalChildTo(model, parent, folder_or_url);
    }
  }

  static RemoteTreeNode BuildRemoteNode(const FolderOrUrl& folder_or_url,
                                        const base::Uuid& parent_uuid) {
    if (std::holds_alternative<UrlBookmarkBuilder>(folder_or_url)) {
      return std::get<UrlBookmarkBuilder>(folder_or_url)
          .BuildRemoteNode(parent_uuid);
    } else {
      CHECK(std::holds_alternative<FolderBuilder>(folder_or_url));
      return std::get<FolderBuilder>(folder_or_url)
          .BuildRemoteNode(parent_uuid);
    }
  }

  static std::vector<RemoteTreeNode> BuildRemoteNodes(
      const std::vector<FolderOrUrl>& children,
      const base::Uuid& parent_uuid) {
    std::vector<RemoteTreeNode> nodes;
    for (const FolderOrUrl& folder_or_url : children) {
      nodes.push_back(BuildRemoteNode(folder_or_url, parent_uuid));
    }
    return nodes;
  }

  explicit FolderBuilder(const std::u16string& title)
      : title_(title), uuid_(base::Uuid::GenerateRandomV4()) {}
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

  void BuildLocal(bookmarks::BookmarkModel* model,
                  const bookmarks::BookmarkNode* parent) const {
    const bookmarks::BookmarkNode* folder = model->AddFolder(
        parent, parent->children().size(), title_,
        /*meta_info=*/nullptr, /*creation_time=*/std::nullopt, uuid_);
    AddLocalChildrenTo(model, folder, children_);
  }

  RemoteTreeNode BuildRemoteNode(const base::Uuid& parent_uuid) const {
    syncer::UpdateResponseData data;
    sync_pb::BookmarkSpecifics* bookmark_specifics =
        data.entity.specifics.mutable_bookmark();
    bookmark_specifics->set_full_title(base::UTF16ToUTF8(title_));
    bookmark_specifics->set_type(sync_pb::BookmarkSpecifics::FOLDER);
    bookmark_specifics->set_guid(uuid_.AsLowercaseString());
    return RemoteTreeNode::BuildForTesting(
        std::move(data), BuildRemoteNodes(children_,
                                          /*parent_uuid=*/uuid_));
  }

 private:
  const std::u16string title_;
  std::vector<FolderOrUrl> children_;
  base::Uuid uuid_;
};

class BookmarkModelMergerComparisonMetricsTest : public testing::Test {
 protected:
  BookmarkModelMergerComparisonMetricsTest() = default;

  ~BookmarkModelMergerComparisonMetricsTest() override = default;

  void AddLocalNodes(
      const std::vector<FolderBuilder::FolderOrUrl>& children_of_bookmark_bar,
      const std::vector<FolderBuilder::FolderOrUrl>& children_of_mobile_node,
      const std::vector<FolderBuilder::FolderOrUrl>& children_of_other_node) {
    FolderBuilder::AddLocalChildrenTo(model_.get(), model_->bookmark_bar_node(),
                                      children_of_bookmark_bar);
    FolderBuilder::AddLocalChildrenTo(model_.get(), model_->mobile_node(),
                                      children_of_mobile_node);
    FolderBuilder::AddLocalChildrenTo(model_.get(), model_->other_node(),
                                      children_of_other_node);
  }

  static RemoteTreeNode BuildRemoteNodeForPermanentFolder(
      std::string_view server_defined_unique_tag,
      std::string_view uuid,
      const std::vector<FolderBuilder::FolderOrUrl>& children) {
    // Use semi-empty UpdateResponseData() as these tests don't rely on
    // realistic updates for permanent folders, beyond having the tag.
    syncer::UpdateResponseData data;
    data.entity.server_defined_unique_tag = server_defined_unique_tag;
    return RemoteTreeNode::BuildForTesting(
        std::move(data), FolderBuilder::BuildRemoteNodes(
                             children,
                             /*parent_uuid=*/base::Uuid::ParseLowercase(uuid)));
  }

  static BookmarkModelMerger::RemoteForest BuildAccountNodes(
      const std::vector<FolderBuilder::FolderOrUrl>& children_of_bookmark_bar,
      const std::vector<FolderBuilder::FolderOrUrl>& children_of_mobile_node,
      const std::vector<FolderBuilder::FolderOrUrl>& children_of_other_node) {
    BookmarkModelMerger::RemoteForest forest;
    // String literals used below are part of the sync protocol and listed in
    // bookmark_model_merger.cc.
    forest.emplace(kBookmarkBarTag,
                   BuildRemoteNodeForPermanentFolder(
                       kBookmarkBarTag, bookmarks::kBookmarkBarNodeUuid,
                       children_of_bookmark_bar));
    forest.emplace(kMobileBookmarksTag,
                   BuildRemoteNodeForPermanentFolder(
                       kMobileBookmarksTag, bookmarks::kMobileBookmarksNodeUuid,
                       children_of_mobile_node));
    forest.emplace(kOtherBookmarksTag,
                   BuildRemoteNodeForPermanentFolder(
                       kOtherBookmarksTag, bookmarks::kOtherBookmarksNodeUuid,
                       children_of_other_node));
    return forest;
  }

  const std::unique_ptr<bookmarks::BookmarkModel> model_ =
      bookmarks::TestBookmarkClient::CreateModel();
};

TEST_F(BookmarkModelMergerComparisonMetricsTest, ShouldExtractLocalNodes) {
  const std::u16string kFolder1Title = u"folder1";

  const std::u16string kUrl1Title = u"url1";
  const std::u16string kUrl2Title = u"url2";
  const std::u16string kUrl3Title = u"url3";
  const std::u16string kUrl4Title = u"url4";

  const GURL kUrl1("http://www.url1.com/");
  const GURL kUrl2("http://www.url2.com/");
  const GURL kUrl3("http://www.url3.com/");
  const GURL kUrl4("http://www.url4.com/");

  const base::Uuid kUrl1Uuid = base::Uuid::GenerateRandomV4();
  const base::Uuid kUrl2Uuid = base::Uuid::GenerateRandomV4();
  const base::Uuid kUrl3Uuid = base::Uuid::GenerateRandomV4();
  const base::Uuid kUrl4Uuid = base::Uuid::GenerateRandomV4();

  // -------- Local bookmarks --------
  // bookmark_bar
  //  |- url1(http://www.url1.com)
  //  |- folder 1
  //    |- url2(http://www.url2.com)
  // mobile_node
  //  |- url3(http://www.url3.com)
  // other_node
  //  |- url4(http://www.url4.com)
  AddLocalNodes(
      /*children_of_bookmark_bar=*/{UrlBookmarkBuilder(kUrl1Title, kUrl1)
                                        .SetUuid(kUrl1Uuid),
                                    FolderBuilder(kFolder1Title)
                                        .SetChildren(
                                            {UrlBookmarkBuilder(kUrl2Title,
                                                                kUrl2)
                                                 .SetUuid(kUrl2Uuid)})},
      /*children_of_mobile_node=*/
      {UrlBookmarkBuilder(kUrl3Title, kUrl3).SetUuid(kUrl3Uuid)},
      /*children_of_other_node=*/
      {UrlBookmarkBuilder(kUrl4Title, kUrl4).SetUuid(kUrl4Uuid)});

  // By URL and title.
  EXPECT_THAT(ExtractUniqueLocalNodesByUrlAndTitleForTesting(
                  BookmarkModelViewUsingLocalOrSyncableNodes(model_.get()),
                  SubtreeSelection::kConsideringAllBookmarks),
              UnorderedElementsAre(UrlAndTitle{kUrl1, kUrl1Title},
                                   UrlAndTitle{kUrl2, kUrl2Title},
                                   UrlAndTitle{kUrl3, kUrl3Title},
                                   UrlAndTitle{kUrl4, kUrl4Title}));
  EXPECT_THAT(ExtractUniqueLocalNodesByUrlAndTitleForTesting(
                  BookmarkModelViewUsingLocalOrSyncableNodes(model_.get()),
                  SubtreeSelection::kUnderBookmarkBar),
              UnorderedElementsAre(UrlAndTitle{kUrl1, kUrl1Title},
                                   UrlAndTitle{kUrl2, kUrl2Title}));

  // By URL and UUID.
  EXPECT_THAT(ExtractUniqueLocalNodesByUrlAndUuidForTesting(
                  BookmarkModelViewUsingLocalOrSyncableNodes(model_.get()),
                  SubtreeSelection::kConsideringAllBookmarks),
              UnorderedElementsAre(
                  UrlAndUuid{kUrl1, kUrl1Uuid}, UrlAndUuid{kUrl2, kUrl2Uuid},
                  UrlAndUuid{kUrl3, kUrl3Uuid}, UrlAndUuid{kUrl4, kUrl4Uuid}));
  EXPECT_THAT(ExtractUniqueLocalNodesByUrlAndUuidForTesting(
                  BookmarkModelViewUsingLocalOrSyncableNodes(model_.get()),
                  SubtreeSelection::kUnderBookmarkBar),
              UnorderedElementsAre(UrlAndUuid{kUrl1, kUrl1Uuid},
                                   UrlAndUuid{kUrl2, kUrl2Uuid}));
}

TEST_F(BookmarkModelMergerComparisonMetricsTest, ShouldExtractAccountNodes) {
  const std::u16string kFolder1Title = u"folder1";

  const std::u16string kUrl1Title = u"url1";
  const std::u16string kUrl2Title = u"url2";
  const std::u16string kUrl3Title = u"url3";
  const std::u16string kUrl4Title = u"url4";

  const GURL kUrl1("http://www.url1.com/");
  const GURL kUrl2("http://www.url2.com/");
  const GURL kUrl3("http://www.url3.com/");
  const GURL kUrl4("http://www.url4.com/");

  const base::Uuid kUrl1Uuid = base::Uuid::GenerateRandomV4();
  const base::Uuid kUrl2Uuid = base::Uuid::GenerateRandomV4();
  const base::Uuid kUrl3Uuid = base::Uuid::GenerateRandomV4();
  const base::Uuid kUrl4Uuid = base::Uuid::GenerateRandomV4();

  // -------- Account bookmarks --------
  // bookmark_bar
  //  |- url1(http://www.url1.com)
  //  |- folder 1
  //    |- url2(http://www.url2.com)
  // mobile_node
  //  |- url3(http://www.url3.com)
  // other_node
  //  |- url4(http://www.url4.com)
  BookmarkModelMerger::RemoteForest account_data = BuildAccountNodes(
      /*children_of_bookmark_bar=*/{UrlBookmarkBuilder(kUrl1Title, kUrl1)
                                        .SetUuid(kUrl1Uuid),
                                    FolderBuilder(kFolder1Title)
                                        .SetChildren(
                                            {UrlBookmarkBuilder(kUrl2Title,
                                                                kUrl2)
                                                 .SetUuid(kUrl2Uuid)})},
      /*children_of_mobile_node=*/
      {UrlBookmarkBuilder(kUrl3Title, kUrl3).SetUuid(kUrl3Uuid)},
      /*children_of_other_node=*/
      {UrlBookmarkBuilder(kUrl4Title, kUrl4).SetUuid(kUrl4Uuid)});

  // By URL and title.
  EXPECT_THAT(ExtractUniqueAccountNodesByUrlAndTitleForTesting(
                  account_data, SubtreeSelection::kConsideringAllBookmarks),
              UnorderedElementsAre(UrlAndTitle{kUrl1, kUrl1Title},
                                   UrlAndTitle{kUrl2, kUrl2Title},
                                   UrlAndTitle{kUrl3, kUrl3Title},
                                   UrlAndTitle{kUrl4, kUrl4Title}));
  EXPECT_THAT(ExtractUniqueAccountNodesByUrlAndTitleForTesting(
                  account_data, SubtreeSelection::kUnderBookmarkBar),
              UnorderedElementsAre(UrlAndTitle{kUrl1, kUrl1Title},
                                   UrlAndTitle{kUrl2, kUrl2Title}));

  // By URL and UUID.
  EXPECT_THAT(ExtractUniqueAccountNodesByUrlAndUuidForTesting(
                  account_data, SubtreeSelection::kConsideringAllBookmarks),
              UnorderedElementsAre(
                  UrlAndUuid{kUrl1, kUrl1Uuid}, UrlAndUuid{kUrl2, kUrl2Uuid},
                  UrlAndUuid{kUrl3, kUrl3Uuid}, UrlAndUuid{kUrl4, kUrl4Uuid}));
  EXPECT_THAT(ExtractUniqueAccountNodesByUrlAndUuidForTesting(
                  account_data, SubtreeSelection::kUnderBookmarkBar),
              UnorderedElementsAre(UrlAndUuid{kUrl1, kUrl1Uuid},
                                   UrlAndUuid{kUrl2, kUrl2Uuid}));
}

}  // namespace
}  // namespace metrics
}  // namespace sync_bookmarks
