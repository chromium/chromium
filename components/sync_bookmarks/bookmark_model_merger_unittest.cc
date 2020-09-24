// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_bookmarks/bookmark_model_merger.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/guid.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/test/test_bookmark_client.h"
#include "components/favicon/core/test/mock_favicon_service.h"
#include "components/sync/base/unique_position.h"
#include "components/sync_bookmarks/bookmark_specifics_conversions.h"
#include "components/sync_bookmarks/switches.h"
#include "components/sync_bookmarks/synced_bookmark_tracker.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::Eq;
using testing::IsNull;
using testing::Ne;
using testing::NotNull;
using testing::UnorderedElementsAre;

namespace sync_bookmarks {

namespace {

// Copy of BookmarksGUIDDuplicates.
enum class ExpectedBookmarksGUIDDuplicates {
  kMatchingUrls = 0,
  kMatchingFolders = 1,
  kDifferentUrls = 2,
  kDifferentFolders = 3,
  kDifferentTypes = 4,
};

const char kBookmarkBarId[] = "bookmark_bar_id";
const char kBookmarkBarTag[] = "bookmark_bar";

// Fork of enum RemoteBookmarkUpdateError.
enum class ExpectedRemoteBookmarkUpdateError {
  kInvalidSpecifics = 1,
  kInvalidUniquePosition = 2,
  kMissingParentEntity = 4,
  kUnexpectedGuid = 9,
  kParentNotFolder = 10,
  kMaxValue = kParentNotFolder,
};

// |*arg| must be of type std::vector<std::unique_ptr<bookmarks::BookmarkNode>>.
MATCHER_P(ElementRawPointersAre, expected_raw_ptr, "") {
  if (arg.size() != 1) {
    return false;
  }
  return arg[0].get() == expected_raw_ptr;
}

// |*arg| must be of type std::vector<std::unique_ptr<bookmarks::BookmarkNode>>.
MATCHER_P2(ElementRawPointersAre, expected_raw_ptr0, expected_raw_ptr1, "") {
  if (arg.size() != 2) {
    return false;
  }
  return arg[0].get() == expected_raw_ptr0 && arg[1].get() == expected_raw_ptr1;
}

class UpdateResponseDataBuilder {
 public:
  UpdateResponseDataBuilder(const std::string& server_id,
                            const std::string& parent_id,
                            const std::string& title,
                            const syncer::UniquePosition& unique_position) {
    data_.id = server_id;
    data_.parent_id = parent_id;
    data_.unique_position = unique_position.ToProto();
    data_.is_folder = true;

    sync_pb::BookmarkSpecifics* bookmark_specifics =
        data_.specifics.mutable_bookmark();
    bookmark_specifics->set_legacy_canonicalized_title(title);
    bookmark_specifics->set_full_title(title);

    SetGuid(base::GenerateGUID());
  }

  UpdateResponseDataBuilder& SetUrl(const GURL& url) {
    data_.is_folder = false;
    data_.specifics.mutable_bookmark()->set_url(url.spec());
    return *this;
  }

  UpdateResponseDataBuilder& SetLegacyTitleOnly() {
    data_.specifics.mutable_bookmark()->clear_full_title();
    return *this;
  }

  UpdateResponseDataBuilder& SetFavicon(const GURL& favicon_url,
                                        const std::string& favicon_data) {
    data_.specifics.mutable_bookmark()->set_icon_url(favicon_url.spec());
    data_.specifics.mutable_bookmark()->set_favicon(favicon_data);
    return *this;
  }

  UpdateResponseDataBuilder& SetGuid(const std::string& guid) {
    data_.originator_client_item_id = guid;
    data_.specifics.mutable_bookmark()->set_guid(guid);
    return *this;
  }

  syncer::UpdateResponseData Build() {
    syncer::UpdateResponseData response_data;
    response_data.entity = std::move(data_);
    // Similar to what's done in the loopback_server.
    response_data.response_version = 0;
    return response_data;
  }

 private:
  syncer::EntityData data_;
};

syncer::UpdateResponseData CreateUpdateResponseData(
    const std::string& server_id,
    const std::string& parent_id,
    const std::string& title,
    const std::string& url,
    bool is_folder,
    const syncer::UniquePosition& unique_position,
    base::Optional<std::string> guid = base::nullopt,
    const std::string& icon_url = std::string(),
    const std::string& icon_data = std::string()) {
  UpdateResponseDataBuilder builder(server_id, parent_id, title,
                                    unique_position);
  if (guid) {
    builder.SetGuid(*guid);
  }
  if (!is_folder) {
    builder.SetUrl(GURL(url));
  }
  builder.SetFavicon(GURL(icon_url), icon_data);

  return builder.Build();
}

syncer::UpdateResponseData CreateBookmarkBarNodeUpdateData() {
  syncer::EntityData data;
  data.id = kBookmarkBarId;
  data.server_defined_unique_tag = kBookmarkBarTag;

  data.specifics.mutable_bookmark();

  syncer::UpdateResponseData response_data;
  response_data.entity = std::move(data);
  // Similar to what's done in the loopback_server.
  response_data.response_version = 0;
  return response_data;
}

syncer::UniquePosition PositionOf(const bookmarks::BookmarkNode* node,
                                  const SyncedBookmarkTracker& tracker) {
  const SyncedBookmarkTracker::Entity* entity =
      tracker.GetEntityForBookmarkNode(node);
  return syncer::UniquePosition::FromProto(
      entity->metadata()->unique_position());
}

bool PositionsInTrackerMatchModel(const bookmarks::BookmarkNode* node,
                                  const SyncedBookmarkTracker& tracker) {
  if (node->children().empty()) {
    return true;
  }
  syncer::UniquePosition last_pos =
      PositionOf(node->children().front().get(), tracker);
  for (size_t i = 1; i < node->children().size(); ++i) {
    syncer::UniquePosition pos = PositionOf(node->children()[i].get(), tracker);
    if (pos.LessThan(last_pos)) {
      DLOG(ERROR) << "Position of " << node->children()[i]->GetTitle()
                  << " is less than position of "
                  << node->children()[i - 1]->GetTitle();
      return false;
    }
    last_pos = pos;
  }
  return std::all_of(node->children().cbegin(), node->children().cend(),
                     [&tracker](const auto& child) {
                       return PositionsInTrackerMatchModel(child.get(),
                                                           tracker);
                     });
}

std::unique_ptr<SyncedBookmarkTracker> Merge(
    syncer::UpdateResponseDataList updates,
    bookmarks::BookmarkModel* bookmark_model) {
  std::unique_ptr<SyncedBookmarkTracker> tracker =
      SyncedBookmarkTracker::CreateEmpty(sync_pb::ModelTypeState());
  testing::NiceMock<favicon::MockFaviconService> favicon_service;
  BookmarkModelMerger(std::move(updates), bookmark_model, &favicon_service,
                      tracker.get())
      .Merge();
  return tracker;
}

static syncer::UniquePosition MakeRandomPosition() {
  const std::string suffix = syncer::UniquePosition::RandomSuffix();
  return syncer::UniquePosition::InitialPosition(suffix);
}

}  // namespace

// TODO(crbug.com/978430): Parameterize unit tests to account for both
// GUID-based and original merge algorithms.

TEST(BookmarkModelMergerTest, ShouldMergeLocalAndRemoteModels) {
  const size_t kMaxEntries = 1000;

  const std::string kFolder1Title = "folder1";
  const std::string kFolder2Title = "folder2";
  const std::string kFolder3Title = "folder3";

  const std::string kUrl1Title = "url1";
  const std::string kUrl2Title = "url2";
  const std::string kUrl3Title = "url3";
  const std::string kUrl4Title = "url4";

  const std::string kUrl1 = "http://www.url1.com";
  const std::string kUrl2 = "http://www.url2.com";
  const std::string kUrl3 = "http://www.url3.com";
  const std::string kUrl4 = "http://www.url4.com";
  const std::string kAnotherUrl2 = "http://www.another-url2.com";

  const std::string kFolder1Id = "Folder1Id";
  const std::string kFolder3Id = "Folder3Id";
  const std::string kUrl1Id = "Url1Id";
  const std::string kUrl2Id = "Url2Id";
  const std::string kUrl3Id = "Url3Id";
  const std::string kUrl4Id = "Url4Id";

  // -------- The local model --------
  // bookmark_bar
  //  |- folder 1
  //    |- url1(http://www.url1.com)
  //    |- url2(http://www.url2.com)
  //  |- folder 2
  //    |- url3(http://www.url3.com)
  //    |- url4(http://www.url4.com)

  std::unique_ptr<bookmarks::BookmarkModel> bookmark_model =
      bookmarks::TestBookmarkClient::CreateModel();

  const bookmarks::BookmarkNode* bookmark_bar_node =
      bookmark_model->bookmark_bar_node();
  const bookmarks::BookmarkNode* folder1 = bookmark_model->AddFolder(
      /*parent=*/bookmark_bar_node, /*index=*/0,
      base::UTF8ToUTF16(kFolder1Title));

  const bookmarks::BookmarkNode* folder2 = bookmark_model->AddFolder(
      /*parent=*/bookmark_bar_node, /*index=*/1,
      base::UTF8ToUTF16(kFolder2Title));

  bookmark_model->AddURL(
      /*parent=*/folder1, /*index=*/0, base::UTF8ToUTF16(kUrl1Title),
      GURL(kUrl1));
  bookmark_model->AddURL(
      /*parent=*/folder1, /*index=*/1, base::UTF8ToUTF16(kUrl2Title),
      GURL(kUrl2));
  bookmark_model->AddURL(
      /*parent=*/folder2, /*index=*/0, base::UTF8ToUTF16(kUrl3Title),
      GURL(kUrl3));
  bookmark_model->AddURL(
      /*parent=*/folder2, /*index=*/1, base::UTF8ToUTF16(kUrl4Title),
      GURL(kUrl4));

  // -------- The remote model --------
  // bookmark_bar
  //  |- folder 1
  //    |- url1(http://www.url1.com)
  //    |- url2(http://www.another-url2.com)
  //  |- folder 3
  //    |- url3(http://www.url3.com)
  //    |- url4(http://www.url4.com)

  const std::string suffix = syncer::UniquePosition::RandomSuffix();
  syncer::UniquePosition posFolder1 =
      syncer::UniquePosition::InitialPosition(suffix);
  syncer::UniquePosition posFolder3 =
      syncer::UniquePosition::After(posFolder1, suffix);

  syncer::UniquePosition posUrl1 =
      syncer::UniquePosition::InitialPosition(suffix);
  syncer::UniquePosition posUrl2 =
      syncer::UniquePosition::After(posUrl1, suffix);

  syncer::UniquePosition posUrl3 =
      syncer::UniquePosition::InitialPosition(suffix);
  syncer::UniquePosition posUrl4 =
      syncer::UniquePosition::After(posUrl3, suffix);

  syncer::UpdateResponseDataList updates;
  updates.push_back(CreateBookmarkBarNodeUpdateData());
  updates.push_back(CreateUpdateResponseData(
      /*server_id=*/kFolder1Id, /*parent_id=*/kBookmarkBarId, kFolder1Title,
      /*url=*/std::string(),
      /*is_folder=*/true, /*unique_position=*/posFolder1));
  updates.push_back(CreateUpdateResponseData(
      /*server_id=*/kUrl1Id, /*parent_id=*/kFolder1Id, kUrl1Title, kUrl1,
      /*is_folder=*/false, /*unique_position=*/posUrl1));
  updates.push_back(CreateUpdateResponseData(
      /*server_id=*/kUrl2Id, /*parent_id=*/kFolder1Id, kUrl2Title, kAnotherUrl2,
      /*is_folder=*/false, /*unique_position=*/posUrl2));
  updates.push_back(CreateUpdateResponseData(
      /*server_id=*/kFolder3Id, /*parent_id=*/kBookmarkBarId, kFolder3Title,
      /*url=*/std::string(),
      /*is_folder=*/true, /*unique_position=*/posFolder3));
  updates.push_back(CreateUpdateResponseData(
      /*server_id=*/kUrl3Id, /*parent_id=*/kFolder3Id, kUrl3Title, kUrl3,
      /*is_folder=*/false, /*unique_position=*/posUrl3));
  updates.push_back(CreateUpdateResponseData(
      /*server_id=*/kUrl4Id, /*parent_id=*/kFolder3Id, kUrl4Title, kUrl4,
      /*is_folder=*/false, /*unique_position=*/posUrl4));

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

  std::unique_ptr<SyncedBookmarkTracker> tracker =
      Merge(std::move(updates), bookmark_model.get());
  ASSERT_THAT(bookmark_bar_node->children().size(), Eq(3u));

  // Verify Folder 1.
  EXPECT_THAT(bookmark_bar_node->children()[0]->GetTitle(),
              Eq(base::ASCIIToUTF16(kFolder1Title)));
  ASSERT_THAT(bookmark_bar_node->children()[0]->children().size(), Eq(3u));

  EXPECT_THAT(bookmark_bar_node->children()[0]->children()[0]->GetTitle(),
              Eq(base::ASCIIToUTF16(kUrl1Title)));
  EXPECT_THAT(bookmark_bar_node->children()[0]->children()[0]->url(),
              Eq(GURL(kUrl1)));

  EXPECT_THAT(bookmark_bar_node->children()[0]->children()[1]->GetTitle(),
              Eq(base::ASCIIToUTF16(kUrl2Title)));
  EXPECT_THAT(bookmark_bar_node->children()[0]->children()[1]->url(),
              Eq(GURL(kAnotherUrl2)));

  EXPECT_THAT(bookmark_bar_node->children()[0]->children()[2]->GetTitle(),
              Eq(base::ASCIIToUTF16(kUrl2Title)));
  EXPECT_THAT(bookmark_bar_node->children()[0]->children()[2]->url(),
              Eq(GURL(kUrl2)));

  // Verify Folder 3.
  EXPECT_THAT(bookmark_bar_node->children()[1]->GetTitle(),
              Eq(base::ASCIIToUTF16(kFolder3Title)));
  ASSERT_THAT(bookmark_bar_node->children()[1]->children().size(), Eq(2u));

  EXPECT_THAT(bookmark_bar_node->children()[1]->children()[0]->GetTitle(),
              Eq(base::ASCIIToUTF16(kUrl3Title)));
  EXPECT_THAT(bookmark_bar_node->children()[1]->children()[0]->url(),
              Eq(GURL(kUrl3)));
  EXPECT_THAT(bookmark_bar_node->children()[1]->children()[1]->GetTitle(),
              Eq(base::ASCIIToUTF16(kUrl4Title)));
  EXPECT_THAT(bookmark_bar_node->children()[1]->children()[1]->url(),
              Eq(GURL(kUrl4)));

  // Verify Folder 2.
  EXPECT_THAT(bookmark_bar_node->children()[2]->GetTitle(),
              Eq(base::ASCIIToUTF16(kFolder2Title)));
  ASSERT_THAT(bookmark_bar_node->children()[2]->children().size(), Eq(2u));

  EXPECT_THAT(bookmark_bar_node->children()[2]->children()[0]->GetTitle(),
              Eq(base::ASCIIToUTF16(kUrl3Title)));
  EXPECT_THAT(bookmark_bar_node->children()[2]->children()[0]->url(),
              Eq(GURL(kUrl3)));
  EXPECT_THAT(bookmark_bar_node->children()[2]->children()[1]->GetTitle(),
              Eq(base::ASCIIToUTF16(kUrl4Title)));
  EXPECT_THAT(bookmark_bar_node->children()[2]->children()[1]->url(),
              Eq(GURL(kUrl4)));

  // Verify the tracker contents.
  EXPECT_THAT(tracker->TrackedEntitiesCountForTest(), Eq(11U));
  std::vector<const SyncedBookmarkTracker::Entity*> local_changes =
      tracker->GetEntitiesWithLocalChanges(kMaxEntries);

  EXPECT_THAT(local_changes.size(), Eq(4U));
  std::vector<const bookmarks::BookmarkNode*> nodes_with_local_changes;
  for (const SyncedBookmarkTracker::Entity* local_change : local_changes) {
    nodes_with_local_changes.push_back(local_change->bookmark_node());
  }
  // Verify that url2(http://www.url2.com), Folder 2 and children have
  // corresponding update.
  EXPECT_THAT(nodes_with_local_changes,
              UnorderedElementsAre(
                  bookmark_bar_node->children()[0]->children()[2].get(),
                  bookmark_bar_node->children()[2].get(),
                  bookmark_bar_node->children()[2]->children()[0].get(),
                  bookmark_bar_node->children()[2]->children()[1].get()));

  // Verify positions in tracker.
  EXPECT_TRUE(PositionsInTrackerMatchModel(bookmark_bar_node, *tracker));
}

TEST(BookmarkModelMergerTest, ShouldMergeRemoteReorderToLocalModel) {
  const size_t kMaxEntries = 1000;

  const std::string kFolder1Title = "folder1";
  const std::string kFolder2Title = "folder2";
  const std::string kFolder3Title = "folder3";

  const std::string kFolder1Id = "Folder1Id";
  const std::string kFolder2Id = "Folder2Id";
  const std::string kFolder3Id = "Folder3Id";

  // -------- The local model --------
  // bookmark_bar
  //  |- folder 1
  //  |- folder 2
  //  |- folder 3

  std::unique_ptr<bookmarks::BookmarkModel> bookmark_model =
      bookmarks::TestBookmarkClient::CreateModel();

  const bookmarks::BookmarkNode* bookmark_bar_node =
      bookmark_model->bookmark_bar_node();
  bookmark_model->AddFolder(
      /*parent=*/bookmark_bar_node, /*index=*/0,
      base::UTF8ToUTF16(kFolder1Title));

  bookmark_model->AddFolder(
      /*parent=*/bookmark_bar_node, /*index=*/1,
      base::UTF8ToUTF16(kFolder2Title));

  bookmark_model->AddFolder(
      /*parent=*/bookmark_bar_node, /*index=*/2,
      base::UTF8ToUTF16(kFolder3Title));

  // -------- The remote model --------
  // bookmark_bar
  //  |- folder 1
  //  |- folder 3
  //  |- folder 2

  const std::string suffix = syncer::UniquePosition::RandomSuffix();
  syncer::UniquePosition posFolder1 =
      syncer::UniquePosition::InitialPosition(suffix);
  syncer::UniquePosition posFolder3 =
      syncer::UniquePosition::After(posFolder1, suffix);
  syncer::UniquePosition posFolder2 =
      syncer::UniquePosition::After(posFolder3, suffix);

  syncer::UpdateResponseDataList updates;
  updates.push_back(CreateBookmarkBarNodeUpdateData());
  updates.push_back(CreateUpdateResponseData(
      /*server_id=*/kFolder1Id, /*parent_id=*/kBookmarkBarId, kFolder1Title,
      /*url=*/std::string(),
      /*is_folder=*/true, /*unique_position=*/posFolder1));
  updates.push_back(CreateUpdateResponseData(
      /*server_id=*/kFolder2Id, /*parent_id=*/kBookmarkBarId, kFolder2Title,
      /*url=*/std::string(),
      /*is_folder=*/true, /*unique_position=*/posFolder2));
  updates.push_back(CreateUpdateResponseData(
      /*server_id=*/kFolder3Id, /*parent_id=*/kBookmarkBarId, kFolder3Title,
      /*url=*/std::string(),
      /*is_folder=*/true, /*unique_position=*/posFolder3));

  // -------- The expected merge outcome --------
  // bookmark_bar
  //  |- folder 1
  //  |- folder 3
  //  |- folder 2

  std::unique_ptr<SyncedBookmarkTracker> tracker =
      Merge(std::move(updates), bookmark_model.get());
  ASSERT_THAT(bookmark_bar_node->children().size(), Eq(3u));

  EXPECT_THAT(bookmark_bar_node->children()[0]->GetTitle(),
              Eq(base::ASCIIToUTF16(kFolder1Title)));
  EXPECT_THAT(bookmark_bar_node->children()[1]->GetTitle(),
              Eq(base::ASCIIToUTF16(kFolder3Title)));
  EXPECT_THAT(bookmark_bar_node->children()[2]->GetTitle(),
              Eq(base::ASCIIToUTF16(kFolder2Title)));

  // Verify the tracker contents.
  EXPECT_THAT(tracker->TrackedEntitiesCountForTest(), Eq(4U));

  // There should be no local changes.
  std::vector<const SyncedBookmarkTracker::Entity*> local_changes =
      tracker->GetEntitiesWithLocalChanges(kMaxEntries);
  EXPECT_THAT(local_changes.size(), Eq(0U));

  // Verify positions in tracker.
  EXPECT_TRUE(PositionsInTrackerMatchModel(bookmark_bar_node, *tracker));
}

TEST(BookmarkModelMergerTest, ShouldMergeFaviconsForRemoteNodesOnly) {
  const std::string kTitle1 = "title1";
  const GURL kUrl1("http://www.url1.com");
  // -------- The local model --------
  // bookmark_bar
  //  |- title 1

  std::unique_ptr<bookmarks::BookmarkModel> bookmark_model =
      bookmarks::TestBookmarkClient::CreateModel();

  const bookmarks::BookmarkNode* bookmark_bar_node =
      bookmark_model->bookmark_bar_node();
  bookmark_model->AddURL(
      /*parent=*/bookmark_bar_node, /*index=*/0, base::UTF8ToUTF16(kTitle1),
      kUrl1);

  // -------- The remote model --------
  // bookmark_bar
  //  |- title 2

  const std::string kTitle2 = "title2";
  const std::string kId2 = "Id2";
  const GURL kUrl2("http://www.url2.com");
  const GURL kIcon2Url("http://www.icon-url.com");
  syncer::UniquePosition pos2 = syncer::UniquePosition::InitialPosition(
      syncer::UniquePosition::RandomSuffix());

  syncer::UpdateResponseDataList updates;
  updates.push_back(CreateBookmarkBarNodeUpdateData());
  updates.push_back(CreateUpdateResponseData(
      /*server_id=*/kId2, /*parent_id=*/kBookmarkBarId, kTitle2, kUrl2.spec(),
      /*is_folder=*/false, /*unique_position=*/pos2, base::GenerateGUID(),
      kIcon2Url.spec(),
      /*icon_data=*/"PNG"));

  // -------- The expected merge outcome --------
  // bookmark_bar
  //  |- title 2
  //  |- title 1

  std::unique_ptr<SyncedBookmarkTracker> tracker =
      SyncedBookmarkTracker::CreateEmpty(sync_pb::ModelTypeState());
  testing::NiceMock<favicon::MockFaviconService> favicon_service;

  // Favicon should be set for the remote node.
  EXPECT_CALL(favicon_service,
              AddPageNoVisitForBookmark(kUrl2, base::UTF8ToUTF16(kTitle2)));
  EXPECT_CALL(favicon_service, MergeFavicon(kUrl2, _, _, _, _));

  BookmarkModelMerger(std::move(updates), bookmark_model.get(),
                      &favicon_service, tracker.get())
      .Merge();
}

// This tests that canonical titles produced by legacy clients are properly
// matched. Legacy clients append blank space to empty titles.
TEST(BookmarkModelMergerTest,
     ShouldMergeLocalAndRemoteNodesWhenRemoteHasLegacyCanonicalTitle) {
  const std::string kLocalTitle = "";
  const std::string kRemoteTitle = " ";
  const std::string kId = "Id";

  std::unique_ptr<bookmarks::BookmarkModel> bookmark_model =
      bookmarks::TestBookmarkClient::CreateModel();

  // -------- The local model --------
  const bookmarks::BookmarkNode* bookmark_bar_node =
      bookmark_model->bookmark_bar_node();
  const bookmarks::BookmarkNode* folder = bookmark_model->AddFolder(
      /*parent=*/bookmark_bar_node, /*index=*/0,
      base::UTF8ToUTF16(kLocalTitle));
  ASSERT_TRUE(folder);

  // -------- The remote model --------
  const std::string suffix = syncer::UniquePosition::RandomSuffix();
  syncer::UniquePosition pos = syncer::UniquePosition::InitialPosition(suffix);

  syncer::UpdateResponseDataList updates;
  updates.push_back(CreateBookmarkBarNodeUpdateData());
  updates.push_back(UpdateResponseDataBuilder(/*server_id=*/kId,
                                              /*parent_id=*/kBookmarkBarId,
                                              kRemoteTitle,
                                              /*unique_position=*/pos)
                        .SetLegacyTitleOnly()
                        .Build());

  std::unique_ptr<SyncedBookmarkTracker> tracker =
      Merge(std::move(updates), bookmark_model.get());

  // Both titles should have matched against each other and only node is in the
  // model and the tracker.
  EXPECT_THAT(bookmark_bar_node->children().size(), Eq(1u));
  EXPECT_THAT(tracker->TrackedEntitiesCountForTest(), Eq(2U));
}

// This tests that truncated titles produced by legacy clients are properly
// matched.
TEST(BookmarkModelMergerTest,
     ShouldMergeLocalAndRemoteNodesWhenRemoteHasLegacyTruncatedTitle) {
  const std::string kLocalLongTitle =
      "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrst"
      "uvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMN"
      "OPQRSTUVWXYZabcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZabcdefgh"
      "ijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyzAB"
      "CDEFGHIJKLMNOPQRSTUVWXYZ";
  const std::string kRemoteTruncatedTitle =
      "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrst"
      "uvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMN"
      "OPQRSTUVWXYZabcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZabcdefgh"
      "ijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTU";
  const std::string kId = "Id";

  std::unique_ptr<bookmarks::BookmarkModel> bookmark_model =
      bookmarks::TestBookmarkClient::CreateModel();

  // -------- The local model --------
  const bookmarks::BookmarkNode* bookmark_bar_node =
      bookmark_model->bookmark_bar_node();
  const bookmarks::BookmarkNode* folder = bookmark_model->AddFolder(
      /*parent=*/bookmark_bar_node, /*index=*/0,
      base::UTF8ToUTF16(kLocalLongTitle));
  ASSERT_TRUE(folder);

  // -------- The remote model --------
  const std::string suffix = syncer::UniquePosition::RandomSuffix();
  syncer::UniquePosition pos = syncer::UniquePosition::InitialPosition(suffix);

  syncer::UpdateResponseDataList updates;
  updates.push_back(CreateBookmarkBarNodeUpdateData());
  updates.push_back(CreateUpdateResponseData(
      /*server_id=*/kId, /*parent_id=*/kBookmarkBarId, kRemoteTruncatedTitle,
      /*url=*/std::string(),
      /*is_folder=*/true, /*unique_position=*/pos));

  std::unique_ptr<SyncedBookmarkTracker> tracker =
      SyncedBookmarkTracker::CreateEmpty(sync_pb::ModelTypeState());
  testing::NiceMock<favicon::MockFaviconService> favicon_service;
  BookmarkModelMerger(std::move(updates), bookmark_model.get(),
                      &favicon_service, tracker.get())
      .Merge();

  // Both titles should have matched against each other and only node is in the
  // model and the tracker.
  EXPECT_THAT(bookmark_bar_node->children().size(), Eq(1u));
  EXPECT_THAT(tracker->TrackedEntitiesCountForTest(), Eq(2U));
}

TEST(BookmarkModelMergerTest,
     ShouldMergeNodesWhenRemoteHasLegacyTruncatedTitleInFullTitle) {
  const std::string kLocalLongTitle(300, 'A');
  const std::string kRemoteTruncatedFullTitle(255, 'A');
  const std::string kId = "Id";

  std::unique_ptr<bookmarks::BookmarkModel> bookmark_model =
      bookmarks::TestBookmarkClient::CreateModel();

  // -------- The local model --------
  const bookmarks::BookmarkNode* bookmark_bar_node =
      bookmark_model->bookmark_bar_node();
  const bookmarks::BookmarkNode* folder = bookmark_model->AddFolder(
      /*parent=*/bookmark_bar_node, /*index=*/0,
      base::UTF8ToUTF16(kLocalLongTitle));
  ASSERT_TRUE(folder);

  // -------- The remote model --------
  const std::string suffix = syncer::UniquePosition::RandomSuffix();
  syncer::UniquePosition pos = syncer::UniquePosition::InitialPosition(suffix);

  syncer::UpdateResponseDataList updates;
  updates.push_back(CreateBookmarkBarNodeUpdateData());
  updates.push_back(CreateUpdateResponseData(
      /*server_id=*/kId, /*parent_id=*/kBookmarkBarId,
      kRemoteTruncatedFullTitle,
      /*url=*/std::string(),
      /*is_folder=*/true, /*unique_position=*/pos));

  updates.back().entity.specifics.mutable_bookmark()->set_full_title(
      kRemoteTruncatedFullTitle);

  std::unique_ptr<SyncedBookmarkTracker> tracker =
      SyncedBookmarkTracker::CreateEmpty(sync_pb::ModelTypeState());
  testing::NiceMock<favicon::MockFaviconService> favicon_service;
  BookmarkModelMerger(std::move(updates), bookmark_model.get(),
                      &favicon_service, tracker.get())
      .Merge();

  // Both titles should have matched against each other and only node is in the
  // model and the tracker.
  EXPECT_THAT(bookmark_bar_node->children().size(), Eq(1u));
  EXPECT_THAT(tracker->TrackedEntitiesCountForTest(), Eq(2U));
}

// This test checks that local node with truncated title will merge with remote
// node which has full title.
TEST(BookmarkModelMergerTest,
     ShouldMergeLocalAndRemoteNodesWhenLocalHasLegacyTruncatedTitle) {
  const std::string kRemoteFullTitle(300, 'A');
  const std::string kLocalTruncatedTitle(255, 'A');
  const std::string kId = "Id";

  std::unique_ptr<bookmarks::BookmarkModel> bookmark_model =
      bookmarks::TestBookmarkClient::CreateModel();

  // -------- The local model --------
  const bookmarks::BookmarkNode* bookmark_bar_node =
      bookmark_model->bookmark_bar_node();
  const bookmarks::BookmarkNode* folder = bookmark_model->AddFolder(
      /*parent=*/bookmark_bar_node, /*index=*/0,
      base::UTF8ToUTF16(kLocalTruncatedTitle));
  ASSERT_TRUE(folder);

  // -------- The remote model --------
  const std::string suffix = syncer::UniquePosition::RandomSuffix();
  syncer::UniquePosition pos = syncer::UniquePosition::InitialPosition(suffix);

  syncer::UpdateResponseDataList updates;
  updates.push_back(CreateBookmarkBarNodeUpdateData());
  updates.push_back(CreateUpdateResponseData(
      /*server_id=*/kId, /*parent_id=*/kBookmarkBarId,
      sync_bookmarks::FullTitleToLegacyCanonicalizedTitle(kRemoteFullTitle),
      /*url=*/std::string(),
      /*is_folder=*/true, /*unique_position=*/pos));
  ASSERT_EQ(
      kLocalTruncatedTitle,
      updates.back().entity.specifics.bookmark().legacy_canonicalized_title());

  updates.back().entity.specifics.mutable_bookmark()->set_full_title(
      kRemoteFullTitle);

  std::unique_ptr<SyncedBookmarkTracker> tracker =
      SyncedBookmarkTracker::CreateEmpty(sync_pb::ModelTypeState());
  testing::NiceMock<favicon::MockFaviconService> favicon_service;
  BookmarkModelMerger(std::move(updates), bookmark_model.get(),
                      &favicon_service, tracker.get())
      .Merge();

  // Both titles should have matched against each other and only node is in the
  // model and the tracker.
  EXPECT_THAT(bookmark_bar_node->children().size(), Eq(1u));
  EXPECT_THAT(tracker->TrackedEntitiesCountForTest(), Eq(2U));
}

TEST(BookmarkModelMergerTest, ShouldMergeAndUseRemoteGUID) {
  const std::string kId = "Id";
  const std::string kTitle = "Title";
  const std::string kRemoteGuid = base::GenerateGUID();

  std::unique_ptr<bookmarks::BookmarkModel> bookmark_model =
      bookmarks::TestBookmarkClient::CreateModel();

  // -------- The local model --------
  const bookmarks::BookmarkNode* bookmark_bar_node =
      bookmark_model->bookmark_bar_node();
  const bookmarks::BookmarkNode* folder = bookmark_model->AddFolder(
      /*parent=*/bookmark_bar_node, /*index=*/0, base::UTF8ToUTF16(kTitle));
  ASSERT_TRUE(folder);

  // -------- The remote model --------
  syncer::UpdateResponseDataList updates;
  updates.push_back(CreateBookmarkBarNodeUpdateData());
  updates.push_back(CreateUpdateResponseData(
      /*server_id=*/kId, /*parent_id=*/kBookmarkBarId, kTitle,
      /*url=*/std::string(),
      /*is_folder=*/true, /*unique_position=*/MakeRandomPosition(),
      /*guid=*/kRemoteGuid));

  std::unique_ptr<SyncedBookmarkTracker> tracker =
      Merge(std::move(updates), bookmark_model.get());

  // Node should have been replaced and GUID should be set to that stored in the
  // specifics.
  ASSERT_EQ(bookmark_bar_node->children().size(), 1u);
  const bookmarks::BookmarkNode* bookmark =
      bookmark_model->bookmark_bar_node()->children()[0].get();
  EXPECT_EQ(bookmark->guid(), kRemoteGuid);
  EXPECT_THAT(tracker->GetEntityForBookmarkNode(bookmark), NotNull());
}

TEST(BookmarkModelMergerTest,
     ShouldMergeAndKeepOldGUIDWhenRemoteGUIDIsInvalid) {
  const std::string kId = "Id";
  const std::string kTitle = "Title";

  std::unique_ptr<bookmarks::BookmarkModel> bookmark_model =
      bookmarks::TestBookmarkClient::CreateModel();

  // -------- The local model --------
  const bookmarks::BookmarkNode* bookmark_bar_node =
      bookmark_model->bookmark_bar_node();
  const bookmarks::BookmarkNode* folder = bookmark_model->AddFolder(
      /*parent=*/bookmark_bar_node, /*index=*/0, base::UTF8ToUTF16(kTitle));
  ASSERT_TRUE(folder);
  const std::string old_guid = folder->guid();

  // -------- The remote model --------
  syncer::UpdateResponseDataList updates;
  updates.push_back(CreateBookmarkBarNodeUpdateData());
  updates.push_back(CreateUpdateResponseData(
      /*server_id=*/kId, /*parent_id=*/kBookmarkBarId, kTitle,
      /*url=*/std::string(),
      /*is_folder=*/true,
      /*unique_position=*/MakeRandomPosition(),
      /*guid=*/""));

  std::unique_ptr<SyncedBookmarkTracker> tracker =
      Merge(std::move(updates), bookmark_model.get());

  // Node should not have been replaced and GUID should not have been set to
  // that stored in the specifics, as it was invalid.
  ASSERT_EQ(bookmark_bar_node->children().size(), 1u);
  const bookmarks::BookmarkNode* bookmark =
      bookmark_model->bookmark_bar_node()->children()[0].get();
  EXPECT_EQ(bookmark->guid(), old_guid);
  EXPECT_THAT(tracker->GetEntityForBookmarkNode(bookmark), NotNull());
}

TEST(BookmarkModelMergerTest, ShouldMergeBookmarkByGUID) {
  const std::string kId = "Id";
  const std::string kLocalTitle = "Title 1";
  const std::string kRemoteTitle = "Title 2";
  const std::string kUrl = "http://www.foo.com/";
  const std::string kGuid = base::GenerateGUID();

  std::unique_ptr<bookmarks::BookmarkModel> bookmark_model =
      bookmarks::TestBookmarkClient::CreateModel();

  // -------- The local model --------
  // bookmark_bar
  //  | - bookmark(kGuid/kLocalTitle)

  const bookmarks::BookmarkNode* bookmark_bar_node =
      bookmark_model->bookmark_bar_node();
  const bookmarks::BookmarkNode* bookmark = bookmark_model->AddURL(
      /*parent=*/bookmark_bar_node, /*index=*/0, base::UTF8ToUTF16(kLocalTitle),
      GURL(kUrl), nullptr, base::Time::Now(), kGuid);
  ASSERT_TRUE(bookmark);
  ASSERT_THAT(bookmark_bar_node->children(), ElementRawPointersAre(bookmark));

  // -------- The remote model --------
  // bookmark_bar
  //  | - bookmark(kGuid/kRemoteTitle)

  syncer::UpdateResponseDataList updates;
  updates.push_back(CreateBookmarkBarNodeUpdateData());
  updates.push_back(CreateUpdateResponseData(
      /*server_id=*/kId, /*parent_id=*/kBookmarkBarId, kRemoteTitle,
      /*url=*/kUrl,
      /*is_folder=*/false,
      /*unique_position=*/MakeRandomPosition(),
      /*guid=*/kGuid));

  std::unique_ptr<SyncedBookmarkTracker> tracker =
      Merge(std::move(updates), bookmark_model.get());

  // -------- The merged model --------
  // bookmark_bar
  //  |- bookmark(kGuid/kRemoteTitle)

  // Node should have been merged.
  EXPECT_THAT(bookmark_bar_node->children(), ElementRawPointersAre(bookmark));
  EXPECT_EQ(bookmark->GetTitle(), base::UTF8ToUTF16(kRemoteTitle));
  EXPECT_THAT(tracker->GetEntityForBookmarkNode(bookmark), NotNull());
}

TEST(BookmarkModelMergerTest, ShouldMergeBookmarkByGUIDAndReparent) {
  const std::string kId = "Id";
  const std::string kLocalTitle = "Title 1";
  const std::string kRemoteTitle = "Title 2";
  const std::string kUrl = "http://www.foo.com/";
  const std::string kGuid = base::GenerateGUID();

  std::unique_ptr<bookmarks::BookmarkModel> bookmark_model =
      bookmarks::TestBookmarkClient::CreateModel();

  // -------- The local model --------
  // bookmark_bar
  //  | - folder
  //    | - bookmark(kGuid)

  const bookmarks::BookmarkNode* bookmark_bar_node =
      bookmark_model->bookmark_bar_node();
  const bookmarks::BookmarkNode* folder = bookmark_model->AddFolder(
      /*parent=*/bookmark_bar_node, /*index=*/0,
      base::UTF8ToUTF16("Folder Title"), nullptr, base::GenerateGUID());
  const bookmarks::BookmarkNode* bookmark = bookmark_model->AddURL(
      /*parent=*/folder, /*index=*/0, base::UTF8ToUTF16(kLocalTitle),
      GURL(kUrl), nullptr, base::Time::Now(), kGuid);
  ASSERT_TRUE(folder);
  ASSERT_TRUE(bookmark);
  ASSERT_THAT(bookmark_bar_node->children(), ElementRawPointersAre(folder));
  ASSERT_THAT(folder->children(), ElementRawPointersAre(bookmark));

  // -------- The remote model --------
  // bookmark_bar
  //  |- bookmark(kGuid)

  syncer::UpdateResponseDataList updates;
  updates.push_back(CreateBookmarkBarNodeUpdateData());
  updates.push_back(CreateUpdateResponseData(
      /*server_id=*/kId, /*parent_id=*/kBookmarkBarId, kRemoteTitle,
      /*url=*/kUrl,
      /*is_folder=*/false,
      /*unique_position=*/MakeRandomPosition(),
      /*guid=*/kGuid));

  std::unique_ptr<SyncedBookmarkTracker> tracker =
      Merge(std::move(updates), bookmark_model.get());

  // -------- The merged model --------
  // bookmark_bar
  //  | - bookmark(kGuid/kRemoteTitle)
  //  | - folder

  // Node should have been merged and the local node should have been
  // reparented.
  EXPECT_THAT(bookmark_bar_node->children(),
              ElementRawPointersAre(bookmark, folder));
  EXPECT_EQ(folder->children().size(), 0u);
  EXPECT_EQ(bookmark->GetTitle(), base::UTF8ToUTF16(kRemoteTitle));
  EXPECT_THAT(tracker->GetEntityForBookmarkNode(bookmark), NotNull());
  EXPECT_THAT(tracker->GetEntityForBookmarkNode(folder), NotNull());
}

TEST(BookmarkModelMergerTest, ShouldMergeFolderByGUIDAndNotSemantics) {
  const std::string kFolderId = "Folder Id";
  const std::string kTitle1 = "Title 1";
  const std::string kTitle2 = "Title 2";
  const std::string kUrl = "http://www.foo.com/";
  const std::string kGuid1 = base::GenerateGUID();
  const std::string kGuid2 = base::GenerateGUID();

  std::unique_ptr<bookmarks::BookmarkModel> bookmark_model =
      bookmarks::TestBookmarkClient::CreateModel();

  // -------- The local model --------
  // bookmark_bar
  //  | - folder 1 (kGuid1/kTitle1)
  //    | - folder 2 (kGuid2/kTitle2)

  const bookmarks::BookmarkNode* bookmark_bar_node =
      bookmark_model->bookmark_bar_node();
  const bookmarks::BookmarkNode* folder1 =
      bookmark_model->AddFolder(/*parent=*/bookmark_bar_node, /*index=*/0,
                                base::UTF8ToUTF16(kTitle1), nullptr, kGuid1);
  const bookmarks::BookmarkNode* folder2 =
      bookmark_model->AddFolder(/*parent=*/folder1, /*index=*/0,
                                base::UTF8ToUTF16(kTitle2), nullptr, kGuid2);
  ASSERT_TRUE(folder1);
  ASSERT_TRUE(folder2);
  ASSERT_THAT(bookmark_bar_node->children(), ElementRawPointersAre(folder1));
  ASSERT_THAT(folder1->children(), ElementRawPointersAre(folder2));

  // -------- The remote model --------
  // bookmark_bar
  //  | - folder (kGuid2/kTitle1)

  syncer::UpdateResponseDataList updates;
  updates.push_back(CreateBookmarkBarNodeUpdateData());

  // Add a remote folder to correspond to the local folder by GUID and
  // semantics.
  updates.push_back(CreateUpdateResponseData(
      /*server_id=*/kFolderId, /*parent_id=*/kBookmarkBarId, kTitle1,
      /*url=*/"",
      /*is_folder=*/true,
      /*unique_position=*/MakeRandomPosition(),
      /*guid=*/kGuid2));

  std::unique_ptr<SyncedBookmarkTracker> tracker =
      Merge(std::move(updates), bookmark_model.get());

  // -------- The merged model --------
  // bookmark_bar
  //  | - folder 2 (kGuid2/kTitle1)
  //  | - folder 1 (kGuid1/kTitle1)

  // Node should have been merged with its GUID match.
  EXPECT_THAT(bookmark_bar_node->children(),
              ElementRawPointersAre(folder2, folder1));
  EXPECT_EQ(folder1->guid(), kGuid1);
  EXPECT_EQ(folder1->GetTitle(), base::UTF8ToUTF16(kTitle1));
  EXPECT_EQ(folder1->children().size(), 0u);
  EXPECT_EQ(folder2->guid(), kGuid2);
  EXPECT_EQ(folder2->GetTitle(), base::UTF8ToUTF16(kTitle1));
  EXPECT_THAT(tracker->GetEntityForBookmarkNode(folder1), NotNull());
  EXPECT_THAT(tracker->GetEntityForBookmarkNode(folder2), NotNull());
}

TEST(BookmarkModelMergerTest, ShouldIgnoreChildrenForNonFolderNodes) {
  const std::string kParentId = "parent_id";
  const std::string kChildId = "child_id";
  const std::string kParentTitle = "Parent Title";
  const std::string kChildTitle = "Child Title";
  const std::string kGuid1 = base::GenerateGUID();
  const std::string kGuid2 = base::GenerateGUID();
  const std::string kUrl1 = "http://www.foo.com/";
  const std::string kUrl2 = "http://www.bar.com/";

  // -------- The remote model --------
  // bookmark_bar
  //  | - bookmark (kGuid1/kParentTitle, not a folder)
  //    | - bookmark

  syncer::UpdateResponseDataList updates;
  updates.push_back(CreateBookmarkBarNodeUpdateData());

  const std::string suffix = syncer::UniquePosition::RandomSuffix();
  const syncer::UniquePosition pos1 =
      syncer::UniquePosition::InitialPosition(suffix);
  const syncer::UniquePosition pos2 =
      syncer::UniquePosition::After(pos1, suffix);

  updates.push_back(CreateUpdateResponseData(
      /*server_id=*/kParentId, /*parent_id=*/kBookmarkBarId, kParentTitle,
      /*url=*/kUrl1,
      /*is_folder=*/false,
      /*unique_position=*/pos1,
      /*guid=*/kGuid1));

  updates.push_back(CreateUpdateResponseData(
      /*server_id=*/kChildId, /*parent_id=*/kParentId, kChildTitle,
      /*url=*/kUrl2,
      /*is_folder=*/false,
      /*unique_position=*/pos2,
      /*guid=*/kGuid2));

  std::unique_ptr<bookmarks::BookmarkModel> bookmark_model =
      bookmarks::TestBookmarkClient::CreateModel();
  std::unique_ptr<SyncedBookmarkTracker> tracker =
      Merge(std::move(updates), bookmark_model.get());

  // -------- The merged model --------
  // bookmark_bar
  //  | - bookmark (kGuid1/kParentTitle)

  const bookmarks::BookmarkNode* bookmark_bar_node =
      bookmark_model->bookmark_bar_node();

  ASSERT_EQ(bookmark_bar_node->children().size(), 1u);
  EXPECT_EQ(bookmark_bar_node->children()[0]->guid(), kGuid1);
  EXPECT_EQ(bookmark_bar_node->children()[0]->GetTitle(),
            base::UTF8ToUTF16(kParentTitle));
  EXPECT_EQ(bookmark_bar_node->children()[0]->children().size(), 0u);
  EXPECT_EQ(tracker->TrackedEntitiesCountForTest(), 2U);
}

TEST(
    BookmarkModelMergerTest,
    ShouldIgnoreFolderSemanticsMatchAndLaterMatchByGUIDWithSemanticsNodeFirst) {
  const std::string kFolderId1 = "Folder Id 1";
  const std::string kFolderId2 = "Folder Id 2";
  const std::string kOriginalTitle = "Original Title";
  const std::string kNewTitle = "New Title";
  const std::string kGuid1 = base::GenerateGUID();
  const std::string kGuid2 = base::GenerateGUID();

  std::unique_ptr<bookmarks::BookmarkModel> bookmark_model =
      bookmarks::TestBookmarkClient::CreateModel();

  // -------- The local model --------
  // bookmark_bar
  //  | - folder (kGuid1/kOriginalTitle)
  //    | - bookmark

  const bookmarks::BookmarkNode* bookmark_bar_node =
      bookmark_model->bookmark_bar_node();
  const bookmarks::BookmarkNode* folder = bookmark_model->AddFolder(
      /*parent=*/bookmark_bar_node, /*index=*/0,
      base::UTF8ToUTF16(kOriginalTitle), nullptr, kGuid1);
  const bookmarks::BookmarkNode* bookmark = bookmark_model->AddURL(
      /*parent=*/folder, /*index=*/0, base::UTF8ToUTF16("Bookmark Title"),
      GURL("http://foo.com/"), nullptr, base::Time::Now(),
      base::GenerateGUID());
  ASSERT_TRUE(folder);
  ASSERT_TRUE(bookmark);
  ASSERT_THAT(bookmark_bar_node->children(), ElementRawPointersAre(folder));
  ASSERT_THAT(folder->children(), ElementRawPointersAre(bookmark));

  // -------- The remote model --------
  // bookmark_bar
  //  | - folder (kGuid2/kOriginalTitle)
  //  | - folder (kGuid1/kNewTitle)

  syncer::UpdateResponseDataList updates;
  updates.push_back(CreateBookmarkBarNodeUpdateData());

  const std::string suffix = syncer::UniquePosition::RandomSuffix();
  syncer::UniquePosition pos1 = syncer::UniquePosition::InitialPosition(suffix);
  syncer::UniquePosition pos2 = syncer::UniquePosition::After(pos1, suffix);

  // Add a remote folder to correspond to the local folder by semantics and not
  // GUID.
  updates.push_back(CreateUpdateResponseData(
      /*server_id=*/kFolderId1, /*parent_id=*/kBookmarkBarId, kOriginalTitle,
      /*url=*/"",
      /*is_folder=*/true,
      /*unique_position=*/pos1,
      /*guid=*/kGuid2));

  // Add a remote folder to correspond to the local folder by GUID and not
  // semantics.
  updates.push_back(CreateUpdateResponseData(
      /*server_id=*/kFolderId2, /*parent_id=*/kBookmarkBarId, kNewTitle,
      /*url=*/"",
      /*is_folder=*/true,
      /*unique_position=*/pos2,
      /*guid=*/kGuid1));

  std::unique_ptr<SyncedBookmarkTracker> tracker =
      Merge(std::move(updates), bookmark_model.get());

  // -------- The merged model --------
  // bookmark_bar
  //  | - folder (kGuid2/kOriginalTitle)
  //  | - folder (kGuid1/kNewTitle)
  //    | - bookmark

  // Node should have been merged with its GUID match.
  ASSERT_EQ(bookmark_bar_node->children().size(), 2u);
  EXPECT_EQ(bookmark_bar_node->children()[0]->guid(), kGuid2);
  EXPECT_EQ(bookmark_bar_node->children()[0]->GetTitle(),
            base::UTF8ToUTF16(kOriginalTitle));
  EXPECT_EQ(bookmark_bar_node->children()[0]->children().size(), 0u);
  EXPECT_EQ(bookmark_bar_node->children()[1]->guid(), kGuid1);
  EXPECT_EQ(bookmark_bar_node->children()[1]->GetTitle(),
            base::UTF8ToUTF16(kNewTitle));
  EXPECT_EQ(bookmark_bar_node->children()[1]->children().size(), 1u);
  EXPECT_THAT(tracker->TrackedEntitiesCountForTest(), Eq(4U));
}

TEST(BookmarkModelMergerTest,
     ShouldIgnoreFolderSemanticsMatchAndLaterMatchByGUIDWithGUIDNodeFirst) {
  const std::string kFolderId1 = "Folder Id 1";
  const std::string kFolderId2 = "Folder Id 2";
  const std::string kOriginalTitle = "Original Title";
  const std::string kNewTitle = "New Title";
  const std::string kGuid1 = base::GenerateGUID();
  const std::string kGuid2 = base::GenerateGUID();

  std::unique_ptr<bookmarks::BookmarkModel> bookmark_model =
      bookmarks::TestBookmarkClient::CreateModel();

  // -------- The local model --------
  // bookmark_bar
  //  | - folder (kGuid1/kOriginalTitle)
  //    | - bookmark

  const bookmarks::BookmarkNode* bookmark_bar_node =
      bookmark_model->bookmark_bar_node();
  const bookmarks::BookmarkNode* folder = bookmark_model->AddFolder(
      /*parent=*/bookmark_bar_node, /*index=*/0,
      base::UTF8ToUTF16(kOriginalTitle), nullptr, kGuid1);
  const bookmarks::BookmarkNode* bookmark = bookmark_model->AddURL(
      /*parent=*/folder, /*index=*/0, base::UTF8ToUTF16("Bookmark Title"),
      GURL("http://foo.com/"), nullptr, base::Time::Now(),
      base::GenerateGUID());
  ASSERT_TRUE(folder);
  ASSERT_TRUE(bookmark);
  ASSERT_THAT(bookmark_bar_node->children(), ElementRawPointersAre(folder));
  ASSERT_THAT(folder->children(), ElementRawPointersAre(bookmark));

  // -------- The remote model --------
  // bookmark_bar
  //  | - folder (kGuid1/kNewTitle)
  //  | - folder (kGuid2/kOriginalTitle)

  syncer::UpdateResponseDataList updates;
  updates.push_back(CreateBookmarkBarNodeUpdateData());

  const std::string suffix = syncer::UniquePosition::RandomSuffix();
  syncer::UniquePosition pos1 = syncer::UniquePosition::InitialPosition(suffix);
  syncer::UniquePosition pos2 = syncer::UniquePosition::After(pos1, suffix);

  // Add a remote folder to correspond to the local folder by GUID and not
  // semantics.
  updates.push_back(CreateUpdateResponseData(
      /*server_id=*/kFolderId2, /*parent_id=*/kBookmarkBarId, kNewTitle,
      /*url=*/"",
      /*is_folder=*/true,
      /*unique_position=*/pos1,
      /*guid=*/kGuid1));

  // Add a remote folder to correspond to the local folder by
  // semantics and not GUID.
  updates.push_back(CreateUpdateResponseData(
      /*server_id=*/kFolderId1, /*parent_id=*/kBookmarkBarId, kOriginalTitle,
      /*url=*/"",
      /*is_folder=*/true,
      /*unique_position=*/pos2,
      /*guid=*/kGuid2));

  Merge(std::move(updates), bookmark_model.get());

  // -------- The merged model --------
  // bookmark_bar
  //  | - folder (kGuid1/kNewTitle)
  //  | - folder (kGuid2/kOriginalTitle)

  // Node should have been merged with its GUID match.
  ASSERT_EQ(bookmark_bar_node->children().size(), 2u);
  EXPECT_EQ(bookmark_bar_node->children()[0]->guid(), kGuid1);
  EXPECT_EQ(bookmark_bar_node->children()[0]->GetTitle(),
            base::UTF8ToUTF16(kNewTitle));
  EXPECT_EQ(bookmark_bar_node->children()[0]->children().size(), 1u);
  EXPECT_EQ(bookmark_bar_node->children()[1]->guid(), kGuid2);
  EXPECT_EQ(bookmark_bar_node->children()[1]->GetTitle(),
            base::UTF8ToUTF16(kOriginalTitle));
  EXPECT_EQ(bookmark_bar_node->children()[1]->children().size(), 0u);
}

TEST(BookmarkModelMergerTest, ShouldReplaceBookmarkGUIDWithConflictingURLs) {
  const std::string kTitle = "Title";
  const std::string kUrl1 = "http://www.foo.com/";
  const std::string kUrl2 = "http://www.bar.com/";
  const std::string kGuid = base::GenerateGUID();

  std::unique_ptr<bookmarks::BookmarkModel> bookmark_model =
      bookmarks::TestBookmarkClient::CreateModel();

  // -------- The local model --------
  // bookmark_bar
  //  | - bookmark (kGuid/kUril1)

  const bookmarks::BookmarkNode* bookmark_bar_node =
      bookmark_model->bookmark_bar_node();
  const bookmarks::BookmarkNode* bookmark = bookmark_model->AddURL(
      /*parent=*/bookmark_bar_node, /*index=*/0, base::UTF8ToUTF16(kTitle),
      GURL(kUrl1), nullptr, base::Time::Now(), kGuid);
  ASSERT_TRUE(bookmark);
  ASSERT_THAT(bookmark_bar_node->children(), ElementRawPointersAre(bookmark));

  // -------- The remote model --------
  // bookmark_bar
  //  | - bookmark (kGuid/kUrl2)

  syncer::UpdateResponseDataList updates;
  updates.push_back(CreateBookmarkBarNodeUpdateData());

  updates.push_back(CreateUpdateResponseData(  // Remote B
      /*server_id=*/"Id", /*parent_id=*/kBookmarkBarId, kTitle,
      /*url=*/kUrl2,
      /*is_folder=*/false,
      /*unique_position=*/MakeRandomPosition(),
      /*guid=*/kGuid));

  Merge(std::move(updates), bookmark_model.get());

  // -------- The merged model --------
  // bookmark_bar
  //  | - bookmark (kGuid/kUrl2)
  //  | - bookmark ([new GUID]/kUrl1)

  // Conflicting node GUID should have been replaced.
  ASSERT_EQ(bookmark_bar_node->children().size(), 2u);
  EXPECT_EQ(bookmark_bar_node->children()[0]->guid(), kGuid);
  EXPECT_EQ(bookmark_bar_node->children()[0]->url(), kUrl2);
  EXPECT_NE(bookmark_bar_node->children()[1]->guid(), kGuid);
  EXPECT_TRUE(
      base::IsValidGUIDOutputString(bookmark_bar_node->children()[1]->guid()));
  EXPECT_EQ(bookmark_bar_node->children()[1]->url(), kUrl1);
}

TEST(BookmarkModelMergerTest, ShouldReplaceBookmarkGUIDWithConflictingTypes) {
  const std::string kTitle = "Title";
  const std::string kGuid = base::GenerateGUID();

  std::unique_ptr<bookmarks::BookmarkModel> bookmark_model =
      bookmarks::TestBookmarkClient::CreateModel();

  // -------- The local model --------
  // bookmark_bar
  //  | - bookmark (kGuid)

  const bookmarks::BookmarkNode* bookmark_bar_node =
      bookmark_model->bookmark_bar_node();
  const bookmarks::BookmarkNode* bookmark = bookmark_model->AddURL(
      /*parent=*/bookmark_bar_node, /*index=*/0, base::UTF8ToUTF16(kTitle),
      GURL("http://www.foo.com/"), nullptr, base::Time::Now(), kGuid);
  ASSERT_TRUE(bookmark);
  ASSERT_THAT(bookmark_bar_node->children(), ElementRawPointersAre(bookmark));

  // -------- The remote model --------
  // bookmark_bar
  //  | - folder(kGuid)

  syncer::UpdateResponseDataList updates;
  updates.push_back(CreateBookmarkBarNodeUpdateData());

  updates.push_back(CreateUpdateResponseData(  // Remote B
      /*server_id=*/"Id", /*parent_id=*/kBookmarkBarId, kTitle,
      /*url=*/"",
      /*is_folder=*/true,
      /*unique_position=*/MakeRandomPosition(),
      /*guid=*/kGuid));

  Merge(std::move(updates), bookmark_model.get());

  // -------- The merged model --------
  // bookmark_bar
  //  | - folder (kGuid)
  //  | - bookmark ([new GUID])

  // Conflicting node GUID should have been replaced.
  ASSERT_EQ(bookmark_bar_node->children().size(), 2u);
  EXPECT_EQ(bookmark_bar_node->children()[0]->guid(), kGuid);
  EXPECT_TRUE(bookmark_bar_node->children()[0]->is_folder());
  EXPECT_NE(bookmark_bar_node->children()[1]->guid(), kGuid);
  EXPECT_TRUE(
      base::IsValidGUIDOutputString(bookmark_bar_node->children()[1]->guid()));
  EXPECT_FALSE(bookmark_bar_node->children()[1]->is_folder());
}

TEST(BookmarkModelMergerTest,
     ShouldReplaceBookmarkGUIDWithConflictingTypesAndLocalChildren) {
  const std::string kGuid = base::GenerateGUID();
  const std::string kGuid2 = base::GenerateGUID();

  std::unique_ptr<bookmarks::BookmarkModel> bookmark_model =
      bookmarks::TestBookmarkClient::CreateModel();

  // -------- The local model --------
  // bookmark_bar
  //  | - folder (kGuid)
  //    | - bookmark (kGuid2)

  const bookmarks::BookmarkNode* bookmark_bar_node =
      bookmark_model->bookmark_bar_node();
  const bookmarks::BookmarkNode* folder = bookmark_model->AddFolder(
      /*parent=*/bookmark_bar_node, /*index=*/0,
      base::UTF8ToUTF16("Folder Title"), nullptr, kGuid);
  const bookmarks::BookmarkNode* bookmark = bookmark_model->AddURL(
      /*parent=*/folder, /*index=*/0, base::UTF8ToUTF16("Foo's title"),
      GURL("http://foo.com"), nullptr, base::Time::Now(), kGuid2);
  ASSERT_TRUE(folder);
  ASSERT_TRUE(bookmark);
  ASSERT_THAT(bookmark_bar_node->children(), ElementRawPointersAre(folder));
  ASSERT_THAT(folder->children(), ElementRawPointersAre(bookmark));

  // -------- The remote model --------
  // bookmark_bar
  //  | - bookmark (kGuid)

  syncer::UpdateResponseDataList updates;
  updates.push_back(CreateBookmarkBarNodeUpdateData());

  updates.push_back(CreateUpdateResponseData(
      /*server_id=*/"Id", /*parent_id=*/kBookmarkBarId, "Bar's title",
      "http://bar.com/",
      /*is_folder=*/false,
      /*unique_position=*/MakeRandomPosition(),
      /*guid=*/kGuid));

  Merge(std::move(updates), bookmark_model.get());

  // -------- The merged model --------
  // bookmark_bar
  //  | - bookmark (kGuid)
  //  | - folder ([new GUID])
  //    | - bookmark (kGuid2)

  // Conflicting node GUID should have been replaced.
  ASSERT_EQ(bookmark_bar_node->children().size(), 2u);
  EXPECT_EQ(bookmark_bar_node->children()[0]->guid(), kGuid);
  EXPECT_NE(bookmark_bar_node->children()[1]->guid(), kGuid);
  EXPECT_NE(bookmark_bar_node->children()[1]->guid(), kGuid2);
  EXPECT_FALSE(bookmark_bar_node->children()[0]->is_folder());
  EXPECT_TRUE(bookmark_bar_node->children()[1]->is_folder());
  EXPECT_EQ(bookmark_bar_node->children()[1]->children().size(), 1u);
  EXPECT_FALSE(bookmark_bar_node->children()[1]->children()[0]->is_folder());
  EXPECT_EQ(bookmark_bar_node->children()[1]->children()[0]->guid(), kGuid2);
}

// Tests that the GUID-based matching algorithm handles well the case where a
// local bookmark matches a remote bookmark that is orphan. In this case the
// remote node should be ignored and the local bookmark included in the merged
// tree.
TEST(BookmarkModelMergerTest, ShouldIgnoreRemoteGUIDIfOrphanNode) {
  const std::string kId = "Id";
  const std::string kInexistentParentId = "InexistentParentId";
  const std::string kTitle = "Title";
  const std::string kUrl = "http://www.foo.com/";
  const std::string kGuid = base::GenerateGUID();

  std::unique_ptr<bookmarks::BookmarkModel> bookmark_model =
      bookmarks::TestBookmarkClient::CreateModel();

  // -------- The local model --------
  // bookmark_bar
  //  | - bookmark(kGuid/kTitle)

  const bookmarks::BookmarkNode* bookmark_bar_node =
      bookmark_model->bookmark_bar_node();
  const bookmarks::BookmarkNode* bookmark = bookmark_model->AddURL(
      /*parent=*/bookmark_bar_node, /*index=*/0, base::UTF8ToUTF16(kTitle),
      GURL(kUrl), nullptr, base::Time::Now(), kGuid);
  ASSERT_TRUE(bookmark);
  ASSERT_THAT(bookmark_bar_node->children(), ElementRawPointersAre(bookmark));

  // -------- The remote model --------
  // bookmark_bar
  // Orphan node: bookmark(kGuid/kTitle)

  syncer::UpdateResponseDataList updates;
  updates.push_back(CreateBookmarkBarNodeUpdateData());
  updates.push_back(CreateUpdateResponseData(
      /*server_id=*/kId, /*parent_id=*/kInexistentParentId, kTitle,
      /*url=*/kUrl,
      /*is_folder=*/false,
      /*unique_position=*/MakeRandomPosition(),
      /*guid=*/kGuid));

  std::unique_ptr<SyncedBookmarkTracker> tracker =
      Merge(std::move(updates), bookmark_model.get());

  // -------- The merged model --------
  // bookmark_bar
  //  |- bookmark(kGuid/kTitle)

  // The local node should have been tracked.
  EXPECT_THAT(bookmark_bar_node->children(), ElementRawPointersAre(bookmark));
  EXPECT_EQ(bookmark->GetTitle(), base::UTF8ToUTF16(kTitle));
  EXPECT_THAT(tracker->GetEntityForBookmarkNode(bookmark), NotNull());
}

// Tests that the GUID-based matching algorithm handles well the case where a
// local bookmark matches a remote bookmark that contains invalid specifics
// (e.g. invalid URL). In this case the remote node should be ignored and the
// local bookmark included in the merged tree.
TEST(BookmarkModelMergerTest, ShouldIgnoreRemoteGUIDIfInvalidSpecifics) {
  const std::string kId = "Id";
  const std::string kTitle = "Title";
  const std::string kLocalUrl = "http://www.foo.com/";
  const std::string kInvalidUrl = "invalidurl";
  const std::string kGuid = base::GenerateGUID();

  std::unique_ptr<bookmarks::BookmarkModel> bookmark_model =
      bookmarks::TestBookmarkClient::CreateModel();

  // -------- The local model --------
  // bookmark_bar
  //  | - bookmark(kGuid/kLocalUrl/kTitle)

  const bookmarks::BookmarkNode* bookmark_bar_node =
      bookmark_model->bookmark_bar_node();
  const bookmarks::BookmarkNode* bookmark = bookmark_model->AddURL(
      /*parent=*/bookmark_bar_node, /*index=*/0, base::UTF8ToUTF16(kTitle),
      GURL(kLocalUrl), nullptr, base::Time::Now(), kGuid);
  ASSERT_TRUE(bookmark);
  ASSERT_THAT(bookmark_bar_node->children(), ElementRawPointersAre(bookmark));

  // -------- The remote model --------
  // bookmark_bar
  //  | - bookmark (kGuid/kInvalidURL/kTitle)

  syncer::UpdateResponseDataList updates;
  updates.push_back(CreateBookmarkBarNodeUpdateData());
  updates.push_back(CreateUpdateResponseData(
      /*server_id=*/kId, /*parent_id=*/kBookmarkBarId, kTitle,
      /*url=*/kInvalidUrl,
      /*is_folder=*/false,
      /*unique_position=*/MakeRandomPosition(),
      /*guid=*/kGuid));

  std::unique_ptr<SyncedBookmarkTracker> tracker =
      Merge(std::move(updates), bookmark_model.get());

  // -------- The merged model --------
  // bookmark_bar
  //  |- bookmark(kGuid/kLocalUrl/kTitle)

  // The local node should have been tracked.
  EXPECT_THAT(bookmark_bar_node->children(), ElementRawPointersAre(bookmark));
  EXPECT_EQ(bookmark->url(), GURL(kLocalUrl));
  EXPECT_EQ(bookmark->GetTitle(), base::UTF8ToUTF16(kTitle));
  EXPECT_THAT(tracker->GetEntityForBookmarkNode(bookmark), NotNull());
}

// Tests that updates with a GUID that is different to originator client item ID
// are ignored.
TEST(BookmarkModelMergerTest, ShouldIgnoreRemoteUpdateWithInvalidGUID) {
  const std::string kId1 = "Id1";
  const std::string kId2 = "Id2";
  const std::string kTitle1 = "Title1";
  const std::string kTitle2 = "Title2";
  const std::string kLocalTitle = "LocalTitle";
  const std::string kUrl = "http://www.foo.com/";
  const std::string kGuid = base::GenerateGUID();

  std::unique_ptr<bookmarks::BookmarkModel> bookmark_model =
      bookmarks::TestBookmarkClient::CreateModel();

  // -------- The local model --------
  //  | - bookmark(kGuid/kUrl/kLocalTitle)
  const bookmarks::BookmarkNode* bookmark_bar_node =
      bookmark_model->bookmark_bar_node();
  const bookmarks::BookmarkNode* bookmark = bookmark_model->AddURL(
      /*parent=*/bookmark_bar_node, /*index=*/0, base::UTF8ToUTF16(kLocalTitle),
      GURL(kUrl), nullptr, base::Time::Now(), kGuid);
  ASSERT_TRUE(bookmark);
  ASSERT_THAT(bookmark_bar_node->children(), ElementRawPointersAre(bookmark));

  // -------- The remote model --------
  // bookmark_bar
  //  | - bookmark (kGuid/kUrl/kTitle1)
  //  | - bookmark (kGuid/kUrl/kTitle2)
  const std::string suffix = syncer::UniquePosition::RandomSuffix();
  syncer::UniquePosition position1 =
      syncer::UniquePosition::InitialPosition(suffix);
  syncer::UniquePosition position2 =
      syncer::UniquePosition::After(position1, suffix);

  syncer::UpdateResponseDataList updates;
  updates.push_back(CreateBookmarkBarNodeUpdateData());
  updates.push_back(CreateUpdateResponseData(
      /*server_id=*/kId1, /*parent_id=*/kBookmarkBarId, kTitle1,
      /*url=*/kUrl,
      /*is_folder=*/false, /*unique_position=*/position1,
      /*guid=*/kGuid));
  updates.push_back(CreateUpdateResponseData(
      /*server_id=*/kId2, /*parent_id=*/kBookmarkBarId, kTitle2,
      /*url=*/kUrl,
      /*is_folder=*/false, /*unique_position=*/position2,
      /*guid=*/kGuid));

  // |originator_client_item_id| cannot itself be duplicated because
  // ModelTypeWorker guarantees otherwise.
  updates.back().entity.originator_client_item_id = base::GenerateGUID();

  std::unique_ptr<SyncedBookmarkTracker> tracker =
      Merge(std::move(updates), bookmark_model.get());

  // -------- The merged model --------
  //  | - bookmark (kGuid/kUrl/kTitle1)

  // The second remote node should have been filtered out.
  ASSERT_EQ(bookmark_bar_node->children().size(), 1u);
  const bookmarks::BookmarkNode* merged_bookmark =
      bookmark_model->bookmark_bar_node()->children()[0].get();
  EXPECT_THAT(merged_bookmark->guid(), Eq(kGuid));
  EXPECT_THAT(tracker->GetEntityForBookmarkNode(merged_bookmark), NotNull());
}

// Regression test for crbug.com/1050776. Verifies that computing the unique
// position does not crash when processing local creation of bookmark during
// initial merge.
TEST(BookmarkModelMergerTest,
     ShouldProcessLocalCreationWithUntrackedPredecessorNode) {
  const std::string kFolder1Title = "folder1";
  const std::string kFolder2Title = "folder2";

  const std::string kUrl1Title = "url1";
  const std::string kUrl2Title = "url2";

  const std::string kUrl1 = "http://www.url1.com/";
  const std::string kUrl2 = "http://www.url2.com/";

  const std::string kFolder1Id = "Folder1Id";
  const std::string kFolder2Id = "Folder2Id";
  const std::string kUrl1Id = "Url1Id";

  // It is needed to use at least two folders to reproduce the crash. It is
  // needed because the bookmarks are processed in the order of remote entities
  // on the same level of the tree. To start processing of locally created
  // bookmarks while other remote bookmarks are not processed we need to use at
  // least one local folder with several urls.
  //
  // -------- The local model --------
  // bookmark_bar
  //  |- folder 1
  //    |- url1(http://www.url1.com)
  //    |- url2(http://www.url2.com)

  std::unique_ptr<bookmarks::BookmarkModel> bookmark_model =
      bookmarks::TestBookmarkClient::CreateModel();

  const bookmarks::BookmarkNode* bookmark_bar_node =
      bookmark_model->bookmark_bar_node();
  const bookmarks::BookmarkNode* folder1 = bookmark_model->AddFolder(
      /*parent=*/bookmark_bar_node, /*index=*/0,
      base::UTF8ToUTF16(kFolder1Title));
  const bookmarks::BookmarkNode* folder1_url1_node = bookmark_model->AddURL(
      /*parent=*/folder1, /*index=*/0, base::UTF8ToUTF16(kUrl1Title),
      GURL(kUrl1));
  bookmark_model->AddURL(
      /*parent=*/folder1, /*index=*/1, base::UTF8ToUTF16(kUrl2Title),
      GURL(kUrl2));

  // The remote model contains two folders. The first one is the same as in
  // local model, but it does not contain any urls. The second one has the url1
  // from first folder with same GUID. This will cause skip local creation for
  // |url1| while processing |folder1|.
  //
  // -------- The remote model --------
  // bookmark_bar
  //  |- folder 1
  //  |- folder 2
  //    |- url1(http://www.url1.com)

  const std::string suffix = syncer::UniquePosition::RandomSuffix();
  syncer::UniquePosition posFolder1 =
      syncer::UniquePosition::InitialPosition(suffix);
  syncer::UniquePosition posFolder2 =
      syncer::UniquePosition::After(posFolder1, suffix);

  syncer::UniquePosition posUrl1 =
      syncer::UniquePosition::InitialPosition(suffix);

  syncer::UpdateResponseDataList updates;
  updates.push_back(CreateBookmarkBarNodeUpdateData());
  updates.push_back(CreateUpdateResponseData(
      /*server_id=*/kFolder1Id, /*parent_id=*/kBookmarkBarId, kFolder1Title,
      /*url=*/std::string(),
      /*is_folder=*/true, /*unique_position=*/posFolder1));
  updates.push_back(CreateUpdateResponseData(
      /*server_id=*/kFolder2Id, /*parent_id=*/kBookmarkBarId, kFolder2Title,
      /*url=*/std::string(),
      /*is_folder=*/true, /*unique_position=*/posFolder2));
  updates.push_back(CreateUpdateResponseData(
      /*server_id=*/kUrl1Id, /*parent_id=*/kFolder2Id, kUrl1Title, kUrl1,
      /*is_folder=*/false, /*unique_position=*/posUrl1,
      folder1_url1_node->guid()));

  // -------- The expected merge outcome --------
  // bookmark_bar
  //  |- folder 1
  //    |- url2(http://www.url2.com)
  //  |- folder 2
  //    |- url1(http://www.url1.com)

  std::unique_ptr<SyncedBookmarkTracker> tracker =
      Merge(std::move(updates), bookmark_model.get());
  ASSERT_THAT(bookmark_bar_node->children().size(), Eq(2u));

  // Verify Folder 1.
  EXPECT_THAT(bookmark_bar_node->children()[0]->GetTitle(),
              Eq(base::ASCIIToUTF16(kFolder1Title)));
  ASSERT_THAT(bookmark_bar_node->children()[0]->children().size(), Eq(1u));

  EXPECT_THAT(bookmark_bar_node->children()[0]->children()[0]->GetTitle(),
              Eq(base::ASCIIToUTF16(kUrl2Title)));
  EXPECT_THAT(bookmark_bar_node->children()[0]->children()[0]->url(),
              Eq(GURL(kUrl2)));

  // Verify Folder 2.
  EXPECT_THAT(bookmark_bar_node->children()[1]->GetTitle(),
              Eq(base::ASCIIToUTF16(kFolder2Title)));
  ASSERT_THAT(bookmark_bar_node->children()[1]->children().size(), Eq(1u));

  EXPECT_THAT(bookmark_bar_node->children()[1]->children()[0]->GetTitle(),
              Eq(base::ASCIIToUTF16(kUrl1Title)));
  EXPECT_THAT(bookmark_bar_node->children()[1]->children()[0]->url(),
              Eq(GURL(kUrl1)));

  // Verify the tracker contents.
  EXPECT_THAT(tracker->TrackedEntitiesCountForTest(), Eq(5U));

  const size_t kMaxEntries = 1000;
  std::vector<const SyncedBookmarkTracker::Entity*> local_changes =
      tracker->GetEntitiesWithLocalChanges(kMaxEntries);

  ASSERT_THAT(local_changes.size(), Eq(1U));
  EXPECT_THAT(local_changes[0]->bookmark_node(),
              Eq(bookmark_bar_node->children()[0]->children()[0].get()));

  // Verify positions in tracker.
  EXPECT_TRUE(PositionsInTrackerMatchModel(bookmark_bar_node, *tracker));
}

TEST(BookmarkModelMergerTest, ShouldLogMetricsForInvalidSpecifics) {
  std::unique_ptr<bookmarks::BookmarkModel> bookmark_model =
      bookmarks::TestBookmarkClient::CreateModel();

  // -------- The remote model --------
  // bookmark_bar
  //  | - bookmark (<invalid url>)

  syncer::UpdateResponseDataList updates;
  updates.push_back(CreateBookmarkBarNodeUpdateData());
  updates.push_back(CreateUpdateResponseData(
      /*server_id=*/"Id", /*parent_id=*/kBookmarkBarId, "Title",
      /*url=*/"invalidurl",
      /*is_folder=*/false,
      /*unique_position=*/MakeRandomPosition(),
      /*guid=*/base::GenerateGUID()));

  base::HistogramTester histogram_tester;
  Merge(std::move(updates), bookmark_model.get());
  histogram_tester.ExpectUniqueSample(
      "Sync.ProblematicServerSideBookmarksDuringMerge",
      /*sample=*/ExpectedRemoteBookmarkUpdateError::kInvalidSpecifics,
      /*count=*/1);
}

TEST(BookmarkModelMergerTest, ShouldLogMetricsForChildrenOfNonFolder) {
  std::unique_ptr<bookmarks::BookmarkModel> bookmark_model =
      bookmarks::TestBookmarkClient::CreateModel();

  // -------- The remote model --------
  // bookmark_bar
  //  | - bookmark (url1/Title1)
  //    | - bookmark (url2/Title2)
  //    | - bookmark (url3/Title3)
  //    | - bookmark (url4/Title4)

  syncer::UpdateResponseDataList updates;
  updates.push_back(CreateBookmarkBarNodeUpdateData());
  updates.push_back(CreateUpdateResponseData(
      /*server_id=*/"Id1", /*parent_id=*/kBookmarkBarId, "Title1",
      /*url=*/"http://url1",
      /*is_folder=*/false,
      /*unique_position=*/MakeRandomPosition(),
      /*guid=*/base::GenerateGUID()));
  updates.push_back(CreateUpdateResponseData(
      /*server_id=*/"Id2", /*parent_id=*/"Id1", "Title2",
      /*url=*/"http://url2",
      /*is_folder=*/false,
      /*unique_position=*/MakeRandomPosition(),
      /*guid=*/base::GenerateGUID()));
  updates.push_back(CreateUpdateResponseData(
      /*server_id=*/"Id3", /*parent_id=*/"Id1", "Title3",
      /*url=*/"http://url3",
      /*is_folder=*/false,
      /*unique_position=*/MakeRandomPosition(),
      /*guid=*/base::GenerateGUID()));
  updates.push_back(CreateUpdateResponseData(
      /*server_id=*/"Id4", /*parent_id=*/"Id1", "Title4",
      /*url=*/"http://url4",
      /*is_folder=*/false,
      /*unique_position=*/MakeRandomPosition(),
      /*guid=*/base::GenerateGUID()));

  base::HistogramTester histogram_tester;
  Merge(std::move(updates), bookmark_model.get());
  histogram_tester.ExpectUniqueSample(
      "Sync.ProblematicServerSideBookmarksDuringMerge",
      /*sample=*/ExpectedRemoteBookmarkUpdateError::kParentNotFolder,
      /*count=*/3);
}

TEST(BookmarkModelMergerTest, ShouldLogMetricsForChildrenOfOrphanUpdates) {
  std::unique_ptr<bookmarks::BookmarkModel> bookmark_model =
      bookmarks::TestBookmarkClient::CreateModel();

  // -------- The remote model --------
  // bookmark_bar
  // Orphan node: bookmark(url1/title1)

  syncer::UpdateResponseDataList updates;
  updates.push_back(CreateBookmarkBarNodeUpdateData());
  updates.push_back(CreateUpdateResponseData(
      /*server_id=*/"Id", /*parent_id=*/"UnknownId", "Title1",
      /*url=*/"http://url1",
      /*is_folder=*/false,
      /*unique_position=*/MakeRandomPosition(),
      /*guid=*/base::GenerateGUID()));

  base::HistogramTester histogram_tester;
  Merge(std::move(updates), bookmark_model.get());
  histogram_tester.ExpectUniqueSample(
      "Sync.ProblematicServerSideBookmarksDuringMerge",
      /*sample=*/ExpectedRemoteBookmarkUpdateError::kMissingParentEntity,
      /*count=*/1);
}

TEST(BookmarkModelMergerTest, ShouldRemoveMatchingDuplicatesByGUID) {
  const std::string kTitle1 = "Title 1";
  const std::string kTitle2 = "Title 2";
  const std::string kUrl = "http://www.url.com/";

  const std::string kUrlGUID = base::GenerateGUID();

  // The remote model has 2 duplicate folders with the same title and 2
  // duplicate bookmarks with the same URL.
  //
  // -------- The remote model --------
  // bookmark_bar
  //  |- url1(http://www.url.com, UrlGUID)
  //  |- url2(http://www.url.com, UrlGUID)
  std::unique_ptr<bookmarks::BookmarkModel> bookmark_model =
      bookmarks::TestBookmarkClient::CreateModel();

  syncer::UpdateResponseDataList updates;
  updates.push_back(CreateBookmarkBarNodeUpdateData());

  updates.push_back(CreateUpdateResponseData(
      /*server_id=*/"Id1", /*parent_id=*/kBookmarkBarId, kTitle1,
      /*url=*/kUrl,
      /*is_folder=*/false, /*unique_position=*/MakeRandomPosition(), kUrlGUID));
  updates.back().entity.creation_time =
      base::Time::Now() - base::TimeDelta::FromDays(1);
  updates.push_back(CreateUpdateResponseData(
      /*server_id=*/"Id2", /*parent_id=*/kBookmarkBarId, kTitle2,
      /*url=*/kUrl,
      /*is_folder=*/false, /*unique_position=*/MakeRandomPosition(), kUrlGUID));
  updates.back().entity.creation_time = base::Time::Now();

  base::HistogramTester histogram_tester;
  std::unique_ptr<SyncedBookmarkTracker> tracker =
      Merge(std::move(updates), bookmark_model.get());
  const bookmarks::BookmarkNode* bookmark_bar_node =
      bookmark_model->bookmark_bar_node();
  ASSERT_THAT(bookmark_bar_node->children().size(), Eq(1u));
  histogram_tester.ExpectBucketCount(
      "Sync.BookmarksGUIDDuplicates",
      /*sample=*/ExpectedBookmarksGUIDDuplicates::kMatchingUrls, /*count=*/1);
  EXPECT_EQ(bookmark_bar_node->children().front()->GetTitle(),
            base::UTF8ToUTF16(kTitle2));
}

TEST(BookmarkModelMergerTest, ShouldRemoveDifferentDuplicatesByGUID) {
  const std::string kTitle1 = "Title 1";
  const std::string kTitle2 = "Title 2";
  const std::string kUrl = "http://www.url.com/";
  const std::string kDifferentUrl = "http://www.different-url.com/";

  const std::string kUrlGUID = base::GenerateGUID();

  // The remote model will have 2 duplicate folders with
  // different titles and 2 duplicate bookmarks with different URLs
  //
  // -------- The remote model --------
  // bookmark_bar
  //  |- url1(http://www.url.com, UrlGUID)
  //  |- url2(http://www.different-url.com, UrlGUID)
  std::unique_ptr<bookmarks::BookmarkModel> bookmark_model =
      bookmarks::TestBookmarkClient::CreateModel();

  syncer::UpdateResponseDataList updates;
  updates.push_back(CreateBookmarkBarNodeUpdateData());

  updates.push_back(CreateUpdateResponseData(
      /*server_id=*/"Id1", /*parent_id=*/kBookmarkBarId, kTitle1,
      /*url=*/kUrl,
      /*is_folder=*/false, /*unique_position=*/MakeRandomPosition(), kUrlGUID));
  updates.back().entity.creation_time = base::Time::Now();
  updates.push_back(CreateUpdateResponseData(
      /*server_id=*/"Id2", /*parent_id=*/kBookmarkBarId, kTitle2,
      /*url=*/kDifferentUrl,
      /*is_folder=*/false, /*unique_position=*/MakeRandomPosition(), kUrlGUID));
  updates.back().entity.creation_time =
      base::Time::Now() - base::TimeDelta::FromDays(1);

  base::HistogramTester histogram_tester;
  std::unique_ptr<SyncedBookmarkTracker> tracker =
      Merge(std::move(updates), bookmark_model.get());
  const bookmarks::BookmarkNode* bookmark_bar_node =
      bookmark_model->bookmark_bar_node();
  ASSERT_THAT(bookmark_bar_node->children().size(), Eq(1u));
  histogram_tester.ExpectBucketCount(
      "Sync.BookmarksGUIDDuplicates",
      /*sample=*/ExpectedBookmarksGUIDDuplicates::kDifferentUrls, /*count=*/1);
  EXPECT_EQ(bookmark_bar_node->children().front()->GetTitle(),
            base::UTF8ToUTF16(kTitle1));
}

TEST(BookmarkModelMergerTest, ShouldRemoveMatchingFolderDuplicatesByGUID) {
  const std::string kTitle = "Title";

  const std::string kGUID = base::GenerateGUID();

  // The remote model has 2 duplicate folders with the same title and 2
  // duplicate bookmarks with the same URL.
  //
  // -------- The remote model --------
  // bookmark_bar
  //  |- folder1(Title, GUID)
  //  |- folder2(Title, GUID)
  std::unique_ptr<bookmarks::BookmarkModel> bookmark_model =
      bookmarks::TestBookmarkClient::CreateModel();

  syncer::UpdateResponseDataList updates;
  updates.push_back(CreateBookmarkBarNodeUpdateData());

  updates.push_back(CreateUpdateResponseData(
      /*server_id=*/"Id1", /*parent_id=*/kBookmarkBarId, kTitle,
      /*url=*/"",
      /*is_folder=*/true, /*unique_position=*/MakeRandomPosition(), kGUID));
  updates.back().entity.creation_time =
      base::Time::Now() - base::TimeDelta::FromDays(1);
  updates.push_back(CreateUpdateResponseData(
      /*server_id=*/"Id2", /*parent_id=*/kBookmarkBarId, kTitle,
      /*url=*/"",
      /*is_folder=*/true, /*unique_position=*/MakeRandomPosition(), kGUID));
  updates.back().entity.creation_time = base::Time::Now();

  base::HistogramTester histogram_tester;
  std::unique_ptr<SyncedBookmarkTracker> tracker =
      Merge(std::move(updates), bookmark_model.get());
  const bookmarks::BookmarkNode* bookmark_bar_node =
      bookmark_model->bookmark_bar_node();
  ASSERT_THAT(bookmark_bar_node->children().size(), Eq(1u));
  histogram_tester.ExpectBucketCount(
      "Sync.BookmarksGUIDDuplicates",
      /*sample=*/ExpectedBookmarksGUIDDuplicates::kMatchingFolders,
      /*count=*/1);
  EXPECT_THAT(tracker->GetEntityForSyncId("Id1"), IsNull());
  EXPECT_THAT(tracker->GetEntityForSyncId("Id2"), NotNull());
}

TEST(BookmarkModelMergerTest, ShouldRemoveDifferentFolderDuplicatesByGUID) {
  const std::string kTitle1 = "Title 1";
  const std::string kTitle2 = "Title 2";

  const std::string kGUID = base::GenerateGUID();

  // The remote model has 2 duplicate folders with the same title and 2
  // duplicate bookmarks with the same URL.
  //
  // -------- The remote model --------
  // bookmark_bar
  //  |- folder1(Title, GUID)
  //    |- folder11
  //  |- folder2(Title, GUID)
  //    |- folder21
  std::unique_ptr<bookmarks::BookmarkModel> bookmark_model =
      bookmarks::TestBookmarkClient::CreateModel();

  syncer::UpdateResponseDataList updates;
  updates.push_back(CreateBookmarkBarNodeUpdateData());

  updates.push_back(CreateUpdateResponseData(
      /*server_id=*/"Id1", /*parent_id=*/kBookmarkBarId, kTitle1,
      /*url=*/"",
      /*is_folder=*/true, MakeRandomPosition(), kGUID));
  updates.back().entity.creation_time = base::Time::Now();
  updates.push_back(CreateUpdateResponseData(
      /*server_id=*/"Id11", /*parent_id=*/"Id1", "Some title",
      /*url=*/"", /*is_folder=*/true, MakeRandomPosition()));

  updates.push_back(CreateUpdateResponseData(
      /*server_id=*/"Id2", /*parent_id=*/kBookmarkBarId, kTitle2,
      /*url=*/"", /*is_folder=*/true, MakeRandomPosition(), kGUID));
  updates.back().entity.creation_time =
      base::Time::Now() - base::TimeDelta::FromDays(1);
  updates.push_back(CreateUpdateResponseData(
      /*server_id=*/"Id21", /*parent_id=*/"Id2", "Some title 2",
      /*url=*/"", /*is_folder=*/true, MakeRandomPosition()));

  base::HistogramTester histogram_tester;
  std::unique_ptr<SyncedBookmarkTracker> tracker =
      Merge(std::move(updates), bookmark_model.get());
  const bookmarks::BookmarkNode* bookmark_bar_node =
      bookmark_model->bookmark_bar_node();
  ASSERT_THAT(bookmark_bar_node->children().size(), Eq(1u));
  histogram_tester.ExpectBucketCount(
      "Sync.BookmarksGUIDDuplicates",
      /*sample=*/ExpectedBookmarksGUIDDuplicates::kDifferentFolders,
      /*count=*/1);
  EXPECT_THAT(tracker->GetEntityForSyncId("Id1"), NotNull());
  EXPECT_THAT(tracker->GetEntityForSyncId("Id2"), IsNull());
  EXPECT_EQ(bookmark_bar_node->children().front()->GetTitle(),
            base::UTF8ToUTF16(kTitle1));
  EXPECT_EQ(bookmark_bar_node->children().front()->children().size(), 2u);
}

// This tests ensures maximum depth of the bookmark tree is not exceeded. This
// prevents a stack overflow.
TEST(BookmarkModelMergerTest, ShouldEnsureLimitDepthOfTree) {
  const std::string kLocalTitle = "local";
  const std::string kRemoteTitle = "remote";
  const std::string folderIdPrefix = "folder_";
  // Maximum depth to sync bookmarks tree to protect against stack overflow.
  // This matches |kMaxBookmarkTreeDepth| in bookmark_model_merger.cc.
  const size_t kMaxBookmarkTreeDepth = 200;
  const size_t kRemoteUpdatesDepth = kMaxBookmarkTreeDepth + 10;

  std::unique_ptr<bookmarks::BookmarkModel> bookmark_model =
      bookmarks::TestBookmarkClient::CreateModel();

  // -------- The local model --------
  const bookmarks::BookmarkNode* bookmark_bar_node =
      bookmark_model->bookmark_bar_node();
  const bookmarks::BookmarkNode* folder = bookmark_model->AddFolder(
      /*parent=*/bookmark_bar_node, /*index=*/0,
      base::UTF8ToUTF16(kLocalTitle));
  ASSERT_TRUE(folder);

  // -------- The remote model --------
  syncer::UpdateResponseDataList updates;
  updates.push_back(CreateBookmarkBarNodeUpdateData());

  std::string parent_id = kBookmarkBarId;
  // Create a tree with depth |kRemoteUpdatesDepth| to verify the limit of
  // kMaxBookmarkTreeDepth is enforced.
  for (size_t i = 1; i < kRemoteUpdatesDepth; ++i) {
    std::string folder_id = folderIdPrefix + base::NumberToString(i);
    updates.push_back(CreateUpdateResponseData(
        /*server_id=*/folder_id, /*parent_id=*/parent_id, kRemoteTitle,
        /*url=*/"",
        /*is_folder=*/true, MakeRandomPosition()));
    parent_id = folder_id;
  }

  ASSERT_THAT(updates.size(), Eq(kRemoteUpdatesDepth));

  std::unique_ptr<SyncedBookmarkTracker> tracker =
      SyncedBookmarkTracker::CreateEmpty(sync_pb::ModelTypeState());
  testing::NiceMock<favicon::MockFaviconService> favicon_service;
  BookmarkModelMerger(std::move(updates), bookmark_model.get(),
                      &favicon_service, tracker.get())
      .Merge();

  // Check max depth hasn't been exceeded. Take into account root of the
  // tracker and bookmark bar.
  EXPECT_THAT(tracker->TrackedEntitiesCountForTest(),
              Eq(kMaxBookmarkTreeDepth + 2));
}

TEST(BookmarkModelMergerTest, ShouldReuploadBookmarkOnEmptyGuid) {
  base::test::ScopedFeatureList override_features;
  override_features.InitAndEnableFeature(
      switches::kSyncReuploadBookmarkFullTitles);

  const std::string kFolder1Title = "folder1";
  const std::string kFolder2Title = "folder2";

  const std::string kFolder1Id = "Folder1Id";
  const std::string kFolder2Id = "Folder2Id";

  const std::string suffix = syncer::UniquePosition::RandomSuffix();
  const syncer::UniquePosition posFolder1 =
      syncer::UniquePosition::InitialPosition(suffix);
  const syncer::UniquePosition posFolder2 =
      syncer::UniquePosition::After(posFolder1, suffix);

  std::unique_ptr<bookmarks::BookmarkModel> bookmark_model =
      bookmarks::TestBookmarkClient::CreateModel();

  // -------- The remote model --------
  syncer::UpdateResponseDataList updates;
  updates.push_back(CreateBookmarkBarNodeUpdateData());
  updates.push_back(CreateUpdateResponseData(
      /*server_id=*/kFolder1Id, /*parent_id=*/kBookmarkBarId, kFolder1Title,
      /*url=*/std::string(),
      /*is_folder=*/true, /*unique_position=*/posFolder1,
      base::GenerateGUID()));

  // Mimic that the entity didn't have GUID in specifics. This entity should be
  // reuploaded later.
  updates.back().entity.is_bookmark_guid_in_specifics_preprocessed = true;

  updates.push_back(CreateUpdateResponseData(
      /*server_id=*/kFolder2Id, /*parent_id=*/kBookmarkBarId, kFolder2Title,
      /*url=*/std::string(),
      /*is_folder=*/true, /*unique_position=*/posFolder2,
      base::GenerateGUID()));

  std::unique_ptr<SyncedBookmarkTracker> tracker =
      Merge(std::move(updates), bookmark_model.get());

  ASSERT_THAT(tracker->GetEntityForSyncId(kFolder1Id), NotNull());
  ASSERT_THAT(tracker->GetEntityForSyncId(kFolder2Id), NotNull());

  EXPECT_TRUE(tracker->GetEntityForSyncId(kFolder1Id)->IsUnsynced());
  EXPECT_FALSE(tracker->GetEntityForSyncId(kFolder2Id)->IsUnsynced());
}

TEST(BookmarkModelMergerTest, ShouldRemoveDifferentTypeDuplicatesByGUID) {
  const std::string kTitle = "Title";

  const std::string kGUID = base::GenerateGUID();

  // The remote model has 2 duplicates, a folder and a URL.
  //
  // -------- The remote model --------
  // bookmark_bar
  //  |- folder1(GUID)
  //    |- folder11
  //  |- URL1(GUID)
  std::unique_ptr<bookmarks::BookmarkModel> bookmark_model =
      bookmarks::TestBookmarkClient::CreateModel();

  syncer::UpdateResponseDataList updates;
  updates.push_back(CreateBookmarkBarNodeUpdateData());

  updates.push_back(CreateUpdateResponseData(
      /*server_id=*/"Id1", /*parent_id=*/kBookmarkBarId, kTitle,
      /*url=*/"",
      /*is_folder=*/true, MakeRandomPosition(), kGUID));
  updates.push_back(CreateUpdateResponseData(
      /*server_id=*/"Id11", /*parent_id=*/"Id1", "Some title",
      /*url=*/"", /*is_folder=*/true, MakeRandomPosition()));

  updates.push_back(CreateUpdateResponseData(
      /*server_id=*/"Id2", /*parent_id=*/kBookmarkBarId, kTitle,
      /*url=*/"http://url1.com", /*is_folder=*/false, MakeRandomPosition(),
      kGUID));

  base::HistogramTester histogram_tester;
  std::unique_ptr<SyncedBookmarkTracker> tracker =
      Merge(std::move(updates), bookmark_model.get());
  const bookmarks::BookmarkNode* bookmark_bar_node =
      bookmark_model->bookmark_bar_node();
  ASSERT_THAT(bookmark_bar_node->children().size(), Eq(1u));
  histogram_tester.ExpectUniqueSample(
      "Sync.BookmarksGUIDDuplicates",
      /*sample=*/ExpectedBookmarksGUIDDuplicates::kDifferentTypes,
      /*count=*/1);
  EXPECT_THAT(tracker->GetEntityForSyncId("Id1"), NotNull());
  EXPECT_THAT(tracker->GetEntityForSyncId("Id2"), IsNull());
  EXPECT_EQ(bookmark_bar_node->children().front()->children().size(), 1u);
}

}  // namespace sync_bookmarks
