// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_bookmarks/bookmark_model_merger.h"

#include <memory>
#include <string>
#include <vector>

#include "base/strings/utf_string_conversions.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/test/test_bookmark_client.h"
#include "components/favicon/core/test/mock_favicon_service.h"
#include "components/sync/base/unique_position.h"
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

syncer::UpdateResponseData CreateUpdateResponseData(
    const std::string& server_id,
    const std::string& parent_id,
    const std::string& title,
    const std::string& url,
    bool is_folder,
    const syncer::UniquePosition& unique_position,
    const std::string& icon_url = std::string(),
    const std::string& icon_data = std::string()) {
  syncer::EntityData data;
  data.id = server_id;
  data.parent_id = parent_id;
  data.unique_position = unique_position.ToProto();

  sync_pb::BookmarkSpecifics* bookmark_specifics =
      data.specifics.mutable_bookmark();
  bookmark_specifics->set_title(title);
  bookmark_specifics->set_url(url);
  bookmark_specifics->set_icon_url(icon_url);
  bookmark_specifics->set_favicon(icon_data);

  data.is_folder = is_folder;
  syncer::UpdateResponseData response_data;
  response_data.entity = data.PassToPtr();
  // Similar to what's done in the loopback_server.
  response_data.response_version = 0;
  return response_data;
}

syncer::UpdateResponseData CreateBookmarkBarNodeUpdateData() {
  syncer::EntityData data;
  data.id = kBookmarkBarId;
  data.server_defined_unique_tag = kBookmarkBarTag;

  data.specifics.mutable_bookmark();

  syncer::UpdateResponseData response_data;
  response_data.entity = data.PassToPtr();
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
  if (node->child_count() == 0) {
    return true;
  }
  syncer::UniquePosition pos = PositionOf(node->GetChild(0), tracker);
  for (int i = 1; i < node->child_count(); ++i) {
    if (PositionOf(node->GetChild(i), tracker).LessThan(pos)) {
      DLOG(ERROR) << "Position of " << node->GetChild(i)->GetTitle()
                  << " is less than position of "
                  << node->GetChild(i - 1)->GetTitle();
      return false;
    }
    pos = PositionOf(node->GetChild(i), tracker);
  }
  for (int i = 0; i < node->child_count(); ++i) {
    if (!PositionsInTrackerMatchModel(node->GetChild(i), tracker)) {
      return false;
    }
  }
  return true;
}

}  // namespace

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
  BookmarkModelMerger(&updates, bookmark_model.get(), &favicon_service,
                      &tracker)
      .Merge();
  ASSERT_THAT(bookmark_bar_node->child_count(), Eq(3));

  // Verify Folder 1.
  EXPECT_THAT(bookmark_bar_node->GetChild(0)->GetTitle(),
              Eq(base::ASCIIToUTF16(kFolder1Title)));
  ASSERT_THAT(bookmark_bar_node->GetChild(0)->child_count(), Eq(3));

  EXPECT_THAT(bookmark_bar_node->GetChild(0)->GetChild(0)->GetTitle(),
              Eq(base::ASCIIToUTF16(kUrl1Title)));
  EXPECT_THAT(bookmark_bar_node->GetChild(0)->GetChild(0)->url(),
              Eq(GURL(kUrl1)));

  EXPECT_THAT(bookmark_bar_node->GetChild(0)->GetChild(1)->GetTitle(),
              Eq(base::ASCIIToUTF16(kUrl2Title)));
  EXPECT_THAT(bookmark_bar_node->GetChild(0)->GetChild(1)->url(),
              Eq(GURL(kAnotherUrl2)));

  EXPECT_THAT(bookmark_bar_node->GetChild(0)->GetChild(2)->GetTitle(),
              Eq(base::ASCIIToUTF16(kUrl2Title)));
  EXPECT_THAT(bookmark_bar_node->GetChild(0)->GetChild(2)->url(),
              Eq(GURL(kUrl2)));

  // Verify Folder 3.
  EXPECT_THAT(bookmark_bar_node->GetChild(1)->GetTitle(),
              Eq(base::ASCIIToUTF16(kFolder3Title)));
  ASSERT_THAT(bookmark_bar_node->GetChild(1)->child_count(), Eq(2));

  EXPECT_THAT(bookmark_bar_node->GetChild(1)->GetChild(0)->GetTitle(),
              Eq(base::ASCIIToUTF16(kUrl3Title)));
  EXPECT_THAT(bookmark_bar_node->GetChild(1)->GetChild(0)->url(),
              Eq(GURL(kUrl3)));
  EXPECT_THAT(bookmark_bar_node->GetChild(1)->GetChild(1)->GetTitle(),
              Eq(base::ASCIIToUTF16(kUrl4Title)));
  EXPECT_THAT(bookmark_bar_node->GetChild(1)->GetChild(1)->url(),
              Eq(GURL(kUrl4)));

  // Verify Folder 2.
  EXPECT_THAT(bookmark_bar_node->GetChild(2)->GetTitle(),
              Eq(base::ASCIIToUTF16(kFolder2Title)));
  ASSERT_THAT(bookmark_bar_node->GetChild(2)->child_count(), Eq(2));

  EXPECT_THAT(bookmark_bar_node->GetChild(2)->GetChild(0)->GetTitle(),
              Eq(base::ASCIIToUTF16(kUrl3Title)));
  EXPECT_THAT(bookmark_bar_node->GetChild(2)->GetChild(0)->url(),
              Eq(GURL(kUrl3)));
  EXPECT_THAT(bookmark_bar_node->GetChild(2)->GetChild(1)->GetTitle(),
              Eq(base::ASCIIToUTF16(kUrl4Title)));
  EXPECT_THAT(bookmark_bar_node->GetChild(2)->GetChild(1)->url(),
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
  EXPECT_THAT(
      nodes_with_local_changes,
      UnorderedElementsAre(bookmark_bar_node->GetChild(0)->GetChild(2),
                           bookmark_bar_node->GetChild(2),
                           bookmark_bar_node->GetChild(2)->GetChild(0),
                           bookmark_bar_node->GetChild(2)->GetChild(1)));

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
  BookmarkModelMerger(&updates, bookmark_model.get(), &favicon_service,
                      &tracker)
      .Merge();
  ASSERT_THAT(bookmark_bar_node->child_count(), Eq(3));

  EXPECT_THAT(bookmark_bar_node->GetChild(0)->GetTitle(),
              Eq(base::ASCIIToUTF16(kFolder1Title)));
  EXPECT_THAT(bookmark_bar_node->GetChild(1)->GetTitle(),
              Eq(base::ASCIIToUTF16(kFolder3Title)));
  EXPECT_THAT(bookmark_bar_node->GetChild(2)->GetTitle(),
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
      /*is_folder=*/false, /*unique_position=*/pos2, kIcon2Url.spec(),
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

  BookmarkModelMerger(&updates, bookmark_model.get(), &favicon_service,
                      &tracker)
      .Merge();
}

}  // namespace sync_bookmarks
