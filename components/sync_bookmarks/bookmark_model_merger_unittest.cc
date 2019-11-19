// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_bookmarks/bookmark_model_merger.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/guid.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/test/test_bookmark_client.h"
#include "components/favicon/core/test/mock_favicon_service.h"
#include "components/sync/base/unique_position.h"
#include "components/sync/driver/sync_driver_switches.h"
#include "components/sync_bookmarks/synced_bookmark_tracker.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::Eq;
using testing::UnorderedElementsAre;

namespace sync_bookmarks {

namespace {

const char kBookmarkBarId[] = "bookmark_bar_id";
const char kBookmarkBarTag[] = "bookmark_bar";

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

std::unique_ptr<syncer::UpdateResponseData> CreateUpdateResponseData(
    const std::string& server_id,
    const std::string& parent_id,
    const std::string& title,
    const std::string& url,
    bool is_folder,
    const syncer::UniquePosition& unique_position,
    base::Optional<std::string> guid = base::nullopt,
    const std::string& icon_url = std::string(),
    const std::string& icon_data = std::string()) {
  if (!guid)
    guid = base::GenerateGUID();

  auto data = std::make_unique<syncer::EntityData>();
  data->id = server_id;
  data->parent_id = parent_id;
  data->unique_position = unique_position.ToProto();

  sync_pb::BookmarkSpecifics* bookmark_specifics =
      data->specifics.mutable_bookmark();
  bookmark_specifics->set_guid(*guid);
  bookmark_specifics->set_title(title);
  bookmark_specifics->set_url(url);
  bookmark_specifics->set_icon_url(icon_url);
  bookmark_specifics->set_favicon(icon_data);

  data->is_folder = is_folder;
  auto response_data = std::make_unique<syncer::UpdateResponseData>();
  response_data->entity = std::move(data);
  // Similar to what's done in the loopback_server.
  response_data->response_version = 0;
  return response_data;
}

std::unique_ptr<syncer::UpdateResponseData> CreateBookmarkBarNodeUpdateData() {
  auto data = std::make_unique<syncer::EntityData>();
  data->id = kBookmarkBarId;
  data->server_defined_unique_tag = kBookmarkBarTag;

  data->specifics.mutable_bookmark();

  auto response_data = std::make_unique<syncer::UpdateResponseData>();
  response_data->entity = std::move(data);
  // Similar to what's done in the loopback_server.
  response_data->response_version = 0;
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

void Merge(syncer::UpdateResponseDataList updates,
           bookmarks::BookmarkModel* bookmark_model) {
  SyncedBookmarkTracker tracker(std::vector<NodeMetadataPair>(),
                                std::make_unique<sync_pb::ModelTypeState>());
  testing::NiceMock<favicon::MockFaviconService> favicon_service;
  BookmarkModelMerger(std::move(updates), bookmark_model, &favicon_service,
                      &tracker)
      .Merge();
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

  SyncedBookmarkTracker tracker(std::vector<NodeMetadataPair>(),
                                std::make_unique<sync_pb::ModelTypeState>());
  testing::NiceMock<favicon::MockFaviconService> favicon_service;
  BookmarkModelMerger(std::move(updates), bookmark_model.get(),
                      &favicon_service, &tracker)
      .Merge();
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
  EXPECT_THAT(tracker.TrackedEntitiesCountForTest(), Eq(11U));
  std::vector<const SyncedBookmarkTracker::Entity*> local_changes =
      tracker.GetEntitiesWithLocalChanges(kMaxEntries);

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
  EXPECT_TRUE(PositionsInTrackerMatchModel(bookmark_bar_node, tracker));
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

  SyncedBookmarkTracker tracker(std::vector<NodeMetadataPair>(),
                                std::make_unique<sync_pb::ModelTypeState>());
  testing::NiceMock<favicon::MockFaviconService> favicon_service;
  BookmarkModelMerger(std::move(updates), bookmark_model.get(),
                      &favicon_service, &tracker)
      .Merge();
  ASSERT_THAT(bookmark_bar_node->children().size(), Eq(3u));

  EXPECT_THAT(bookmark_bar_node->children()[0]->GetTitle(),
              Eq(base::ASCIIToUTF16(kFolder1Title)));
  EXPECT_THAT(bookmark_bar_node->children()[1]->GetTitle(),
              Eq(base::ASCIIToUTF16(kFolder3Title)));
  EXPECT_THAT(bookmark_bar_node->children()[2]->GetTitle(),
              Eq(base::ASCIIToUTF16(kFolder2Title)));

  // Verify the tracker contents.
  EXPECT_THAT(tracker.TrackedEntitiesCountForTest(), Eq(4U));

  // There should be no local changes.
  std::vector<const SyncedBookmarkTracker::Entity*> local_changes =
      tracker.GetEntitiesWithLocalChanges(kMaxEntries);
  EXPECT_THAT(local_changes.size(), Eq(0U));

  // Verify positions in tracker.
  EXPECT_TRUE(PositionsInTrackerMatchModel(bookmark_bar_node, tracker));
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

  SyncedBookmarkTracker tracker(std::vector<NodeMetadataPair>(),
                                std::make_unique<sync_pb::ModelTypeState>());
  testing::NiceMock<favicon::MockFaviconService> favicon_service;

  // Favicon should be set for the remote node.
  EXPECT_CALL(favicon_service,
              AddPageNoVisitForBookmark(kUrl2, base::UTF8ToUTF16(kTitle2)));
  EXPECT_CALL(favicon_service, MergeFavicon(kUrl2, _, _, _, _));

  BookmarkModelMerger(std::move(updates), bookmark_model.get(),
                      &favicon_service, &tracker)
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
  updates.push_back(CreateUpdateResponseData(
      /*server_id=*/kId, /*parent_id=*/kBookmarkBarId, kRemoteTitle,
      /*url=*/std::string(),
      /*is_folder=*/true, /*unique_position=*/pos));

  SyncedBookmarkTracker tracker(std::vector<NodeMetadataPair>(),
                                std::make_unique<sync_pb::ModelTypeState>());
  testing::NiceMock<favicon::MockFaviconService> favicon_service;
  BookmarkModelMerger(std::move(updates), bookmark_model.get(),
                      &favicon_service, &tracker)
      .Merge();

  // Both titles should have matched against each other and only node is in the
  // model and the tracker.
  EXPECT_THAT(bookmark_bar_node->children().size(), Eq(1u));
  EXPECT_THAT(tracker.TrackedEntitiesCountForTest(), Eq(2U));
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

  SyncedBookmarkTracker tracker(std::vector<NodeMetadataPair>(),
                                std::make_unique<sync_pb::ModelTypeState>());
  testing::NiceMock<favicon::MockFaviconService> favicon_service;
  BookmarkModelMerger(std::move(updates), bookmark_model.get(),
                      &favicon_service, &tracker)
      .Merge();

  // Both titles should have matched against each other and only node is in the
  // model and the tracker.
  EXPECT_THAT(bookmark_bar_node->children().size(), Eq(1u));
  EXPECT_THAT(tracker.TrackedEntitiesCountForTest(), Eq(2U));
}

TEST(BookmarkModelMergerTest, ShouldMergeRemoteCreationWithoutGUID) {
  const std::string kId = "Id";
  const std::string kTitle = "Title";

  std::unique_ptr<bookmarks::BookmarkModel> bookmark_model =
      bookmarks::TestBookmarkClient::CreateModel();
  ASSERT_EQ(bookmark_model->bookmark_bar_node()->children().size(), 0u);

  // -------- The remote model --------
  syncer::UpdateResponseDataList updates;
  updates.push_back(CreateBookmarkBarNodeUpdateData());
  updates.push_back(CreateUpdateResponseData(
      /*server_id=*/kId, /*parent_id=*/kBookmarkBarId, kTitle,
      /*url=*/std::string(),
      /*is_folder=*/true, /*unique_position=*/MakeRandomPosition(),
      /*guid=*/""));

  Merge(std::move(updates), bookmark_model.get());

  // Node should have been created and new GUID should have been set.
  ASSERT_EQ(bookmark_model->bookmark_bar_node()->children().size(), 1u);
  EXPECT_TRUE(base::IsValidGUID(
      bookmark_model->bookmark_bar_node()->children()[0].get()->guid()));
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

  Merge(std::move(updates), bookmark_model.get());

  // Node should have been replaced and GUID should be set to that stored in the
  // specifics.
  ASSERT_EQ(bookmark_bar_node->children().size(), 1u);
  EXPECT_EQ(bookmark_bar_node->children()[0].get()->guid(), kRemoteGuid);
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

  Merge(std::move(updates), bookmark_model.get());

  // Node should not have been replaced and GUID should not have been set to
  // that stored in the specifics, as it was invalid.
  ASSERT_EQ(bookmark_bar_node->children().size(), 1u);
  EXPECT_EQ(bookmark_bar_node->children()[0].get()->guid(), old_guid);
}

TEST(BookmarkModelMergerTest, ShouldMergeBookmarkByGUID) {
  base::test::ScopedFeatureList override_features;
  override_features.InitAndEnableFeature(switches::kMergeBookmarksUsingGUIDs);

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

  Merge(std::move(updates), bookmark_model.get());

  // -------- The merged model --------
  // bookmark_bar
  //  |- bookmark(kGuid/kRemoteTitle)

  // Node should have been merged.
  EXPECT_THAT(bookmark_bar_node->children(), ElementRawPointersAre(bookmark));
  EXPECT_EQ(bookmark->GetTitle(), base::UTF8ToUTF16(kRemoteTitle));
}

TEST(BookmarkModelMergerTest, ShouldMergeBookmarkByGUIDAndReparent) {
  base::test::ScopedFeatureList override_features;
  override_features.InitAndEnableFeature(switches::kMergeBookmarksUsingGUIDs);

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
}

TEST(BookmarkModelMergerTest, ShouldMergeFolderByGUIDAndNotSemantics) {
  base::test::ScopedFeatureList override_features;
  override_features.InitAndEnableFeature(switches::kMergeBookmarksUsingGUIDs);

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
}

TEST(
    BookmarkModelMergerTest,
    ShouldIgnoreFolderSemanticsMatchAndLaterMatchByGUIDWithSemanticsNodeFirst) {
  base::test::ScopedFeatureList override_features;
  override_features.InitAndEnableFeature(switches::kMergeBookmarksUsingGUIDs);

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
}

TEST(BookmarkModelMergerTest,
     ShouldIgnoreFolderSemanticsMatchAndLaterMatchByGUIDWithGUIDNodeFirst) {
  base::test::ScopedFeatureList override_features;
  override_features.InitAndEnableFeature(switches::kMergeBookmarksUsingGUIDs);

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
  base::test::ScopedFeatureList override_features;
  override_features.InitAndEnableFeature(switches::kMergeBookmarksUsingGUIDs);

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
  EXPECT_TRUE(base::IsValidGUID(bookmark_bar_node->children()[1]->guid()));
  EXPECT_EQ(bookmark_bar_node->children()[1]->url(), kUrl1);
}

TEST(BookmarkModelMergerTest, ShouldReplaceBookmarkGUIDWithConflictingTypes) {
  base::test::ScopedFeatureList override_features;
  override_features.InitAndEnableFeature(switches::kMergeBookmarksUsingGUIDs);

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
  EXPECT_TRUE(base::IsValidGUID(bookmark_bar_node->children()[1]->guid()));
  EXPECT_FALSE(bookmark_bar_node->children()[1]->is_folder());
}

}  // namespace sync_bookmarks
