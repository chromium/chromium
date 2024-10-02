// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_bookmarks/bookmark_model_merger.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/ranges/algorithm.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/uuid.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "components/bookmarks/browser/bookmark_uuids.h"
#include "components/favicon/core/test/mock_favicon_service.h"
#include "components/sync/base/features.h"
#include "components/sync/base/unique_position.h"
#include "components/sync/protocol/entity_metadata.pb.h"
#include "components/sync_bookmarks/bookmark_model_view.h"
#include "components/sync_bookmarks/bookmark_specifics_conversions.h"
#include "components/sync_bookmarks/switches.h"
#include "components/sync_bookmarks/synced_bookmark_tracker.h"
#include "components/sync_bookmarks/synced_bookmark_tracker_entity.h"
#include "components/sync_bookmarks/test_bookmark_model_view.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::Eq;
using testing::IsEmpty;
using testing::IsNull;
using testing::NotNull;
using testing::UnorderedElementsAre;

namespace sync_bookmarks {

namespace {

MATCHER_P(HasTitle, title, "") {
  return arg->GetTitle() == title;
}

// Copy of BookmarksUuidDuplicates.
enum class ExpectedBookmarksUuidDuplicates {
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
  kUnexpectedUuid = 9,
  kParentNotFolder = 10,
  kUnsupportedPermanentFolder = 13,
  kDescendantOfRootNodeWithoutPermanentFolder = 14,
  kMaxValue = kDescendantOfRootNodeWithoutPermanentFolder,
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

base::Uuid BookmarkBarUuid() {
  return base::Uuid::ParseLowercase(bookmarks::kBookmarkBarNodeUuid);
}

// Returns a sync ID mimic-ing what a real server could return, which means it
// generally opaque for the client but deterministic given |uuid|, because the
// sync ID is roughly a hashed UUID, at least in normal circumnstances where the
// UUID is used either as client tag hash or as originator client item ID.
std::string GetFakeServerIdFromUuid(const base::Uuid& uuid) {
  // For convenience in tests, |uuid| may refer to permanent nodes too,
  // and yet the returned sync ID will honor the sync ID constants for permanent
  // nodes.
  if (uuid.AsLowercaseString() == bookmarks::kBookmarkBarNodeUuid) {
    return kBookmarkBarId;
  }
  return base::StrCat({"server_id_for_", uuid.AsLowercaseString()});
}

class UpdateResponseDataBuilder {
 public:
  UpdateResponseDataBuilder(const base::Uuid& uuid,
                            const base::Uuid& parent_uuid,
                            const std::string& title,
                            const syncer::UniquePosition& unique_position) {
    data_.id = GetFakeServerIdFromUuid(uuid);
    data_.originator_client_item_id = uuid.AsLowercaseString();

    sync_pb::BookmarkSpecifics* bookmark_specifics =
        data_.specifics.mutable_bookmark();
    bookmark_specifics->set_legacy_canonicalized_title(title);
    bookmark_specifics->set_full_title(title);
    bookmark_specifics->set_type(sync_pb::BookmarkSpecifics::FOLDER);
    *bookmark_specifics->mutable_unique_position() = unique_position.ToProto();
    bookmark_specifics->set_guid(uuid.AsLowercaseString());
    bookmark_specifics->set_parent_guid(parent_uuid.AsLowercaseString());
  }

  UpdateResponseDataBuilder& SetUrl(const GURL& url) {
    data_.specifics.mutable_bookmark()->set_type(
        sync_pb::BookmarkSpecifics::URL);
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
    const base::Uuid& uuid,
    const base::Uuid& parent_uuid,
    const std::string& title,
    const std::string& url,
    bool is_folder,
    const syncer::UniquePosition& unique_position,
    const std::string& icon_url = std::string(),
    const std::string& icon_data = std::string()) {
  UpdateResponseDataBuilder builder(uuid, parent_uuid, title, unique_position);
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
  const SyncedBookmarkTrackerEntity* entity =
      tracker.GetEntityForBookmarkNode(node);
  return syncer::UniquePosition::FromProto(
      entity->metadata().unique_position());
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
  return base::ranges::all_of(node->children(), [&tracker](const auto& child) {
    return PositionsInTrackerMatchModel(child.get(), tracker);
  });
}

std::unique_ptr<SyncedBookmarkTracker> Merge(
    syncer::UpdateResponseDataList updates,
    BookmarkModelView* bookmark_model) {
  std::unique_ptr<SyncedBookmarkTracker> tracker =
      SyncedBookmarkTracker::CreateEmpty(sync_pb::DataTypeState());
  testing::NiceMock<favicon::MockFaviconService> favicon_service;
  BookmarkModelMerger(std::move(updates), bookmark_model, &favicon_service,
                      tracker.get())
      .Merge();
  return tracker;
}

static syncer::UniquePosition MakeRandomPosition() {
  return syncer::UniquePosition::InitialPosition(
      syncer::UniquePosition::RandomSuffix());
}

}  // namespace

TEST(BookmarkModelMergerTest, ShouldMergeLocalAndRemoteModels) {
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

  const base::Uuid kFolder1Uuid = base::Uuid::GenerateRandomV4();
  const base::Uuid kFolder3Uuid = base::Uuid::GenerateRandomV4();
  const base::Uuid kUrl1Uuid = base::Uuid::GenerateRandomV4();
  const base::Uuid kUrl2Uuid = base::Uuid::GenerateRandomV4();
  const base::Uuid kUrl3Uuid = base::Uuid::GenerateRandomV4();
  const base::Uuid kUrl4Uuid = base::Uuid::GenerateRandomV4();

  // -------- The local model --------
  // bookmark_bar
  //  |- folder 1
  //    |- url1(http://www.url1.com)
  //    |- url2(http://www.url2.com)
  //  |- folder 2
  //    |- url3(http://www.url3.com)
  //    |- url4(http://www.url4.com)

  TestBookmarkModelView bookmark_model;

  const bookmarks::BookmarkNode* bookmark_bar_node =
      bookmark_model.bookmark_bar_node();
  const bookmarks::BookmarkNode* folder1 = bookmark_model.AddFolder(
      /*parent=*/bookmark_bar_node, /*index=*/0,
      base::UTF8ToUTF16(kFolder1Title));

  const bookmarks::BookmarkNode* folder2 = bookmark_model.AddFolder(
      /*parent=*/bookmark_bar_node, /*index=*/1,
      base::UTF8ToUTF16(kFolder2Title));

  bookmark_model.AddURL(
      /*parent=*/folder1, /*index=*/0, base::UTF8ToUTF16(kUrl1Title),
      GURL(kUrl1));
  bookmark_model.AddURL(
      /*parent=*/folder1, /*index=*/1, base::UTF8ToUTF16(kUrl2Title),
      GURL(kUrl2));
  bookmark_model.AddURL(
      /*parent=*/folder2, /*index=*/0, base::UTF8ToUTF16(kUrl3Title),
      GURL(kUrl3));
  bookmark_model.AddURL(
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

  const syncer::UniquePosition::Suffix suffix =
      syncer::UniquePosition::RandomSuffix();
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
      /*uuid=*/kFolder1Uuid, /*parent_uuid=*/BookmarkBarUuid(), kFolder1Title,
      /*url=*/std::string(),
      /*is_folder=*/true, /*unique_position=*/posFolder1));
  updates.push_back(CreateUpdateResponseData(
      /*uuid=*/kUrl1Uuid, /*parent_uuid=*/kFolder1Uuid, kUrl1Title, kUrl1,
      /*is_folder=*/false, /*unique_position=*/posUrl1));
  updates.push_back(CreateUpdateResponseData(
      /*uuid=*/kUrl2Uuid, /*parent_uuid=*/kFolder1Uuid, kUrl2Title,
      kAnotherUrl2,
      /*is_folder=*/false, /*unique_position=*/posUrl2));
  updates.push_back(CreateUpdateResponseData(
      /*uuid=*/kFolder3Uuid, /*parent_uuid=*/BookmarkBarUuid(), kFolder3Title,
      /*url=*/std::string(),
      /*is_folder=*/true, /*unique_position=*/posFolder3));
  updates.push_back(CreateUpdateResponseData(
      /*uuid=*/kUrl3Uuid, /*parent_uuid=*/kFolder3Uuid, kUrl3Title, kUrl3,
      /*is_folder=*/false, /*unique_position=*/posUrl3));
  updates.push_back(CreateUpdateResponseData(
      /*uuid=*/kUrl4Uuid, /*parent_uuid=*/kFolder3Uuid, kUrl4Title, kUrl4,
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

  base::HistogramTester histogram_tester;

  std::unique_ptr<SyncedBookmarkTracker> tracker =
      Merge(std::move(updates), &bookmark_model);
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

  EXPECT_THAT(histogram_tester.GetTotalSum(
                  "Sync.BookmarkModelMerger.UnsyncedEntitiesUponCompletion"),
              Eq(4));

  // Verify the tracker contents.
  EXPECT_THAT(tracker->TrackedEntitiesCountForTest(), Eq(11U));
  std::vector<const SyncedBookmarkTrackerEntity*> local_changes =
      tracker->GetEntitiesWithLocalChanges();

  EXPECT_THAT(local_changes.size(), Eq(4U));
  std::vector<const bookmarks::BookmarkNode*> nodes_with_local_changes;
  for (const SyncedBookmarkTrackerEntity* local_change : local_changes) {
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
  const std::string kFolder1Title = "folder1";
  const std::string kFolder2Title = "folder2";
  const std::string kFolder3Title = "folder3";

  const base::Uuid kFolder1Uuid = base::Uuid::GenerateRandomV4();
  const base::Uuid kFolder2Uuid = base::Uuid::GenerateRandomV4();
  const base::Uuid kFolder3Uuid = base::Uuid::GenerateRandomV4();

  // -------- The local model --------
  // bookmark_bar
  //  |- folder 1
  //  |- folder 2
  //  |- folder 3

  TestBookmarkModelView bookmark_model;

  const bookmarks::BookmarkNode* bookmark_bar_node =
      bookmark_model.bookmark_bar_node();
  bookmark_model.AddFolder(
      /*parent=*/bookmark_bar_node, /*index=*/0,
      base::UTF8ToUTF16(kFolder1Title));

  bookmark_model.AddFolder(
      /*parent=*/bookmark_bar_node, /*index=*/1,
      base::UTF8ToUTF16(kFolder2Title));

  bookmark_model.AddFolder(
      /*parent=*/bookmark_bar_node, /*index=*/2,
      base::UTF8ToUTF16(kFolder3Title));

  // -------- The remote model --------
  // bookmark_bar
  //  |- folder 1
  //  |- folder 3
  //  |- folder 2

  const syncer::UniquePosition::Suffix suffix =
      syncer::UniquePosition::RandomSuffix();
  syncer::UniquePosition posFolder1 =
      syncer::UniquePosition::InitialPosition(suffix);
  syncer::UniquePosition posFolder3 =
      syncer::UniquePosition::After(posFolder1, suffix);
  syncer::UniquePosition posFolder2 =
      syncer::UniquePosition::After(posFolder3, suffix);

  syncer::UpdateResponseDataList updates;
  updates.push_back(CreateBookmarkBarNodeUpdateData());
  updates.push_back(CreateUpdateResponseData(
      /*uuid=*/kFolder1Uuid, /*parent_uuid=*/BookmarkBarUuid(), kFolder1Title,
      /*url=*/std::string(),
      /*is_folder=*/true, /*unique_position=*/posFolder1));
  updates.push_back(CreateUpdateResponseData(
      /*uuid=*/kFolder2Uuid, /*parent_uuid=*/BookmarkBarUuid(), kFolder2Title,
      /*url=*/std::string(),
      /*is_folder=*/true, /*unique_position=*/posFolder2));
  updates.push_back(CreateUpdateResponseData(
      /*uuid=*/kFolder3Uuid, /*parent_uuid=*/BookmarkBarUuid(), kFolder3Title,
      /*url=*/std::string(),
      /*is_folder=*/true, /*unique_position=*/posFolder3));

  // -------- The expected merge outcome --------
  // bookmark_bar
  //  |- folder 1
  //  |- folder 3
  //  |- folder 2

  std::unique_ptr<SyncedBookmarkTracker> tracker =
      Merge(std::move(updates), &bookmark_model);
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
  std::vector<const SyncedBookmarkTrackerEntity*> local_changes =
      tracker->GetEntitiesWithLocalChanges();
  EXPECT_THAT(local_changes.size(), Eq(0U));

  // Verify positions in tracker.
  EXPECT_TRUE(PositionsInTrackerMatchModel(bookmark_bar_node, *tracker));
}

TEST(BookmarkModelMergerTest, ShouldIgnoreManagedNodes) {
  auto client = std::make_unique<bookmarks::TestBookmarkClient>();
  bookmarks::BookmarkNode* managed_node = client->EnableManagedNode();
  TestBookmarkModelView view(
      TestBookmarkModelView::ViewType::kLocalOrSyncableNodes,
      std::move(client));

  const bookmarks::BookmarkNode* unsyncable_node =
      view.underlying_model()->AddURL(/*parent=*/managed_node, /*index=*/0,
                                      u"Title", GURL("http://www.url.com"));
  ASSERT_FALSE(view.IsNodeSyncable(unsyncable_node));

  std::unique_ptr<SyncedBookmarkTracker> tracker =
      Merge(syncer::UpdateResponseDataList(), &view);
  ASSERT_THAT(tracker, NotNull());

  EXPECT_THAT(tracker->GetEntityForBookmarkNode(unsyncable_node), IsNull());
  EXPECT_THAT(tracker->GetEntitiesWithLocalChanges(), IsEmpty());
  EXPECT_THAT(managed_node->children().size(), Eq(1));
}

TEST(BookmarkModelMergerTest, ShouldIgnoreUnsyncableNodes) {
  base::test::ScopedFeatureList override_features{
      syncer::kSyncEnableBookmarksInTransportMode};
  TestBookmarkModelView view(TestBookmarkModelView::ViewType::kAccountNodes);
  view.EnsurePermanentNodesExist();

  const bookmarks::BookmarkNode* unsyncable_node =
      view.underlying_model()->AddURL(
          /*parent=*/view.underlying_model()->bookmark_bar_node(), /*index=*/0,
          u"Title", GURL("http://www.url.com"));
  ASSERT_FALSE(view.IsNodeSyncable(unsyncable_node));

  std::unique_ptr<SyncedBookmarkTracker> tracker =
      Merge(syncer::UpdateResponseDataList(), &view);
  ASSERT_THAT(tracker, NotNull());

  EXPECT_THAT(tracker->GetEntityForBookmarkNode(unsyncable_node), IsNull());
  EXPECT_THAT(tracker->GetEntitiesWithLocalChanges(), IsEmpty());
  EXPECT_THAT(view.underlying_model()->bookmark_bar_node()->children().size(),
              Eq(1));
}

// Regression test for crbug.com/329278277. A UUID collision with an unsyncable
// node is a common scenario for the case where BookmarkModelMerger is being
// exercised for account bookmarks, while local unsyncable bookmarks contain an
// exact copy of the server-side updates as a result of sync-the-feature having
// been previously turned on and later off.
TEST(BookmarkModelMergerTest, ShouldIgnoreUnsyncableNodeWithCollidingUuid) {
  base::test::ScopedFeatureList override_features{
      syncer::kSyncEnableBookmarksInTransportMode};
  TestBookmarkModelView view(TestBookmarkModelView::ViewType::kAccountNodes);
  view.EnsurePermanentNodesExist();

  const bookmarks::BookmarkNode* unsyncable_node =
      view.underlying_model()->AddURL(
          /*parent=*/view.underlying_model()->bookmark_bar_node(), /*index=*/0,
          u"Title", GURL("http://www.foo.com"));
  ASSERT_FALSE(view.IsNodeSyncable(unsyncable_node));

  syncer::UpdateResponseDataList updates;
  updates.push_back(CreateBookmarkBarNodeUpdateData());
  updates.push_back(CreateUpdateResponseData(
      unsyncable_node->uuid(), /*parent_uuid=*/BookmarkBarUuid(), "Title",
      /*url=*/"http://www.bar.com",
      /*is_folder=*/false,
      syncer::UniquePosition::InitialPosition(
          syncer::UniquePosition::RandomSuffix())));

  std::unique_ptr<SyncedBookmarkTracker> tracker =
      Merge(std::move(updates), &view);
  ASSERT_THAT(tracker, NotNull());

  EXPECT_THAT(tracker->GetEntityForBookmarkNode(unsyncable_node), IsNull());
  EXPECT_THAT(tracker->GetEntitiesWithLocalChanges(), IsEmpty());
  EXPECT_THAT(view.underlying_model()->bookmark_bar_node()->children().size(),
              Eq(1));
}

TEST(BookmarkModelMergerTest, ShouldMergeFaviconsForRemoteNodesOnly) {
  const std::string kTitle1 = "title1";
  const GURL kUrl1("http://www.url1.com");
  // -------- The local model --------
  // bookmark_bar
  //  |- title 1

  TestBookmarkModelView bookmark_model;

  const bookmarks::BookmarkNode* bookmark_bar_node =
      bookmark_model.bookmark_bar_node();
  bookmark_model.AddURL(
      /*parent=*/bookmark_bar_node, /*index=*/0, base::UTF8ToUTF16(kTitle1),
      kUrl1);

  // -------- The remote model --------
  // bookmark_bar
  //  |- title 2

  const std::string kTitle2 = "title2";
  const base::Uuid kUuid2 = base::Uuid::GenerateRandomV4();
  const GURL kUrl2("http://www.url2.com");
  const GURL kIcon2Url("http://www.icon-url.com");
  syncer::UniquePosition pos2 = syncer::UniquePosition::InitialPosition(
      syncer::UniquePosition::RandomSuffix());

  syncer::UpdateResponseDataList updates;
  updates.push_back(CreateBookmarkBarNodeUpdateData());
  updates.push_back(CreateUpdateResponseData(
      /*uuid=*/kUuid2, /*parent_uuid=*/BookmarkBarUuid(), kTitle2, kUrl2.spec(),
      /*is_folder=*/false, /*unique_position=*/pos2, kIcon2Url.spec(),
      /*icon_data=*/"PNG"));

  // -------- The expected merge outcome --------
  // bookmark_bar
  //  |- title 2
  //  |- title 1

  std::unique_ptr<SyncedBookmarkTracker> tracker =
      SyncedBookmarkTracker::CreateEmpty(sync_pb::DataTypeState());
  testing::NiceMock<favicon::MockFaviconService> favicon_service;

  // Favicon should be set for the remote node.
  EXPECT_CALL(favicon_service,
              AddPageNoVisitForBookmark(kUrl2, base::UTF8ToUTF16(kTitle2)));
  EXPECT_CALL(favicon_service, MergeFavicon(kUrl2, _, _, _, _));

  BookmarkModelMerger(std::move(updates), &bookmark_model, &favicon_service,
                      tracker.get())
      .Merge();
}

// This tests that canonical titles produced by legacy clients are properly
// matched. Legacy clients append blank space to empty titles.
TEST(BookmarkModelMergerTest,
     ShouldMergeLocalAndRemoteNodesWhenRemoteHasLegacyCanonicalTitle) {
  const std::string kLocalTitle = "";
  const std::string kRemoteTitle = " ";
  const base::Uuid kUuid = base::Uuid::GenerateRandomV4();

  TestBookmarkModelView bookmark_model;

  // -------- The local model --------
  const bookmarks::BookmarkNode* bookmark_bar_node =
      bookmark_model.bookmark_bar_node();
  const bookmarks::BookmarkNode* folder = bookmark_model.AddFolder(
      /*parent=*/bookmark_bar_node, /*index=*/0,
      base::UTF8ToUTF16(kLocalTitle));
  ASSERT_TRUE(folder);

  // -------- The remote model --------
  syncer::UpdateResponseDataList updates;
  updates.push_back(CreateBookmarkBarNodeUpdateData());
  updates.push_back(
      UpdateResponseDataBuilder(/*uuid=*/kUuid,
                                /*parent_uuid=*/BookmarkBarUuid(), kRemoteTitle,
                                /*unique_position=*/MakeRandomPosition())
          .SetLegacyTitleOnly()
          .Build());

  std::unique_ptr<SyncedBookmarkTracker> tracker =
      Merge(std::move(updates), &bookmark_model);

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
  const base::Uuid kUuid = base::Uuid::GenerateRandomV4();

  TestBookmarkModelView bookmark_model;

  // -------- The local model --------
  const bookmarks::BookmarkNode* bookmark_bar_node =
      bookmark_model.bookmark_bar_node();
  const bookmarks::BookmarkNode* folder = bookmark_model.AddFolder(
      /*parent=*/bookmark_bar_node, /*index=*/0,
      base::UTF8ToUTF16(kLocalLongTitle));
  ASSERT_TRUE(folder);

  // -------- The remote model --------
  syncer::UpdateResponseDataList updates;
  updates.push_back(CreateBookmarkBarNodeUpdateData());
  updates.push_back(CreateUpdateResponseData(
      /*uuid=*/kUuid, /*parent_uuid=*/BookmarkBarUuid(), kRemoteTruncatedTitle,
      /*url=*/std::string(),
      /*is_folder=*/true, /*unique_position=*/MakeRandomPosition()));

  std::unique_ptr<SyncedBookmarkTracker> tracker =
      SyncedBookmarkTracker::CreateEmpty(sync_pb::DataTypeState());
  testing::NiceMock<favicon::MockFaviconService> favicon_service;
  BookmarkModelMerger(std::move(updates), &bookmark_model, &favicon_service,
                      tracker.get())
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
  const base::Uuid kUuid = base::Uuid::GenerateRandomV4();

  TestBookmarkModelView bookmark_model;

  // -------- The local model --------
  const bookmarks::BookmarkNode* bookmark_bar_node =
      bookmark_model.bookmark_bar_node();
  const bookmarks::BookmarkNode* folder = bookmark_model.AddFolder(
      /*parent=*/bookmark_bar_node, /*index=*/0,
      base::UTF8ToUTF16(kLocalLongTitle));
  ASSERT_TRUE(folder);

  // -------- The remote model --------
  syncer::UpdateResponseDataList updates;
  updates.push_back(CreateBookmarkBarNodeUpdateData());
  updates.push_back(CreateUpdateResponseData(
      /*uuid=*/kUuid, /*parent_uuid=*/BookmarkBarUuid(),
      kRemoteTruncatedFullTitle,
      /*url=*/std::string(),
      /*is_folder=*/true, /*unique_position=*/MakeRandomPosition()));

  updates.back().entity.specifics.mutable_bookmark()->set_full_title(
      kRemoteTruncatedFullTitle);

  std::unique_ptr<SyncedBookmarkTracker> tracker =
      SyncedBookmarkTracker::CreateEmpty(sync_pb::DataTypeState());
  testing::NiceMock<favicon::MockFaviconService> favicon_service;
  BookmarkModelMerger(std::move(updates), &bookmark_model, &favicon_service,
                      tracker.get())
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
  const base::Uuid kUuid = base::Uuid::GenerateRandomV4();

  TestBookmarkModelView bookmark_model;

  // -------- The local model --------
  const bookmarks::BookmarkNode* bookmark_bar_node =
      bookmark_model.bookmark_bar_node();
  const bookmarks::BookmarkNode* folder = bookmark_model.AddFolder(
      /*parent=*/bookmark_bar_node, /*index=*/0,
      base::UTF8ToUTF16(kLocalTruncatedTitle));
  ASSERT_TRUE(folder);

  // -------- The remote model --------
  syncer::UpdateResponseDataList updates;
  updates.push_back(CreateBookmarkBarNodeUpdateData());
  updates.push_back(CreateUpdateResponseData(
      /*uuid=*/kUuid, /*parent_uuid=*/BookmarkBarUuid(),
      sync_bookmarks::FullTitleToLegacyCanonicalizedTitle(kRemoteFullTitle),
      /*url=*/std::string(),
      /*is_folder=*/true, /*unique_position=*/MakeRandomPosition()));
  ASSERT_EQ(
      kLocalTruncatedTitle,
      updates.back().entity.specifics.bookmark().legacy_canonicalized_title());

  updates.back().entity.specifics.mutable_bookmark()->set_full_title(
      kRemoteFullTitle);

  std::unique_ptr<SyncedBookmarkTracker> tracker =
      SyncedBookmarkTracker::CreateEmpty(sync_pb::DataTypeState());
  testing::NiceMock<favicon::MockFaviconService> favicon_service;
  BookmarkModelMerger(std::move(updates), &bookmark_model, &favicon_service,
                      tracker.get())
      .Merge();

  // Both titles should have matched against each other and only node is in the
  // model and the tracker.
  EXPECT_THAT(bookmark_bar_node->children().size(), Eq(1u));
  EXPECT_THAT(tracker->TrackedEntitiesCountForTest(), Eq(2U));
}

TEST(BookmarkModelMergerTest, ShouldMergeAndUseRemoteUuid) {
  const base::Uuid kUuid = base::Uuid::GenerateRandomV4();
  const std::string kTitle = "Title";
  const base::Uuid kRemoteUuid = base::Uuid::GenerateRandomV4();

  TestBookmarkModelView bookmark_model;

  // -------- The local model --------
  const bookmarks::BookmarkNode* bookmark_bar_node =
      bookmark_model.bookmark_bar_node();
  const bookmarks::BookmarkNode* folder = bookmark_model.AddFolder(
      /*parent=*/bookmark_bar_node, /*index=*/0, base::UTF8ToUTF16(kTitle));
  ASSERT_TRUE(folder);

  // -------- The remote model --------
  syncer::UpdateResponseDataList updates;
  updates.push_back(CreateBookmarkBarNodeUpdateData());
  updates.push_back(CreateUpdateResponseData(
      /*uuid=*/kRemoteUuid, /*parent_uuid=*/BookmarkBarUuid(), kTitle,
      /*url=*/std::string(),
      /*is_folder=*/true, /*unique_position=*/MakeRandomPosition()));

  std::unique_ptr<SyncedBookmarkTracker> tracker =
      Merge(std::move(updates), &bookmark_model);

  // Node should have been replaced and UUID should be set to that stored in the
  // specifics.
  ASSERT_EQ(bookmark_bar_node->children().size(), 1u);
  const bookmarks::BookmarkNode* bookmark =
      bookmark_model.bookmark_bar_node()->children()[0].get();
  EXPECT_EQ(bookmark->uuid(), kRemoteUuid);
  EXPECT_THAT(tracker->GetEntityForBookmarkNode(bookmark), NotNull());
}

TEST(BookmarkModelMergerTest,
     ShouldMergeAndKeepOldUuidWhenRemoteUuidIsInvalid) {
  const base::Uuid kUuid = base::Uuid::GenerateRandomV4();
  const std::string kTitle = "Title";

  TestBookmarkModelView bookmark_model;

  // -------- The local model --------
  const bookmarks::BookmarkNode* bookmark_bar_node =
      bookmark_model.bookmark_bar_node();
  const bookmarks::BookmarkNode* folder = bookmark_model.AddFolder(
      /*parent=*/bookmark_bar_node, /*index=*/0, base::UTF8ToUTF16(kTitle));
  ASSERT_TRUE(folder);
  const base::Uuid old_uuid = folder->uuid();

  // -------- The remote model --------
  syncer::UpdateResponseDataList updates;
  updates.push_back(CreateBookmarkBarNodeUpdateData());
  updates.push_back(CreateUpdateResponseData(
      /*uuid=*/base::Uuid::GenerateRandomV4(),
      /*parent_uuid=*/BookmarkBarUuid(), kTitle,
      /*url=*/std::string(),
      /*is_folder=*/true,
      /*unique_position=*/MakeRandomPosition()));
  updates.back().entity.specifics.mutable_bookmark()->set_guid("invalid_guid");

  std::unique_ptr<SyncedBookmarkTracker> tracker =
      Merge(std::move(updates), &bookmark_model);

  // Node should not have been replaced and UUID should not have been set to
  // that stored in the specifics, as it was invalid.
  ASSERT_EQ(bookmark_bar_node->children().size(), 1u);
  const bookmarks::BookmarkNode* bookmark =
      bookmark_model.bookmark_bar_node()->children()[0].get();
  EXPECT_EQ(bookmark->uuid(), old_uuid);
  EXPECT_THAT(tracker->GetEntityForBookmarkNode(bookmark), NotNull());
}

TEST(BookmarkModelMergerTest, ShouldMergeBookmarkByUuid) {
  const std::string kLocalTitle = "Title 1";
  const std::string kRemoteTitle = "Title 2";
  const std::string kUrl = "http://www.foo.com/";
  const base::Uuid kUuid = base::Uuid::GenerateRandomV4();

  TestBookmarkModelView bookmark_model;

  // -------- The local model --------
  // bookmark_bar
  //  | - bookmark(kUuid/kLocalTitle)

  const bookmarks::BookmarkNode* bookmark_bar_node =
      bookmark_model.bookmark_bar_node();
  const bookmarks::BookmarkNode* bookmark = bookmark_model.AddURL(
      /*parent=*/bookmark_bar_node, /*index=*/0, base::UTF8ToUTF16(kLocalTitle),
      GURL(kUrl), /*meta_info=*/nullptr, base::Time::Now(), kUuid);
  ASSERT_TRUE(bookmark);
  ASSERT_THAT(bookmark_bar_node->children(), ElementRawPointersAre(bookmark));

  // -------- The remote model --------
  // bookmark_bar
  //  | - bookmark(kUuid/kRemoteTitle)

  syncer::UpdateResponseDataList updates;
  updates.push_back(CreateBookmarkBarNodeUpdateData());
  updates.push_back(CreateUpdateResponseData(
      /*uuid=*/kUuid, /*parent_uuid=*/BookmarkBarUuid(), kRemoteTitle,
      /*url=*/kUrl,
      /*is_folder=*/false,
      /*unique_position=*/MakeRandomPosition()));

  std::unique_ptr<SyncedBookmarkTracker> tracker =
      Merge(std::move(updates), &bookmark_model);

  // -------- The merged model --------
  // bookmark_bar
  //  |- bookmark(kUuid/kRemoteTitle)

  // Node should have been merged.
  EXPECT_THAT(bookmark_bar_node->children(), ElementRawPointersAre(bookmark));
  EXPECT_EQ(bookmark->GetTitle(), base::UTF8ToUTF16(kRemoteTitle));
  EXPECT_THAT(tracker->GetEntityForBookmarkNode(bookmark), NotNull());
}

TEST(BookmarkModelMergerTest, ShouldMergeBookmarkByUuidAndReparent) {
  const std::string kLocalTitle = "Title 1";
  const std::string kRemoteTitle = "Title 2";
  const std::string kUrl = "http://www.foo.com/";
  const base::Uuid kUuid = base::Uuid::GenerateRandomV4();

  TestBookmarkModelView bookmark_model;

  // -------- The local model --------
  // bookmark_bar
  //  | - folder
  //    | - bookmark(kUuid)

  const bookmarks::BookmarkNode* bookmark_bar_node =
      bookmark_model.bookmark_bar_node();
  const bookmarks::BookmarkNode* folder = bookmark_model.AddFolder(
      /*parent=*/bookmark_bar_node, /*index=*/0, u"Folder Title");
  const bookmarks::BookmarkNode* bookmark = bookmark_model.AddURL(
      /*parent=*/folder, /*index=*/0, base::UTF8ToUTF16(kLocalTitle),
      GURL(kUrl), /*meta_info=*/nullptr, base::Time::Now(), kUuid);
  ASSERT_TRUE(folder);
  ASSERT_TRUE(bookmark);
  ASSERT_THAT(bookmark_bar_node->children(), ElementRawPointersAre(folder));
  ASSERT_THAT(folder->children(), ElementRawPointersAre(bookmark));

  // -------- The remote model --------
  // bookmark_bar
  //  |- bookmark(kUuid)

  syncer::UpdateResponseDataList updates;
  updates.push_back(CreateBookmarkBarNodeUpdateData());
  updates.push_back(CreateUpdateResponseData(
      /*uuid=*/kUuid, /*parent_uuid=*/BookmarkBarUuid(), kRemoteTitle,
      /*url=*/kUrl,
      /*is_folder=*/false,
      /*unique_position=*/MakeRandomPosition()));

  std::unique_ptr<SyncedBookmarkTracker> tracker =
      Merge(std::move(updates), &bookmark_model);

  // -------- The merged model --------
  // bookmark_bar
  //  | - bookmark(kUuid/kRemoteTitle)
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

TEST(BookmarkModelMergerTest, ShouldMergeFolderByUuidAndNotSemantics) {
  const std::string kFolderId = "Folder Id";
  const std::string kTitle1 = "Title 1";
  const std::string kTitle2 = "Title 2";
  const std::string kUrl = "http://www.foo.com/";
  const base::Uuid kUuid1 = base::Uuid::GenerateRandomV4();
  const base::Uuid kUuid2 = base::Uuid::GenerateRandomV4();

  TestBookmarkModelView bookmark_model;

  // -------- The local model --------
  // bookmark_bar
  //  | - folder 1 (kUuid1/kTitle1)
  //    | - folder 2 (kUuid2/kTitle2)

  const bookmarks::BookmarkNode* bookmark_bar_node =
      bookmark_model.bookmark_bar_node();
  const bookmarks::BookmarkNode* folder1 = bookmark_model.AddFolder(
      /*parent=*/bookmark_bar_node, /*index=*/0, base::UTF8ToUTF16(kTitle1),
      /*meta_info=*/nullptr, /*creation_time=*/base::Time::Now(), kUuid1);
  const bookmarks::BookmarkNode* folder2 = bookmark_model.AddFolder(
      /*parent=*/folder1, /*index=*/0, base::UTF8ToUTF16(kTitle2),
      /*meta_info=*/nullptr, /*creation_time=*/base::Time::Now(), kUuid2);
  ASSERT_TRUE(folder1);
  ASSERT_TRUE(folder2);
  ASSERT_THAT(bookmark_bar_node->children(), ElementRawPointersAre(folder1));
  ASSERT_THAT(folder1->children(), ElementRawPointersAre(folder2));

  // -------- The remote model --------
  // bookmark_bar
  //  | - folder (kUuid2/kTitle1)

  syncer::UpdateResponseDataList updates;
  updates.push_back(CreateBookmarkBarNodeUpdateData());

  // Add a remote folder to correspond to the local folder by UUID and
  // semantics.
  updates.push_back(CreateUpdateResponseData(
      /*uuid=*/kUuid2, /*parent_uuid=*/BookmarkBarUuid(), kTitle1,
      /*url=*/"",
      /*is_folder=*/true,
      /*unique_position=*/MakeRandomPosition()));

  std::unique_ptr<SyncedBookmarkTracker> tracker =
      Merge(std::move(updates), &bookmark_model);

  // -------- The merged model --------
  // bookmark_bar
  //  | - folder 2 (kUuid2/kTitle1)
  //  | - folder 1 (kUuid1/kTitle1)

  // Node should have been merged with its UUID match.
  EXPECT_THAT(bookmark_bar_node->children(),
              ElementRawPointersAre(folder2, folder1));
  EXPECT_EQ(folder1->uuid(), kUuid1);
  EXPECT_EQ(folder1->GetTitle(), base::UTF8ToUTF16(kTitle1));
  EXPECT_EQ(folder1->children().size(), 0u);
  EXPECT_EQ(folder2->uuid(), kUuid2);
  EXPECT_EQ(folder2->GetTitle(), base::UTF8ToUTF16(kTitle1));
  EXPECT_THAT(tracker->GetEntityForBookmarkNode(folder1), NotNull());
  EXPECT_THAT(tracker->GetEntityForBookmarkNode(folder2), NotNull());
}

TEST(BookmarkModelMergerTest, ShouldIgnoreChildrenForNonFolderNodes) {
  const std::string kChildId = "child_id";
  const std::string kParentTitle = "Parent Title";
  const std::string kChildTitle = "Child Title";
  const base::Uuid kUuid1 = base::Uuid::GenerateRandomV4();
  const base::Uuid kUuid2 = base::Uuid::GenerateRandomV4();
  const std::string kUrl1 = "http://www.foo.com/";
  const std::string kUrl2 = "http://www.bar.com/";

  // -------- The remote model --------
  // bookmark_bar
  //  | - bookmark (kUuid1/kParentTitle, not a folder)
  //    | - bookmark

  syncer::UpdateResponseDataList updates;
  updates.push_back(CreateBookmarkBarNodeUpdateData());

  const syncer::UniquePosition::Suffix suffix =
      syncer::UniquePosition::RandomSuffix();
  const syncer::UniquePosition pos1 =
      syncer::UniquePosition::InitialPosition(suffix);
  const syncer::UniquePosition pos2 =
      syncer::UniquePosition::After(pos1, suffix);

  updates.push_back(CreateUpdateResponseData(
      /*uuid=*/kUuid1, /*parent_uuid=*/BookmarkBarUuid(), kParentTitle,
      /*url=*/kUrl1,
      /*is_folder=*/false,
      /*unique_position=*/pos1));

  updates.push_back(CreateUpdateResponseData(
      /*uuid=*/kUuid2, /*parent_uuid=*/kUuid1, kChildTitle,
      /*url=*/kUrl2,
      /*is_folder=*/false,
      /*unique_position=*/pos2));

  TestBookmarkModelView bookmark_model;
  std::unique_ptr<SyncedBookmarkTracker> tracker =
      Merge(std::move(updates), &bookmark_model);

  // -------- The merged model --------
  // bookmark_bar
  //  | - bookmark (kUuid1/kParentTitle)

  const bookmarks::BookmarkNode* bookmark_bar_node =
      bookmark_model.bookmark_bar_node();

  ASSERT_EQ(bookmark_bar_node->children().size(), 1u);
  EXPECT_EQ(bookmark_bar_node->children()[0]->uuid(), kUuid1);
  EXPECT_EQ(bookmark_bar_node->children()[0]->GetTitle(),
            base::UTF8ToUTF16(kParentTitle));
  EXPECT_EQ(bookmark_bar_node->children()[0]->children().size(), 0u);
  EXPECT_EQ(tracker->TrackedEntitiesCountForTest(), 2U);
}

TEST(
    BookmarkModelMergerTest,
    ShouldIgnoreFolderSemanticsMatchAndLaterMatchByUuidWithSemanticsNodeFirst) {
  const std::string kFolderId1 = "Folder Id 1";
  const std::string kFolderId2 = "Folder Id 2";
  const std::string kOriginalTitle = "Original Title";
  const std::string kNewTitle = "New Title";
  const base::Uuid kUuid1 = base::Uuid::GenerateRandomV4();
  const base::Uuid kUuid2 = base::Uuid::GenerateRandomV4();

  TestBookmarkModelView bookmark_model;

  // -------- The local model --------
  // bookmark_bar
  //  | - folder (kUuid1/kOriginalTitle)
  //    | - bookmark

  const bookmarks::BookmarkNode* bookmark_bar_node =
      bookmark_model.bookmark_bar_node();
  const bookmarks::BookmarkNode* folder = bookmark_model.AddFolder(
      /*parent=*/bookmark_bar_node, /*index=*/0,
      base::UTF8ToUTF16(kOriginalTitle), /*meta_info=*/nullptr,
      /*creation_time=*/base::Time::Now(), kUuid1);
  const bookmarks::BookmarkNode* bookmark = bookmark_model.AddURL(
      /*parent=*/folder, /*index=*/0, u"Bookmark Title",
      GURL("http://foo.com/"));
  ASSERT_TRUE(folder);
  ASSERT_TRUE(bookmark);
  ASSERT_THAT(bookmark_bar_node->children(), ElementRawPointersAre(folder));
  ASSERT_THAT(folder->children(), ElementRawPointersAre(bookmark));

  // -------- The remote model --------
  // bookmark_bar
  //  | - folder (kUuid2/kOriginalTitle)
  //  | - folder (kUuid1/kNewTitle)

  syncer::UpdateResponseDataList updates;
  updates.push_back(CreateBookmarkBarNodeUpdateData());

  const syncer::UniquePosition::Suffix suffix =
      syncer::UniquePosition::RandomSuffix();
  syncer::UniquePosition pos1 = syncer::UniquePosition::InitialPosition(suffix);
  syncer::UniquePosition pos2 = syncer::UniquePosition::After(pos1, suffix);

  // Add a remote folder to correspond to the local folder by semantics and not
  // UUID.
  updates.push_back(CreateUpdateResponseData(
      /*uuid=*/kUuid2, /*parent_uuid=*/BookmarkBarUuid(), kOriginalTitle,
      /*url=*/"",
      /*is_folder=*/true,
      /*unique_position=*/pos1));

  // Add a remote folder to correspond to the local folder by UUID and not
  // semantics.
  updates.push_back(CreateUpdateResponseData(
      /*uuid=*/kUuid1, /*parent_uuid=*/BookmarkBarUuid(), kNewTitle,
      /*url=*/"",
      /*is_folder=*/true,
      /*unique_position=*/pos2));

  std::unique_ptr<SyncedBookmarkTracker> tracker =
      Merge(std::move(updates), &bookmark_model);

  // -------- The merged model --------
  // bookmark_bar
  //  | - folder (kUuid2/kOriginalTitle)
  //  | - folder (kUuid1/kNewTitle)
  //    | - bookmark

  // Node should have been merged with its UUID match.
  ASSERT_EQ(bookmark_bar_node->children().size(), 2u);
  EXPECT_EQ(bookmark_bar_node->children()[0]->uuid(), kUuid2);
  EXPECT_EQ(bookmark_bar_node->children()[0]->GetTitle(),
            base::UTF8ToUTF16(kOriginalTitle));
  EXPECT_EQ(bookmark_bar_node->children()[0]->children().size(), 0u);
  EXPECT_EQ(bookmark_bar_node->children()[1]->uuid(), kUuid1);
  EXPECT_EQ(bookmark_bar_node->children()[1]->GetTitle(),
            base::UTF8ToUTF16(kNewTitle));
  EXPECT_EQ(bookmark_bar_node->children()[1]->children().size(), 1u);
  EXPECT_THAT(tracker->TrackedEntitiesCountForTest(), Eq(4U));
}

TEST(BookmarkModelMergerTest,
     ShouldIgnoreFolderSemanticsMatchAndLaterMatchByUuidWithUuidNodeFirst) {
  const std::string kFolderId1 = "Folder Id 1";
  const std::string kFolderId2 = "Folder Id 2";
  const std::string kOriginalTitle = "Original Title";
  const std::string kNewTitle = "New Title";
  const base::Uuid kUuid1 = base::Uuid::GenerateRandomV4();
  const base::Uuid kUuid2 = base::Uuid::GenerateRandomV4();

  TestBookmarkModelView bookmark_model;

  // -------- The local model --------
  // bookmark_bar
  //  | - folder (kUuid1/kOriginalTitle)
  //    | - bookmark

  const bookmarks::BookmarkNode* bookmark_bar_node =
      bookmark_model.bookmark_bar_node();
  const bookmarks::BookmarkNode* folder = bookmark_model.AddFolder(
      /*parent=*/bookmark_bar_node, /*index=*/0,
      base::UTF8ToUTF16(kOriginalTitle), /*meta_info=*/nullptr,
      /*creation_time=*/base::Time::Now(), kUuid1);
  const bookmarks::BookmarkNode* bookmark = bookmark_model.AddURL(
      /*parent=*/folder, /*index=*/0, u"Bookmark Title",
      GURL("http://foo.com/"));
  ASSERT_TRUE(folder);
  ASSERT_TRUE(bookmark);
  ASSERT_THAT(bookmark_bar_node->children(), ElementRawPointersAre(folder));
  ASSERT_THAT(folder->children(), ElementRawPointersAre(bookmark));

  // -------- The remote model --------
  // bookmark_bar
  //  | - folder (kUuid1/kNewTitle)
  //  | - folder (kUuid2/kOriginalTitle)

  syncer::UpdateResponseDataList updates;
  updates.push_back(CreateBookmarkBarNodeUpdateData());

  const syncer::UniquePosition::Suffix suffix =
      syncer::UniquePosition::RandomSuffix();
  syncer::UniquePosition pos1 = syncer::UniquePosition::InitialPosition(suffix);
  syncer::UniquePosition pos2 = syncer::UniquePosition::After(pos1, suffix);

  // Add a remote folder to correspond to the local folder by UUID and not
  // semantics.
  updates.push_back(CreateUpdateResponseData(
      /*uuid=*/kUuid1, /*parent_uuid=*/BookmarkBarUuid(), kNewTitle,
      /*url=*/"",
      /*is_folder=*/true,
      /*unique_position=*/pos1));

  // Add a remote folder to correspond to the local folder by
  // semantics and not UUID.
  updates.push_back(CreateUpdateResponseData(
      /*uuid=*/kUuid2, /*parent_uuid=*/BookmarkBarUuid(), kOriginalTitle,
      /*url=*/"",
      /*is_folder=*/true,
      /*unique_position=*/pos2));

  Merge(std::move(updates), &bookmark_model);

  // -------- The merged model --------
  // bookmark_bar
  //  | - folder (kUuid1/kNewTitle)
  //  | - folder (kUuid2/kOriginalTitle)

  // Node should have been merged with its UUID match.
  ASSERT_EQ(bookmark_bar_node->children().size(), 2u);
  EXPECT_EQ(bookmark_bar_node->children()[0]->uuid(), kUuid1);
  EXPECT_EQ(bookmark_bar_node->children()[0]->GetTitle(),
            base::UTF8ToUTF16(kNewTitle));
  EXPECT_EQ(bookmark_bar_node->children()[0]->children().size(), 1u);
  EXPECT_EQ(bookmark_bar_node->children()[1]->uuid(), kUuid2);
  EXPECT_EQ(bookmark_bar_node->children()[1]->GetTitle(),
            base::UTF8ToUTF16(kOriginalTitle));
  EXPECT_EQ(bookmark_bar_node->children()[1]->children().size(), 0u);
}

TEST(BookmarkModelMergerTest, ShouldReplaceBookmarkUuidWithConflictingURLs) {
  const std::string kTitle = "Title";
  const std::string kUrl1 = "http://www.foo.com/";
  const std::string kUrl2 = "http://www.bar.com/";
  const base::Uuid kUuid = base::Uuid::GenerateRandomV4();

  TestBookmarkModelView bookmark_model;

  // -------- The local model --------
  // bookmark_bar
  //  | - bookmark (kUuid/kUril1)

  const bookmarks::BookmarkNode* bookmark_bar_node =
      bookmark_model.bookmark_bar_node();
  const bookmarks::BookmarkNode* bookmark = bookmark_model.AddURL(
      /*parent=*/bookmark_bar_node, /*index=*/0, base::UTF8ToUTF16(kTitle),
      GURL(kUrl1), /*meta_info=*/nullptr, base::Time::Now(), kUuid);
  ASSERT_TRUE(bookmark);
  ASSERT_THAT(bookmark_bar_node->children(), ElementRawPointersAre(bookmark));

  // -------- The remote model --------
  // bookmark_bar
  //  | - bookmark (kUuid/kUrl2)

  syncer::UpdateResponseDataList updates;
  updates.push_back(CreateBookmarkBarNodeUpdateData());

  updates.push_back(CreateUpdateResponseData(  // Remote B
      /*uuid=*/kUuid, /*parent_uuid=*/BookmarkBarUuid(), kTitle,
      /*url=*/kUrl2,
      /*is_folder=*/false,
      /*unique_position=*/MakeRandomPosition()));

  Merge(std::move(updates), &bookmark_model);

  // -------- The merged model --------
  // bookmark_bar
  //  | - bookmark (kUuid/kUrl2)
  //  | - bookmark ([new UUID]/kUrl1)

  // Conflicting node UUID should have been replaced.
  ASSERT_EQ(bookmark_bar_node->children().size(), 2u);
  EXPECT_EQ(bookmark_bar_node->children()[0]->uuid(), kUuid);
  EXPECT_EQ(bookmark_bar_node->children()[0]->url(), kUrl2);
  EXPECT_NE(bookmark_bar_node->children()[1]->uuid(), kUuid);
  EXPECT_EQ(bookmark_bar_node->children()[1]->url(), kUrl1);
}

TEST(BookmarkModelMergerTest, ShouldReplaceBookmarkUuidWithConflictingTypes) {
  const std::string kTitle = "Title";
  const base::Uuid kUuid = base::Uuid::GenerateRandomV4();

  TestBookmarkModelView bookmark_model;

  // -------- The local model --------
  // bookmark_bar
  //  | - bookmark (kUuid)

  const bookmarks::BookmarkNode* bookmark_bar_node =
      bookmark_model.bookmark_bar_node();
  const bookmarks::BookmarkNode* bookmark = bookmark_model.AddURL(
      /*parent=*/bookmark_bar_node, /*index=*/0, base::UTF8ToUTF16(kTitle),
      GURL("http://www.foo.com/"), /*meta_info=*/nullptr, base::Time::Now(),
      kUuid);
  ASSERT_TRUE(bookmark);
  ASSERT_THAT(bookmark_bar_node->children(), ElementRawPointersAre(bookmark));

  // -------- The remote model --------
  // bookmark_bar
  //  | - folder(kUuid)

  syncer::UpdateResponseDataList updates;
  updates.push_back(CreateBookmarkBarNodeUpdateData());

  updates.push_back(CreateUpdateResponseData(  // Remote B
      /*uuid=*/kUuid, /*parent_uuid=*/BookmarkBarUuid(), kTitle,
      /*url=*/"",
      /*is_folder=*/true,
      /*unique_position=*/MakeRandomPosition()));

  Merge(std::move(updates), &bookmark_model);

  // -------- The merged model --------
  // bookmark_bar
  //  | - folder (kUuid)
  //  | - bookmark ([new UUID])

  // Conflicting node UUID should have been replaced.
  ASSERT_EQ(bookmark_bar_node->children().size(), 2u);
  EXPECT_EQ(bookmark_bar_node->children()[0]->uuid(), kUuid);
  EXPECT_TRUE(bookmark_bar_node->children()[0]->is_folder());
  EXPECT_NE(bookmark_bar_node->children()[1]->uuid(), kUuid);
  EXPECT_FALSE(bookmark_bar_node->children()[1]->is_folder());
}

TEST(BookmarkModelMergerTest,
     ShouldReplaceBookmarkUuidWithConflictingTypesAndLocalChildren) {
  const base::Uuid kUuid1 = base::Uuid::GenerateRandomV4();
  const base::Uuid kUuid2 = base::Uuid::GenerateRandomV4();

  TestBookmarkModelView bookmark_model;

  // -------- The local model --------
  // bookmark_bar
  //  | - folder (kUuid1)
  //    | - bookmark (kUuid2)

  const bookmarks::BookmarkNode* bookmark_bar_node =
      bookmark_model.bookmark_bar_node();
  const bookmarks::BookmarkNode* folder = bookmark_model.AddFolder(
      /*parent=*/bookmark_bar_node, /*index=*/0, u"Folder Title",
      /*meta_info=*/nullptr, /*creation_time=*/base::Time::Now(), kUuid1);
  const bookmarks::BookmarkNode* bookmark = bookmark_model.AddURL(
      /*parent=*/folder, /*index=*/0, u"Foo's title", GURL("http://foo.com"),
      /*meta_info=*/nullptr, /*creation_time=*/base::Time::Now(), kUuid2);
  ASSERT_TRUE(folder);
  ASSERT_TRUE(bookmark);
  ASSERT_THAT(bookmark_bar_node->children(), ElementRawPointersAre(folder));
  ASSERT_THAT(folder->children(), ElementRawPointersAre(bookmark));

  // -------- The remote model --------
  // bookmark_bar
  //  | - bookmark (kUuid1)

  syncer::UpdateResponseDataList updates;
  updates.push_back(CreateBookmarkBarNodeUpdateData());

  updates.push_back(CreateUpdateResponseData(
      /*uuid=*/kUuid1, /*parent_uuid=*/BookmarkBarUuid(), "Bar's title",
      "http://bar.com/",
      /*is_folder=*/false,
      /*unique_position=*/MakeRandomPosition()));

  Merge(std::move(updates), &bookmark_model);

  // -------- The merged model --------
  // bookmark_bar
  //  | - bookmark (kUuid1)
  //  | - folder ([new UUID])
  //    | - bookmark (kUuid2)

  // Conflicting node UUID should have been replaced.
  ASSERT_EQ(bookmark_bar_node->children().size(), 2u);
  EXPECT_EQ(bookmark_bar_node->children()[0]->uuid(), kUuid1);
  EXPECT_NE(bookmark_bar_node->children()[1]->uuid(), kUuid1);
  EXPECT_NE(bookmark_bar_node->children()[1]->uuid(), kUuid2);
  EXPECT_FALSE(bookmark_bar_node->children()[0]->is_folder());
  EXPECT_TRUE(bookmark_bar_node->children()[1]->is_folder());
  EXPECT_EQ(bookmark_bar_node->children()[1]->children().size(), 1u);
  EXPECT_FALSE(bookmark_bar_node->children()[1]->children()[0]->is_folder());
  EXPECT_EQ(bookmark_bar_node->children()[1]->children()[0]->uuid(), kUuid2);
}

// Tests that the UUID-based matching algorithm handles well the case where a
// local bookmark matches a remote bookmark that is orphan. In this case the
// remote node should be ignored and the local bookmark included in the merged
// tree.
TEST(BookmarkModelMergerTest, ShouldIgnoreRemoteUuidIfOrphanNode) {
  const std::string kInexistentParentId = "InexistentParentId";
  const std::string kTitle = "Title";
  const std::string kUrl = "http://www.foo.com/";
  const base::Uuid kUuid = base::Uuid::GenerateRandomV4();
  const base::Uuid kInexistentParentUuid = base::Uuid::GenerateRandomV4();

  TestBookmarkModelView bookmark_model;

  // -------- The local model --------
  // bookmark_bar
  //  | - bookmark(kUuid/kTitle)

  const bookmarks::BookmarkNode* bookmark_bar_node =
      bookmark_model.bookmark_bar_node();
  const bookmarks::BookmarkNode* bookmark = bookmark_model.AddURL(
      /*parent=*/bookmark_bar_node, /*index=*/0, base::UTF8ToUTF16(kTitle),
      GURL(kUrl), /*meta_info=*/nullptr, base::Time::Now(), kUuid);
  ASSERT_TRUE(bookmark);
  ASSERT_THAT(bookmark_bar_node->children(), ElementRawPointersAre(bookmark));

  // -------- The remote model --------
  // bookmark_bar
  // Orphan node: bookmark(kUuid/kTitle)

  syncer::UpdateResponseDataList updates;
  updates.push_back(CreateBookmarkBarNodeUpdateData());
  updates.push_back(CreateUpdateResponseData(
      /*uuid=*/kUuid, /*parent_uuid=*/kInexistentParentUuid, kTitle,
      /*url=*/kUrl,
      /*is_folder=*/false,
      /*unique_position=*/MakeRandomPosition()));

  std::unique_ptr<SyncedBookmarkTracker> tracker =
      Merge(std::move(updates), &bookmark_model);

  // -------- The merged model --------
  // bookmark_bar
  //  |- bookmark(kUuid/kTitle)

  // The local node should have been tracked.
  EXPECT_THAT(bookmark_bar_node->children(), ElementRawPointersAre(bookmark));
  EXPECT_EQ(bookmark->GetTitle(), base::UTF8ToUTF16(kTitle));
  EXPECT_THAT(tracker->GetEntityForBookmarkNode(bookmark), NotNull());

  EXPECT_THAT(tracker->GetEntityForUuid(kUuid), NotNull());
  EXPECT_THAT(tracker->GetEntityForUuid(kInexistentParentUuid), IsNull());
}

// Tests that the UUID-based matching algorithm handles well the case where a
// local bookmark matches a remote bookmark that contains invalid specifics
// (e.g. invalid URL). In this case the remote node should be ignored and the
// local bookmark included in the merged tree.
TEST(BookmarkModelMergerTest, ShouldIgnoreRemoteUuidIfInvalidSpecifics) {
  const std::string kTitle = "Title";
  const std::string kLocalUrl = "http://www.foo.com/";
  const std::string kInvalidUrl = "invalidurl";
  const base::Uuid kUuid = base::Uuid::GenerateRandomV4();

  TestBookmarkModelView bookmark_model;

  // -------- The local model --------
  // bookmark_bar
  //  | - bookmark(kUuid/kLocalUrl/kTitle)

  const bookmarks::BookmarkNode* bookmark_bar_node =
      bookmark_model.bookmark_bar_node();
  const bookmarks::BookmarkNode* bookmark = bookmark_model.AddURL(
      /*parent=*/bookmark_bar_node, /*index=*/0, base::UTF8ToUTF16(kTitle),
      GURL(kLocalUrl), /*meta_info=*/nullptr, base::Time::Now(), kUuid);
  ASSERT_TRUE(bookmark);
  ASSERT_THAT(bookmark_bar_node->children(), ElementRawPointersAre(bookmark));

  // -------- The remote model --------
  // bookmark_bar
  //  | - bookmark (kUuid/kInvalidURL/kTitle)

  syncer::UpdateResponseDataList updates;
  updates.push_back(CreateBookmarkBarNodeUpdateData());
  updates.push_back(CreateUpdateResponseData(
      /*uuid=*/kUuid, /*parent_uuid=*/BookmarkBarUuid(), kTitle,
      /*url=*/kInvalidUrl,
      /*is_folder=*/false,
      /*unique_position=*/MakeRandomPosition()));

  std::unique_ptr<SyncedBookmarkTracker> tracker =
      Merge(std::move(updates), &bookmark_model);

  // -------- The merged model --------
  // bookmark_bar
  //  |- bookmark(kUuid/kLocalUrl/kTitle)

  // The local node should have been tracked.
  EXPECT_THAT(bookmark_bar_node->children(), ElementRawPointersAre(bookmark));
  EXPECT_EQ(bookmark->url(), GURL(kLocalUrl));
  EXPECT_EQ(bookmark->GetTitle(), base::UTF8ToUTF16(kTitle));
  EXPECT_THAT(tracker->GetEntityForBookmarkNode(bookmark), NotNull());
}

// Tests that updates with a UUID that is different to originator client item ID
// are ignored.
TEST(BookmarkModelMergerTest, ShouldIgnoreRemoteUpdateWithInvalidUuid) {
  const base::Uuid kUuid1 = base::Uuid::GenerateRandomV4();
  const base::Uuid kUuid2 = base::Uuid::GenerateRandomV4();
  const std::string kTitle1 = "Title1";
  const std::string kTitle2 = "Title2";
  const std::string kLocalTitle = "LocalTitle";
  const std::string kUrl = "http://www.foo.com/";
  const base::Uuid kUuid = base::Uuid::GenerateRandomV4();
  const base::Uuid kUnexpectedOriginatorItemId = base::Uuid::GenerateRandomV4();

  TestBookmarkModelView bookmark_model;

  // -------- The local model --------
  //  | - bookmark(kUuid/kUrl/kLocalTitle)
  const bookmarks::BookmarkNode* bookmark_bar_node =
      bookmark_model.bookmark_bar_node();
  const bookmarks::BookmarkNode* bookmark = bookmark_model.AddURL(
      /*parent=*/bookmark_bar_node, /*index=*/0, base::UTF8ToUTF16(kLocalTitle),
      GURL(kUrl), /*meta_info=*/nullptr, base::Time::Now(), kUuid);
  ASSERT_TRUE(bookmark);
  ASSERT_THAT(bookmark_bar_node->children(), ElementRawPointersAre(bookmark));

  // -------- The remote model --------
  // bookmark_bar
  //  | - bookmark (kUuid/kUrl/kTitle1)
  //  | - bookmark (kUuid/kUrl/kTitle2)
  const syncer::UniquePosition::Suffix suffix =
      syncer::UniquePosition::RandomSuffix();
  syncer::UniquePosition position1 =
      syncer::UniquePosition::InitialPosition(suffix);
  syncer::UniquePosition position2 =
      syncer::UniquePosition::After(position1, suffix);

  syncer::UpdateResponseDataList updates;
  updates.push_back(CreateBookmarkBarNodeUpdateData());
  updates.push_back(CreateUpdateResponseData(
      /*uuid=*/kUuid, /*parent_uuid=*/BookmarkBarUuid(), kTitle1,
      /*url=*/kUrl,
      /*is_folder=*/false, /*unique_position=*/position1));
  updates.push_back(CreateUpdateResponseData(
      /*uuid=*/kUuid, /*parent_uuid=*/BookmarkBarUuid(), kTitle2,
      /*url=*/kUrl,
      /*is_folder=*/false, /*unique_position=*/position2));

  // |originator_client_item_id| cannot itself be duplicated because
  // DataTypeWorker guarantees otherwise.
  updates.back().entity.originator_client_item_id =
      kUnexpectedOriginatorItemId.AsLowercaseString();
  updates.back().entity.id =
      GetFakeServerIdFromUuid(kUnexpectedOriginatorItemId);

  std::unique_ptr<SyncedBookmarkTracker> tracker =
      Merge(std::move(updates), &bookmark_model);

  // -------- The merged model --------
  //  | - bookmark (kUuid/kUrl/kTitle1)

  // The second remote node should have been filtered out.
  ASSERT_EQ(bookmark_bar_node->children().size(), 1u);
  const bookmarks::BookmarkNode* merged_bookmark =
      bookmark_model.bookmark_bar_node()->children()[0].get();
  EXPECT_THAT(merged_bookmark->uuid(), Eq(kUuid));
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

  const base::Uuid kFolder1Uuid = base::Uuid::GenerateRandomV4();
  const base::Uuid kFolder2Uuid = base::Uuid::GenerateRandomV4();
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

  TestBookmarkModelView bookmark_model;

  const bookmarks::BookmarkNode* bookmark_bar_node =
      bookmark_model.bookmark_bar_node();
  const bookmarks::BookmarkNode* folder1 = bookmark_model.AddFolder(
      /*parent=*/bookmark_bar_node, /*index=*/0,
      base::UTF8ToUTF16(kFolder1Title));
  const bookmarks::BookmarkNode* folder1_url1_node = bookmark_model.AddURL(
      /*parent=*/folder1, /*index=*/0, base::UTF8ToUTF16(kUrl1Title),
      GURL(kUrl1));
  bookmark_model.AddURL(
      /*parent=*/folder1, /*index=*/1, base::UTF8ToUTF16(kUrl2Title),
      GURL(kUrl2));

  // The remote model contains two folders. The first one is the same as in
  // local model, but it does not contain any urls. The second one has the url1
  // from first folder with same UUID. This will cause skip local creation for
  // |url1| while processing |folder1|.
  //
  // -------- The remote model --------
  // bookmark_bar
  //  |- folder 1
  //  |- folder 2
  //    |- url1(http://www.url1.com)

  const syncer::UniquePosition::Suffix suffix =
      syncer::UniquePosition::RandomSuffix();
  syncer::UniquePosition posFolder1 =
      syncer::UniquePosition::InitialPosition(suffix);
  syncer::UniquePosition posFolder2 =
      syncer::UniquePosition::After(posFolder1, suffix);

  syncer::UniquePosition posUrl1 =
      syncer::UniquePosition::InitialPosition(suffix);

  syncer::UpdateResponseDataList updates;
  updates.push_back(CreateBookmarkBarNodeUpdateData());
  updates.push_back(CreateUpdateResponseData(
      /*uuid=*/kFolder1Uuid, /*parent_uuid=*/BookmarkBarUuid(), kFolder1Title,
      /*url=*/std::string(),
      /*is_folder=*/true, /*unique_position=*/posFolder1));
  updates.push_back(CreateUpdateResponseData(
      /*uuid=*/kFolder2Uuid, /*parent_uuid=*/BookmarkBarUuid(), kFolder2Title,
      /*url=*/std::string(),
      /*is_folder=*/true, /*unique_position=*/posFolder2));
  updates.push_back(CreateUpdateResponseData(
      /*uuid=*/folder1_url1_node->uuid(), /*parent_uuid=*/kFolder2Uuid,
      kUrl1Title, kUrl1,
      /*is_folder=*/false, /*unique_position=*/posUrl1));

  // -------- The expected merge outcome --------
  // bookmark_bar
  //  |- folder 1
  //    |- url2(http://www.url2.com)
  //  |- folder 2
  //    |- url1(http://www.url1.com)

  std::unique_ptr<SyncedBookmarkTracker> tracker =
      Merge(std::move(updates), &bookmark_model);
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

  std::vector<const SyncedBookmarkTrackerEntity*> local_changes =
      tracker->GetEntitiesWithLocalChanges();

  ASSERT_THAT(local_changes.size(), Eq(1U));
  EXPECT_THAT(local_changes[0]->bookmark_node(),
              Eq(bookmark_bar_node->children()[0]->children()[0].get()));

  // Verify positions in tracker.
  EXPECT_TRUE(PositionsInTrackerMatchModel(bookmark_bar_node, *tracker));
}

TEST(BookmarkModelMergerTest, ShouldLogMetricsForInvalidSpecifics) {
  TestBookmarkModelView bookmark_model;

  // -------- The remote model --------
  // bookmark_bar
  //  | - bookmark (<invalid url>)

  syncer::UpdateResponseDataList updates;
  updates.push_back(CreateBookmarkBarNodeUpdateData());
  updates.push_back(CreateUpdateResponseData(
      /*uuid=*/base::Uuid::GenerateRandomV4(),
      /*parent_uuid=*/BookmarkBarUuid(), "Title",
      /*url=*/"invalidurl",
      /*is_folder=*/false,
      /*unique_position=*/MakeRandomPosition()));

  base::HistogramTester histogram_tester;
  Merge(std::move(updates), &bookmark_model);
  histogram_tester.ExpectUniqueSample(
      "Sync.ProblematicServerSideBookmarksDuringMerge",
      /*sample=*/ExpectedRemoteBookmarkUpdateError::kInvalidSpecifics,
      /*expected_bucket_count=*/1);
}

TEST(BookmarkModelMergerTest, ShouldLogMetricsForChildrenOfNonFolder) {
  TestBookmarkModelView bookmark_model;

  const base::Uuid kUuid = base::Uuid::GenerateRandomV4();

  // -------- The remote model --------
  // bookmark_bar
  //  | - bookmark (url1/Title1)
  //    | - bookmark (url2/Title2)
  //    | - bookmark (url3/Title3)
  //    | - bookmark (url4/Title4)

  syncer::UpdateResponseDataList updates;
  updates.push_back(CreateBookmarkBarNodeUpdateData());
  updates.push_back(CreateUpdateResponseData(
      /*uuid=*/kUuid, /*parent_uuid=*/BookmarkBarUuid(), "Title1",
      /*url=*/"http://url1",
      /*is_folder=*/false,
      /*unique_position=*/MakeRandomPosition()));
  updates.push_back(CreateUpdateResponseData(
      /*uuid=*/base::Uuid::GenerateRandomV4(), /*parent_uuid=*/kUuid, "Title2",
      /*url=*/"http://url2",
      /*is_folder=*/false,
      /*unique_position=*/MakeRandomPosition()));
  updates.push_back(CreateUpdateResponseData(
      /*uuid=*/base::Uuid::GenerateRandomV4(), /*parent_uuid=*/kUuid, "Title3",
      /*url=*/"http://url3",
      /*is_folder=*/false,
      /*unique_position=*/MakeRandomPosition()));
  updates.push_back(CreateUpdateResponseData(
      /*uuid=*/base::Uuid::GenerateRandomV4(), /*parent_uuid=*/kUuid, "Title4",
      /*url=*/"http://url4",
      /*is_folder=*/false,
      /*unique_position=*/MakeRandomPosition()));

  base::HistogramTester histogram_tester;
  Merge(std::move(updates), &bookmark_model);
  histogram_tester.ExpectUniqueSample(
      "Sync.ProblematicServerSideBookmarksDuringMerge",
      /*sample=*/ExpectedRemoteBookmarkUpdateError::kParentNotFolder,
      /*expected_bucket_count=*/3);
}

TEST(BookmarkModelMergerTest, ShouldLogMetricsForChildrenOfOrphanUpdates) {
  TestBookmarkModelView bookmark_model;

  // -------- The remote model --------
  // bookmark_bar
  // Orphan node: bookmark(url1/title1)

  syncer::UpdateResponseDataList updates;
  updates.push_back(CreateBookmarkBarNodeUpdateData());
  updates.push_back(CreateUpdateResponseData(
      /*uuid=*/base::Uuid::GenerateRandomV4(),
      /*parent_uuid=*/base::Uuid::GenerateRandomV4(), "Title1",
      /*url=*/"http://url1",
      /*is_folder=*/false,
      /*unique_position=*/MakeRandomPosition()));

  base::HistogramTester histogram_tester;
  std::unique_ptr<SyncedBookmarkTracker> tracker =
      Merge(std::move(updates), &bookmark_model);
  ASSERT_THAT(tracker, NotNull());

  EXPECT_THAT(histogram_tester.GetTotalSum(
                  "Sync.BookmarkModelMerger.ValidInputUpdates"),
              Eq(2));
  histogram_tester.ExpectUniqueSample(
      "Sync.ProblematicServerSideBookmarksDuringMerge",
      /*sample=*/ExpectedRemoteBookmarkUpdateError::kMissingParentEntity,
      /*expected_bucket_count=*/1);
  EXPECT_THAT(histogram_tester.GetTotalSum(
                  "Sync.BookmarkModelMerger.ReachableInputUpdates"),
              Eq(1));

  EXPECT_THAT(tracker->GetNumIgnoredUpdatesDueToMissingParentForTest(), Eq(1));
}

TEST(BookmarkModelMergerTest, ShouldLogMetricsForUnsupportedServerTag) {
  TestBookmarkModelView bookmark_model;

  syncer::UpdateResponseDataList updates;
  updates.push_back(CreateBookmarkBarNodeUpdateData());
  updates.back().entity.server_defined_unique_tag = "someunknowntag";

  base::HistogramTester histogram_tester;
  Merge(std::move(updates), &bookmark_model);
  histogram_tester.ExpectUniqueSample(
      "Sync.ProblematicServerSideBookmarksDuringMerge",
      /*sample=*/ExpectedRemoteBookmarkUpdateError::kUnsupportedPermanentFolder,
      /*expected_bucket_count=*/1);
}

TEST(BookmarkModelMergerTest, ShouldLogMetricsForkDescendantOfRootNode) {
  const std::string kRootNodeId = "test_root_node_id";
  TestBookmarkModelView bookmark_model;

  // -------- The remote model --------
  // root node
  //  | - bookmark (url1/Title1)
  syncer::UpdateResponseDataList updates;
  updates.push_back(CreateBookmarkBarNodeUpdateData());
  updates.back().entity.id = kRootNodeId;
  updates.back().entity.server_defined_unique_tag =
      syncer::DataTypeToProtocolRootTag(syncer::BOOKMARKS);

  updates.push_back(CreateUpdateResponseData(
      /*uuid=*/base::Uuid::GenerateRandomV4(),
      base::Uuid::ParseLowercase(bookmarks::kRootNodeUuid), "Title1",
      /*url=*/"http://url1",
      /*is_folder=*/false,
      /*unique_position=*/MakeRandomPosition()));

  base::HistogramTester histogram_tester;
  Merge(std::move(updates), &bookmark_model);
  histogram_tester.ExpectUniqueSample(
      "Sync.ProblematicServerSideBookmarksDuringMerge",
      /*sample=*/ExpectedRemoteBookmarkUpdateError::kMissingParentEntity,
      /*expected_bucket_count=*/1);
}

TEST(BookmarkModelMergerTest, ShouldRemoveMatchingDuplicatesByUuid) {
  const std::string kTitle1 = "Title 1";
  const std::string kTitle2 = "Title 2";
  const std::string kTitle3 = "Title 3";
  const std::string kUrl = "http://www.url.com/";

  const base::Uuid kUrlUuid = base::Uuid::GenerateRandomV4();

  // The remote model has 2 duplicate folders with the same title and 2
  // duplicate bookmarks with the same URL.
  //
  // -------- The remote model --------
  // bookmark_bar
  //  |- url1(http://www.url.com, UrlUuid)
  //  |- url2(http://www.url.com, UrlUuid)
  //  |- url3(http://www.url.com, <other-uuid>)
  TestBookmarkModelView bookmark_model;

  syncer::UpdateResponseDataList updates;
  updates.push_back(CreateBookmarkBarNodeUpdateData());

  updates.push_back(CreateUpdateResponseData(
      /*uuid=*/kUrlUuid, /*parent_uuid=*/BookmarkBarUuid(), kTitle1,
      /*url=*/kUrl,
      /*is_folder=*/false, /*unique_position=*/MakeRandomPosition()));
  updates.back().entity.id = "Id1";
  updates.back().entity.creation_time = base::Time::Now() - base::Days(1);
  updates.push_back(CreateUpdateResponseData(
      /*uuid=*/kUrlUuid, /*parent_uuid=*/BookmarkBarUuid(), kTitle2,
      /*url=*/kUrl,
      /*is_folder=*/false, /*unique_position=*/MakeRandomPosition()));
  updates.back().entity.id = "Id2";
  updates.back().entity.creation_time = base::Time::Now();
  updates.push_back(CreateUpdateResponseData(
      /*uuid=*/base::Uuid::GenerateRandomV4(),
      /*parent_uuid=*/BookmarkBarUuid(), kTitle3,
      /*url=*/kUrl,
      /*is_folder=*/false, /*unique_position=*/MakeRandomPosition()));
  updates.back().entity.id = "Id3";
  updates.back().entity.creation_time = base::Time::Now();

  base::HistogramTester histogram_tester;
  std::unique_ptr<SyncedBookmarkTracker> tracker =
      Merge(std::move(updates), &bookmark_model);
  const bookmarks::BookmarkNode* bookmark_bar_node =
      bookmark_model.bookmark_bar_node();
  EXPECT_THAT(bookmark_bar_node->children(),
              UnorderedElementsAre(HasTitle(base::UTF8ToUTF16(kTitle2)),
                                   HasTitle(base::UTF8ToUTF16(kTitle3))));

  EXPECT_THAT(histogram_tester.GetTotalSum(
                  "Sync.BookmarkModelMerger.ValidInputUpdates"),
              Eq(4));
  histogram_tester.ExpectBucketCount(
      "Sync.BookmarksGUIDDuplicates",
      /*sample=*/ExpectedBookmarksUuidDuplicates::kMatchingUrls,
      /*expected_count=*/1);
  EXPECT_THAT(histogram_tester.GetTotalSum(
                  "Sync.BookmarkModelMerger.ReachableInputUpdates"),
              Eq(3));
}

TEST(BookmarkModelMergerTest, ShouldRemoveDifferentDuplicatesByUuid) {
  const std::string kTitle1 = "Title 1";
  const std::string kTitle2 = "Title 2";
  const std::string kUrl = "http://www.url.com/";
  const std::string kDifferentUrl = "http://www.different-url.com/";

  const base::Uuid kUrlUuid = base::Uuid::GenerateRandomV4();

  // The remote model will have 2 duplicate folders with
  // different titles and 2 duplicate bookmarks with different URLs
  //
  // -------- The remote model --------
  // bookmark_bar
  //  |- url1(http://www.url.com, UrlUUID)
  //  |- url2(http://www.different-url.com, UrlUUID)
  TestBookmarkModelView bookmark_model;

  syncer::UpdateResponseDataList updates;
  updates.push_back(CreateBookmarkBarNodeUpdateData());

  updates.push_back(CreateUpdateResponseData(
      /*uuid=*/kUrlUuid, /*parent_uuid=*/BookmarkBarUuid(), kTitle1,
      /*url=*/kUrl,
      /*is_folder=*/false, /*unique_position=*/MakeRandomPosition()));
  updates.back().entity.id = "Id1";
  updates.back().entity.creation_time = base::Time::Now();
  updates.push_back(CreateUpdateResponseData(
      /*uuid=*/kUrlUuid, /*parent_uuid=*/BookmarkBarUuid(), kTitle2,
      /*url=*/kDifferentUrl,
      /*is_folder=*/false, /*unique_position=*/MakeRandomPosition()));
  updates.back().entity.id = "Id2";
  updates.back().entity.creation_time = base::Time::Now() - base::Days(1);

  base::HistogramTester histogram_tester;
  std::unique_ptr<SyncedBookmarkTracker> tracker =
      Merge(std::move(updates), &bookmark_model);
  const bookmarks::BookmarkNode* bookmark_bar_node =
      bookmark_model.bookmark_bar_node();
  EXPECT_THAT(bookmark_bar_node->children(),
              UnorderedElementsAre(HasTitle(base::UTF8ToUTF16(kTitle1))));
  histogram_tester.ExpectBucketCount(
      "Sync.BookmarksGUIDDuplicates",
      /*sample=*/ExpectedBookmarksUuidDuplicates::kDifferentUrls,
      /*expected_count=*/1);
}

TEST(BookmarkModelMergerTest, ShouldRemoveMatchingFolderDuplicatesByUuid) {
  const std::string kTitle = "Title";

  const base::Uuid kUuid = base::Uuid::GenerateRandomV4();

  // The remote model has 2 duplicate folders with the same title and 2
  // duplicate bookmarks with the same URL.
  //
  // -------- The remote model --------
  // bookmark_bar
  //  |- folder1(Title, UUID)
  //  |- folder2(Title, UUID)
  TestBookmarkModelView bookmark_model;

  syncer::UpdateResponseDataList updates;
  updates.push_back(CreateBookmarkBarNodeUpdateData());

  updates.push_back(CreateUpdateResponseData(
      /*uuid=*/kUuid, /*parent_uuid=*/BookmarkBarUuid(), kTitle,
      /*url=*/"",
      /*is_folder=*/true, /*unique_position=*/MakeRandomPosition()));
  updates.back().entity.id = "Id1";
  updates.back().entity.creation_time = base::Time::Now() - base::Days(1);
  updates.push_back(CreateUpdateResponseData(
      /*uuid=*/kUuid, /*parent_uuid=*/BookmarkBarUuid(), kTitle,
      /*url=*/"",
      /*is_folder=*/true, /*unique_position=*/MakeRandomPosition()));
  updates.back().entity.id = "Id2";
  updates.back().entity.creation_time = base::Time::Now();

  base::HistogramTester histogram_tester;
  std::unique_ptr<SyncedBookmarkTracker> tracker =
      Merge(std::move(updates), &bookmark_model);
  const bookmarks::BookmarkNode* bookmark_bar_node =
      bookmark_model.bookmark_bar_node();
  ASSERT_THAT(bookmark_bar_node->children().size(), Eq(1u));
  histogram_tester.ExpectBucketCount(
      "Sync.BookmarksGUIDDuplicates",
      /*sample=*/ExpectedBookmarksUuidDuplicates::kMatchingFolders,
      /*expected_count=*/1);
  EXPECT_THAT(tracker->GetEntityForSyncId("Id1"), IsNull());
  EXPECT_THAT(tracker->GetEntityForSyncId("Id2"), NotNull());
}

TEST(BookmarkModelMergerTest, ShouldRemoveDifferentFolderDuplicatesByUuid) {
  const std::string kTitle1 = "Title 1";
  const std::string kTitle2 = "Title 2";

  const base::Uuid kUuid = base::Uuid::GenerateRandomV4();

  // The remote model has 2 duplicate folders with the same title and 2
  // duplicate bookmarks with the same URL.
  //
  // -------- The remote model --------
  // bookmark_bar
  //  |- folder1(Title, UUID)
  //    |- folder11
  //  |- folder2(Title, UUID)
  //    |- folder21
  TestBookmarkModelView bookmark_model;

  syncer::UpdateResponseDataList updates;
  updates.push_back(CreateBookmarkBarNodeUpdateData());

  updates.push_back(CreateUpdateResponseData(
      /*uuid=*/kUuid, /*parent_uuid=*/BookmarkBarUuid(), kTitle1,
      /*url=*/"",
      /*is_folder=*/true, MakeRandomPosition()));
  updates.back().entity.id = "Id1";
  updates.back().entity.creation_time = base::Time::Now();
  updates.push_back(CreateUpdateResponseData(
      /*uuid=*/base::Uuid::GenerateRandomV4(), /*parent_uuid=*/kUuid,
      "Some title",
      /*url=*/"", /*is_folder=*/true, MakeRandomPosition()));

  updates.push_back(CreateUpdateResponseData(
      /*uuid=*/kUuid, /*parent_uuid=*/BookmarkBarUuid(), kTitle2,
      /*url=*/"", /*is_folder=*/true, MakeRandomPosition()));
  updates.back().entity.id = "Id2";
  updates.back().entity.creation_time = base::Time::Now() - base::Days(1);
  updates.push_back(CreateUpdateResponseData(
      /*uuid=*/base::Uuid::GenerateRandomV4(), /*parent_uuid=*/kUuid,
      "Some title 2",
      /*url=*/"", /*is_folder=*/true, MakeRandomPosition()));

  base::HistogramTester histogram_tester;
  std::unique_ptr<SyncedBookmarkTracker> tracker =
      Merge(std::move(updates), &bookmark_model);
  const bookmarks::BookmarkNode* bookmark_bar_node =
      bookmark_model.bookmark_bar_node();
  ASSERT_THAT(bookmark_bar_node->children().size(), Eq(1u));
  histogram_tester.ExpectBucketCount(
      "Sync.BookmarksGUIDDuplicates",
      /*sample=*/ExpectedBookmarksUuidDuplicates::kDifferentFolders,
      /*expected_count=*/1);
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

  TestBookmarkModelView bookmark_model;

  // -------- The local model --------
  const bookmarks::BookmarkNode* bookmark_bar_node =
      bookmark_model.bookmark_bar_node();
  const bookmarks::BookmarkNode* folder = bookmark_model.AddFolder(
      /*parent=*/bookmark_bar_node, /*index=*/0,
      base::UTF8ToUTF16(kLocalTitle));
  ASSERT_TRUE(folder);

  // -------- The remote model --------
  syncer::UpdateResponseDataList updates;
  updates.push_back(CreateBookmarkBarNodeUpdateData());

  base::Uuid parent_uuid = BookmarkBarUuid();
  // Create a tree with depth |kRemoteUpdatesDepth| to verify the limit of
  // kMaxBookmarkTreeDepth is enforced.
  for (size_t i = 1; i < kRemoteUpdatesDepth; ++i) {
    base::Uuid folder_uuid = base::Uuid::GenerateRandomV4();
    updates.push_back(CreateUpdateResponseData(
        /*uuid=*/folder_uuid, /*parent_uuid=*/parent_uuid, kRemoteTitle,
        /*url=*/"",
        /*is_folder=*/true, MakeRandomPosition()));
    parent_uuid = folder_uuid;
  }

  ASSERT_THAT(updates.size(), Eq(kRemoteUpdatesDepth));

  std::unique_ptr<SyncedBookmarkTracker> tracker =
      SyncedBookmarkTracker::CreateEmpty(sync_pb::DataTypeState());
  testing::NiceMock<favicon::MockFaviconService> favicon_service;
  BookmarkModelMerger(std::move(updates), &bookmark_model, &favicon_service,
                      tracker.get())
      .Merge();

  // Check max depth hasn't been exceeded. Take into account root of the
  // tracker and bookmark bar.
  EXPECT_THAT(tracker->TrackedEntitiesCountForTest(),
              Eq(kMaxBookmarkTreeDepth + 2));
}

TEST(BookmarkModelMergerTest, ShouldReuploadBookmarkOnEmptyUniquePosition) {
  base::test::ScopedFeatureList override_features{
      switches::kSyncReuploadBookmarks};

  const std::string kFolder1Title = "folder1";
  const std::string kFolder2Title = "folder2";

  const base::Uuid kFolder1Uuid = base::Uuid::GenerateRandomV4();
  const base::Uuid kFolder2Uuid = base::Uuid::GenerateRandomV4();

  const syncer::UniquePosition::Suffix suffix =
      syncer::UniquePosition::RandomSuffix();
  const syncer::UniquePosition posFolder1 =
      syncer::UniquePosition::InitialPosition(suffix);
  const syncer::UniquePosition posFolder2 =
      syncer::UniquePosition::After(posFolder1, suffix);

  TestBookmarkModelView bookmark_model;

  // -------- The remote model --------
  syncer::UpdateResponseDataList updates;
  updates.push_back(CreateBookmarkBarNodeUpdateData());
  updates.push_back(CreateUpdateResponseData(
      /*uuid=*/kFolder1Uuid, /*parent_uuid=*/BookmarkBarUuid(), kFolder1Title,
      /*url=*/std::string(),
      /*is_folder=*/true, /*unique_position=*/posFolder1));

  // Mimic that the entity didn't have |unique_position| in specifics. This
  // entity should be reuploaded later.
  updates.back().entity.is_bookmark_unique_position_in_specifics_preprocessed =
      true;

  updates.push_back(CreateUpdateResponseData(
      /*uuid=*/kFolder2Uuid, /*parent_uuid=*/BookmarkBarUuid(), kFolder2Title,
      /*url=*/std::string(),
      /*is_folder=*/true, /*unique_position=*/posFolder2));

  base::HistogramTester histogram_tester;
  std::unique_ptr<SyncedBookmarkTracker> tracker =
      Merge(std::move(updates), &bookmark_model);

  ASSERT_THAT(tracker->GetEntityForUuid(kFolder1Uuid), NotNull());
  ASSERT_THAT(tracker->GetEntityForUuid(kFolder2Uuid), NotNull());

  EXPECT_TRUE(tracker->GetEntityForUuid(kFolder1Uuid)->IsUnsynced());
  EXPECT_FALSE(tracker->GetEntityForUuid(kFolder2Uuid)->IsUnsynced());

  EXPECT_THAT(histogram_tester.GetTotalSum(
                  "Sync.BookmarkModelMerger.UnsyncedEntitiesUponCompletion"),
              Eq(1));
}

TEST(BookmarkModelMergerTest, ShouldRemoveDifferentTypeDuplicatesByUuid) {
  const std::string kTitle = "Title";

  const base::Uuid kUuid = base::Uuid::GenerateRandomV4();

  // The remote model has 2 duplicates, a folder and a URL.
  //
  // -------- The remote model --------
  // bookmark_bar
  //  |- folder1(UUID)
  //    |- folder11
  //  |- URL1(UUID)
  TestBookmarkModelView bookmark_model;

  syncer::UpdateResponseDataList updates;
  updates.push_back(CreateBookmarkBarNodeUpdateData());

  updates.push_back(CreateUpdateResponseData(
      /*uuid=*/kUuid, /*parent_uuid=*/BookmarkBarUuid(), kTitle,
      /*url=*/"",
      /*is_folder=*/true, MakeRandomPosition()));
  updates.back().entity.id = "Id1";
  updates.push_back(CreateUpdateResponseData(
      /*uuid=*/base::Uuid::GenerateRandomV4(), /*parent_uuid=*/kUuid,
      "Some title",
      /*url=*/"", /*is_folder=*/true, MakeRandomPosition()));

  updates.push_back(CreateUpdateResponseData(
      /*uuid=*/kUuid, /*parent_uuid=*/BookmarkBarUuid(), kTitle,
      /*url=*/"http://url1.com", /*is_folder=*/false, MakeRandomPosition()));
  updates.back().entity.id = "Id2";

  base::HistogramTester histogram_tester;
  std::unique_ptr<SyncedBookmarkTracker> tracker =
      Merge(std::move(updates), &bookmark_model);
  const bookmarks::BookmarkNode* bookmark_bar_node =
      bookmark_model.bookmark_bar_node();
  ASSERT_THAT(bookmark_bar_node->children().size(), Eq(1u));
  histogram_tester.ExpectUniqueSample(
      "Sync.BookmarksGUIDDuplicates",
      /*sample=*/ExpectedBookmarksUuidDuplicates::kDifferentTypes,
      /*expected_bucket_count=*/1);
  EXPECT_THAT(tracker->GetEntityForSyncId("Id1"), NotNull());
  EXPECT_THAT(tracker->GetEntityForSyncId("Id2"), IsNull());
  EXPECT_EQ(bookmark_bar_node->children().front()->children().size(), 1u);
}

TEST(BookmarkModelMergerTest, ShouldReportTimeMetrics) {
  const std::string kTitle = "Title";
  TestBookmarkModelView bookmark_model;

  syncer::UpdateResponseDataList updates;
  updates.push_back(CreateBookmarkBarNodeUpdateData());

  // Create 10k+ bookmarks to verify reported metrics.
  for (size_t i = 0; i < 10001; ++i) {
    updates.push_back(CreateUpdateResponseData(
        /*uuid=*/base::Uuid::GenerateRandomV4(),
        /*parent_uuid=*/BookmarkBarUuid(), kTitle,
        /*url=*/"",
        /*is_folder=*/true, MakeRandomPosition()));
  }

  base::HistogramTester histogram_tester;
  std::unique_ptr<SyncedBookmarkTracker> tracker =
      Merge(std::move(updates), &bookmark_model);
  histogram_tester.ExpectTotalCount("Sync.BookmarkModelMergerTime", 1);
  histogram_tester.ExpectTotalCount("Sync.BookmarkModelMergerTime.10kUpdates",
                                    1);
  histogram_tester.ExpectTotalCount("Sync.BookmarkModelMergerTime.50kUpdates",
                                    0);
  histogram_tester.ExpectTotalCount("Sync.BookmarkModelMergerTime.100kUpdates",
                                    0);
}

}  // namespace sync_bookmarks
