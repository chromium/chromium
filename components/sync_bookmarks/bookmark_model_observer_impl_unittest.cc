// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "components/sync_bookmarks/bookmark_model_observer_impl.h"

#include <algorithm>
#include <array>
#include <list>
#include <map>
#include <memory>
#include <utility>
#include <vector>

#include "base/functional/callback_helpers.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "components/bookmarks/common/bookmark_metrics.h"
#include "components/bookmarks/test/test_bookmark_client.h"
#include "components/favicon_base/favicon_types.h"
#include "components/sync/base/features.h"
#include "components/sync/base/time.h"
#include "components/sync/base/unique_position.h"
#include "components/sync/protocol/bookmark_specifics.pb.h"
#include "components/sync/protocol/data_type_state.pb.h"
#include "components/sync/protocol/entity_metadata.pb.h"
#include "components/sync/protocol/entity_specifics.pb.h"
#include "components/sync_bookmarks/bookmark_model_view.h"
#include "components/sync_bookmarks/bookmark_specifics_conversions.h"
#include "components/sync_bookmarks/synced_bookmark_tracker.h"
#include "components/sync_bookmarks/synced_bookmark_tracker_entity.h"
#include "components/sync_bookmarks/test_bookmark_model_view.h"
#include "components/undo/bookmark_undo_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_unittest_util.h"

namespace sync_bookmarks {

namespace {

using testing::AtLeast;
using testing::ElementsAre;
using testing::Eq;
using testing::IsEmpty;
using testing::IsNull;
using testing::Ne;
using testing::NiceMock;
using testing::NotNull;
using testing::SizeIs;
using testing::UnorderedElementsAre;

const char kBookmarkBarId[] = "bookmark_bar_id";
const char kBookmarkBarTag[] = "bookmark_bar";
const char kOtherBookmarksId[] = "other_bookmarks_id";
const char kOtherBookmarksTag[] = "other_bookmarks";
const char kMobileBookmarksId[] = "synced_bookmarks_id";
const char kMobileBookmarksTag[] = "synced_bookmarks";

// Matches |arg| of type SyncedBookmarkTrackerEntity*.
MATCHER_P(HasBookmarkNode, node, "") {
  return arg->bookmark_node() == node;
}

// Returns a single-color 16x16 image using |color|.
gfx::Image CreateTestImage(SkColor color) {
  return gfx::test::CreateImage(/*size=*/16, color);
}

void AddPermanentFoldersToTracker(const BookmarkModelView* model,
                                  SyncedBookmarkTracker* tracker) {
  sync_pb::EntitySpecifics specifics;
  specifics.mutable_bookmark()->set_legacy_canonicalized_title(kBookmarkBarTag);
  tracker->Add(
      /*bookmark_node=*/model->bookmark_bar_node(),
      /*sync_id=*/kBookmarkBarId,
      /*server_version=*/0, /*creation_time=*/base::Time::Now(), specifics);
  specifics.mutable_bookmark()->set_legacy_canonicalized_title(
      kOtherBookmarksTag);
  tracker->Add(
      /*bookmark_node=*/model->other_node(),
      /*sync_id=*/kOtherBookmarksId,
      /*server_version=*/0, /*creation_time=*/base::Time::Now(), specifics);
  specifics.mutable_bookmark()->set_legacy_canonicalized_title(
      kMobileBookmarksTag);
  tracker->Add(
      /*bookmark_node=*/model->mobile_node(),
      /*sync_id=*/kMobileBookmarksId,
      /*server_version=*/0, /*creation_time=*/base::Time::Now(), specifics);
}

// TestBookmarkClient that supports undoing removals.
class TestBookmarkClientWithUndo : public bookmarks::TestBookmarkClient {
 public:
  explicit TestBookmarkClientWithUndo(BookmarkUndoService* undo_service)
      : undo_service_(undo_service) {}

  ~TestBookmarkClientWithUndo() override = default;

  // BookmarkClient overrides.
  void OnBookmarkNodeRemovedUndoable(
      const bookmarks::BookmarkNode* parent,
      size_t index,
      std::unique_ptr<bookmarks::BookmarkNode> node) override {
    undo_service_->AddUndoEntryForRemovedNode(parent, index, std::move(node));
  }

 private:
  const raw_ptr<BookmarkUndoService> undo_service_;
};

class BookmarkModelObserverImplTest
    : public testing::TestWithParam<TestBookmarkModelView::ViewType> {
 public:
  BookmarkModelObserverImplTest()
      : bookmark_tracker_(
            SyncedBookmarkTracker::CreateEmpty(sync_pb::DataTypeState())),
        bookmark_model_(
            GetParam(),
            std::make_unique<TestBookmarkClientWithUndo>(&undo_service_)),
        observer_(&bookmark_model_,
                  nudge_for_commit_closure_.Get(),
                  /*on_bookmark_model_being_deleted_closure=*/base::DoNothing(),
                  bookmark_tracker_.get()) {
    undo_service_.StartObservingBookmarkModel(
        bookmark_model_.underlying_model());
    bookmark_model_.EnsurePermanentNodesExist();
    AddPermanentFoldersToTracker(&bookmark_model_, bookmark_tracker_.get());
    bookmark_model_.AddObserver(&observer_);
  }

  ~BookmarkModelObserverImplTest() override {
    bookmark_model_.RemoveObserver(&observer_);
    bookmark_model_.underlying_model()->Shutdown();
    undo_service_.Shutdown();
  }

  void SimulateCommitResponseForAllLocalChanges() {
    for (const SyncedBookmarkTrackerEntity* entity :
         bookmark_tracker()->GetEntitiesWithLocalChanges()) {
      const std::string id = entity->metadata().server_id();
      // Don't simulate change in id for simplicity.
      bookmark_tracker()->UpdateUponCommitResponse(
          entity, id,
          /*server_version=*/1,
          /*acked_sequence_number=*/entity->metadata().sequence_number());
    }
  }

  syncer::UniquePosition PositionOf(
      const bookmarks::BookmarkNode* bookmark_node) {
    const SyncedBookmarkTrackerEntity* entity =
        bookmark_tracker()->GetEntityForBookmarkNode(bookmark_node);
    return syncer::UniquePosition::FromProto(
        entity->metadata().unique_position());
  }

  std::vector<const bookmarks::BookmarkNode*> GenerateBookmarkNodes(
      size_t num_bookmarks) {
    const std::string kTitle = "title";
    const std::string kUrl = "http://www.url.com";

    const bookmarks::BookmarkNode* bookmark_bar_node =
        bookmark_model()->bookmark_bar_node();
    std::vector<const bookmarks::BookmarkNode*> nodes;
    for (size_t i = 0; i < num_bookmarks; ++i) {
      nodes.push_back(bookmark_model()->AddURL(
          /*parent=*/bookmark_bar_node, /*index=*/i, base::UTF8ToUTF16(kTitle),
          GURL(kUrl)));
    }

    // Verify number of entities local changes. Should be the same as number of
    // new nodes.
    DCHECK_EQ(bookmark_tracker()->GetEntitiesWithLocalChanges().size(),
              num_bookmarks);

    // All bookmarks should be tracked now (including permanent nodes).
    DCHECK_EQ(bookmark_tracker()->TrackedEntitiesCountForTest(),
              3 + num_bookmarks);

    return nodes;
  }

  TestBookmarkModelView* bookmark_model() { return &bookmark_model_; }
  SyncedBookmarkTracker* bookmark_tracker() { return bookmark_tracker_.get(); }
  BookmarkModelObserverImpl* observer() { return &observer_; }
  base::MockCallback<base::RepeatingClosure>* nudge_for_commit_closure() {
    return &nudge_for_commit_closure_;
  }
  bookmarks::TestBookmarkClient* bookmark_client() {
    return bookmark_model_.underlying_client();
  }
  UndoManager* undo_manager() { return undo_service_.undo_manager(); }

 private:
  base::test::ScopedFeatureList features_{
      syncer::kSyncEnableBookmarksInTransportMode};
  NiceMock<base::MockCallback<base::RepeatingClosure>>
      nudge_for_commit_closure_;
  std::unique_ptr<SyncedBookmarkTracker> bookmark_tracker_;
  BookmarkUndoService undo_service_;
  TestBookmarkModelView bookmark_model_;
  BookmarkModelObserverImpl observer_;
};

TEST_P(BookmarkModelObserverImplTest,
       BookmarkAddedShouldPutInTheTrackerAndNudgeForCommit) {
  const std::string kTitle = "title";
  const std::string kUrl = "http://www.url.com";

  EXPECT_CALL(*nudge_for_commit_closure(), Run());
  const bookmarks::BookmarkNode* bookmark_bar_node =
      bookmark_model()->bookmark_bar_node();
  const bookmarks::BookmarkNode* bookmark_node = bookmark_model()->AddURL(
      /*parent=*/bookmark_bar_node, /*index=*/0, base::UTF8ToUTF16(kTitle),
      GURL(kUrl));

  EXPECT_THAT(bookmark_tracker()->TrackedEntitiesCountForTest(), 4U);

  std::vector<const SyncedBookmarkTrackerEntity*> local_changes =
      bookmark_tracker()->GetEntitiesWithLocalChanges();
  ASSERT_THAT(local_changes.size(), 1U);
  EXPECT_THAT(local_changes[0]->bookmark_node(), Eq(bookmark_node));
  EXPECT_THAT(local_changes[0]->metadata().server_id(),
              Eq(bookmark_node->uuid().AsLowercaseString()));
}

TEST_P(BookmarkModelObserverImplTest,
       BookmarkChangedShouldUpdateTheTrackerAndNudgeForCommit) {
  const std::string kTitle1 = "title1";
  const std::string kUrl1 = "http://www.url1.com";
  const std::string kNewUrl1 = "http://www.new-url1.com";
  const std::string kTitle2 = "title2";
  const std::string kUrl2 = "http://www.url2.com";
  const std::string kNewTitle2 = "new_title2";

  const bookmarks::BookmarkNode* bookmark_bar_node =
      bookmark_model()->bookmark_bar_node();
  const bookmarks::BookmarkNode* bookmark_node1 = bookmark_model()->AddURL(
      /*parent=*/bookmark_bar_node, /*index=*/0, base::UTF8ToUTF16(kTitle1),
      GURL(kUrl1));
  const bookmarks::BookmarkNode* bookmark_node2 = bookmark_model()->AddURL(
      /*parent=*/bookmark_bar_node, /*index=*/0, base::UTF8ToUTF16(kTitle2),
      GURL(kUrl2));
  // Both bookmarks should be tracked now.
  ASSERT_THAT(bookmark_tracker()->TrackedEntitiesCountForTest(), 5U);
  // There should be two local changes now for both entities.
  ASSERT_THAT(bookmark_tracker()->GetEntitiesWithLocalChanges().size(), 2U);

  SimulateCommitResponseForAllLocalChanges();

  // There should be no local changes now.
  ASSERT_TRUE(bookmark_tracker()->GetEntitiesWithLocalChanges().empty());

  // Now update the title of the 2nd node.
  EXPECT_CALL(*nudge_for_commit_closure(), Run());
  bookmark_model()->SetTitle(bookmark_node2, base::UTF8ToUTF16(kNewTitle2));
  // Node 2 should be in the local changes list.
  EXPECT_THAT(bookmark_tracker()->GetEntitiesWithLocalChanges(),
              ElementsAre(HasBookmarkNode(bookmark_node2)));

  // Now update the url of the 1st node.
  EXPECT_CALL(*nudge_for_commit_closure(), Run());
  bookmark_model()->SetURL(bookmark_node1, GURL(kNewUrl1));

  // Node 1 and 2 should be in the local changes list.
  EXPECT_THAT(bookmark_tracker()->GetEntitiesWithLocalChanges(),
              UnorderedElementsAre(HasBookmarkNode(bookmark_node1),
                                   HasBookmarkNode(bookmark_node2)));

  // Now update metainfo of the 1st node.
  EXPECT_CALL(*nudge_for_commit_closure(), Run());
  bookmark_model()->underlying_model()->SetNodeMetaInfo(bookmark_node1, "key",
                                                        "value");
}

TEST_P(BookmarkModelObserverImplTest,
       BookmarkMovedShouldUpdateTheTrackerAndNudgeForCommit) {
  // Build this structure:
  // bookmark_bar
  //  |- folder1
  //      |- bookmark1
  const GURL kUrl("http://www.url1.com");

  const bookmarks::BookmarkNode* bookmark_bar_node =
      bookmark_model()->bookmark_bar_node();
  const bookmarks::BookmarkNode* folder1_node = bookmark_model()->AddFolder(
      /*parent=*/bookmark_bar_node, /*index=*/0, u"folder1");
  const bookmarks::BookmarkNode* bookmark1_node = bookmark_model()->AddURL(
      /*parent=*/folder1_node, /*index=*/0, u"bookmark1", kUrl);

  // Verify number of entities local changes. Should be the same as number of
  // new nodes.
  ASSERT_THAT(bookmark_tracker()->GetEntitiesWithLocalChanges().size(), 2U);

  // All bookmarks should be tracked now.
  ASSERT_THAT(bookmark_tracker()->TrackedEntitiesCountForTest(), 5U);

  SimulateCommitResponseForAllLocalChanges();

  // There should be no local changes now.
  ASSERT_TRUE(bookmark_tracker()->GetEntitiesWithLocalChanges().empty());

  // Now change it to this structure.
  // Build this structure:
  // bookmark_bar
  //  |- bookmark1
  //  |- folder1

  EXPECT_CALL(*nudge_for_commit_closure(), Run());
  bookmark_model()->Move(bookmark1_node, bookmark_bar_node, 0);
  EXPECT_TRUE(PositionOf(bookmark1_node).LessThan(PositionOf(folder1_node)));
}

TEST_P(BookmarkModelObserverImplTest,
       BookmarkMovedThatBecameUnsyncableShouldIssueTombstone) {
  auto client = std::make_unique<bookmarks::TestBookmarkClient>();
  bookmarks::BookmarkNode* managed_node = client->EnableManagedNode();
  TestBookmarkModelView model(GetParam(), std::move(client));
  model.EnsurePermanentNodesExist();

  // Build this structure:
  // bookmark_bar
  //  |- folder1
  const bookmarks::BookmarkNode* bookmark_bar_node = model.bookmark_bar_node();
  const bookmarks::BookmarkNode* folder_node = model.AddFolder(
      /*parent=*/bookmark_bar_node, /*index=*/0, u"folder1");
  const syncer::ClientTagHash folder_client_tag_hash =
      SyncedBookmarkTracker::GetClientTagHashFromUuid(folder_node->uuid());

  // Build a tracker that already tracks all nodes.
  std::unique_ptr<SyncedBookmarkTracker> bookmark_tracker =
      SyncedBookmarkTracker::CreateEmpty(sync_pb::DataTypeState());
  AddPermanentFoldersToTracker(&model, bookmark_tracker.get());
  bookmark_tracker->Add(
      /*bookmark_node=*/folder_node,
      /*sync_id=*/"folder_sync_id",
      /*server_version=*/0, /*creation_time=*/base::Time::Now(),
      CreateSpecificsFromBookmarkNode(
          folder_node, &model,
          syncer::UniquePosition::InitialPosition(
              syncer::UniquePosition::RandomSuffix())
              .ToProto(),
          /*force_favicon_load=*/false));
  bookmark_tracker->CheckAllNodesTracked(&model);

  BookmarkModelObserverImpl observer(
      &model, nudge_for_commit_closure()->Get(),
      /*on_bookmark_model_being_deleted_closure=*/base::DoNothing(),
      bookmark_tracker.get());
  model.AddObserver(&observer);

  ASSERT_TRUE(model.IsNodeSyncable(folder_node));
  ASSERT_THAT(bookmark_tracker->TrackedEntitiesCountForTest(), 4U);
  ASSERT_THAT(
      bookmark_tracker->GetEntityForClientTagHash(folder_client_tag_hash),
      NotNull());

  // Mimic the folder becoming unsyncable by moving it under the managed node.
  // This isn't very realistic but is good enough for unit-testing.
  EXPECT_CALL(*nudge_for_commit_closure(), Run());
  model.Move(folder_node, managed_node, /*index=*/0);
  ASSERT_FALSE(model.IsNodeSyncable(folder_node));

  const SyncedBookmarkTrackerEntity* folder_entity =
      bookmark_tracker->GetEntityForClientTagHash(folder_client_tag_hash);
  ASSERT_THAT(folder_entity, NotNull());

  // A tombstone should be tracked.
  EXPECT_TRUE(folder_entity->IsUnsynced());
  EXPECT_TRUE(folder_entity->metadata().is_deleted());
  EXPECT_THAT(folder_entity->bookmark_node(), IsNull());
  EXPECT_THAT(bookmark_tracker->GetEntityForBookmarkNode(folder_node),
              IsNull());

  model.RemoveObserver(&observer);
}

TEST_P(BookmarkModelObserverImplTest,
       BookmarkMovedThatBecameSyncableShouldIssueCreation) {
  auto client = std::make_unique<bookmarks::TestBookmarkClient>();
  bookmarks::BookmarkNode* managed_node = client->EnableManagedNode();
  TestBookmarkModelView model(GetParam(), std::move(client));
  model.EnsurePermanentNodesExist();

  // Add one managed folder, which is considered unsyncable.
  const bookmarks::BookmarkNode* folder_node = model.AddFolder(
      /*parent=*/managed_node, /*index=*/0, u"folder1");
  const syncer::ClientTagHash folder_client_tag_hash =
      SyncedBookmarkTracker::GetClientTagHashFromUuid(folder_node->uuid());

  // Build a tracker that already tracks all nodes.
  std::unique_ptr<SyncedBookmarkTracker> bookmark_tracker =
      SyncedBookmarkTracker::CreateEmpty(sync_pb::DataTypeState());
  AddPermanentFoldersToTracker(&model, bookmark_tracker.get());

  BookmarkModelObserverImpl observer(
      &model, nudge_for_commit_closure()->Get(),
      /*on_bookmark_model_being_deleted_closure=*/base::DoNothing(),
      bookmark_tracker.get());
  model.AddObserver(&observer);

  ASSERT_FALSE(model.IsNodeSyncable(folder_node));
  EXPECT_THAT(bookmark_tracker->GetEntityForBookmarkNode(folder_node),
              IsNull());
  ASSERT_THAT(
      bookmark_tracker->GetEntityForClientTagHash(folder_client_tag_hash),
      IsNull());
  ASSERT_THAT(bookmark_tracker->TrackedEntitiesCountForTest(), 3U);

  // Mimic the folder becoming syncable by moving it from the managed node to
  // the bookmark bar. This isn't very realistic but is good enough for
  // unit-testing.
  EXPECT_CALL(*nudge_for_commit_closure(), Run());
  model.Move(folder_node, model.bookmark_bar_node(), /*index=*/0);
  ASSERT_TRUE(model.IsNodeSyncable(folder_node));

  const SyncedBookmarkTrackerEntity* folder_entity =
      bookmark_tracker->GetEntityForClientTagHash(folder_client_tag_hash);
  ASSERT_THAT(folder_entity, NotNull());
  EXPECT_THAT(bookmark_tracker->GetEntityForBookmarkNode(folder_node),
              Eq(folder_entity));
  EXPECT_THAT(bookmark_tracker->TrackedEntitiesCountForTest(), 4U);

  // A pending creation should be tracked.
  EXPECT_TRUE(folder_entity->IsUnsynced());
  EXPECT_FALSE(folder_entity->metadata().is_deleted());
  EXPECT_THAT(folder_entity->bookmark_node(), Eq(folder_node));

  model.RemoveObserver(&observer);
}

TEST_P(BookmarkModelObserverImplTest,
       BookmarkFolderMovedWithChildrenThatBecameSyncableShouldIssueCreations) {
  auto client = std::make_unique<bookmarks::TestBookmarkClient>();
  bookmarks::BookmarkNode* managed_node = client->EnableManagedNode();
  TestBookmarkModelView model(GetParam(), std::move(client));
  model.EnsurePermanentNodesExist();

  // Add one managed folder, which is considered unsyncable.
  const bookmarks::BookmarkNode* folder_node = model.AddFolder(
      /*parent=*/managed_node, /*index=*/0, u"folder1");

  // Add two children to the unsyncable folder.
  const bookmarks::BookmarkNode* bookmark1_node = model.AddURL(
      /*parent=*/folder_node, /*index=*/0, u"bookmark1",
      GURL("http://url1.com"));
  const bookmarks::BookmarkNode* bookmark2_node = model.AddURL(
      /*parent=*/folder_node, /*index=*/1, u"bookmark2",
      GURL("http://url2.com"));

  const syncer::ClientTagHash folder_client_tag_hash =
      SyncedBookmarkTracker::GetClientTagHashFromUuid(folder_node->uuid());
  const syncer::ClientTagHash bookmark1_client_tag_hash =
      SyncedBookmarkTracker::GetClientTagHashFromUuid(bookmark1_node->uuid());
  const syncer::ClientTagHash bookmark2_client_tag_hash =
      SyncedBookmarkTracker::GetClientTagHashFromUuid(bookmark2_node->uuid());

  // Build a tracker that already tracks all syncable nodes (i.e. permanent
  // nodes only).
  std::unique_ptr<SyncedBookmarkTracker> bookmark_tracker =
      SyncedBookmarkTracker::CreateEmpty(sync_pb::DataTypeState());
  AddPermanentFoldersToTracker(&model, bookmark_tracker.get());

  BookmarkModelObserverImpl observer(
      &model, nudge_for_commit_closure()->Get(),
      /*on_bookmark_model_being_deleted_closure=*/base::DoNothing(),
      bookmark_tracker.get());
  model.AddObserver(&observer);

  ASSERT_FALSE(model.IsNodeSyncable(folder_node));
  ASSERT_FALSE(model.IsNodeSyncable(bookmark1_node));
  ASSERT_FALSE(model.IsNodeSyncable(bookmark2_node));
  ASSERT_THAT(bookmark_tracker->GetEntityForBookmarkNode(folder_node),
              IsNull());
  ASSERT_THAT(bookmark_tracker->GetEntityForBookmarkNode(bookmark1_node),
              IsNull());
  ASSERT_THAT(bookmark_tracker->GetEntityForBookmarkNode(bookmark2_node),
              IsNull());
  ASSERT_THAT(
      bookmark_tracker->GetEntityForClientTagHash(folder_client_tag_hash),
      IsNull());
  ASSERT_THAT(
      bookmark_tracker->GetEntityForClientTagHash(bookmark1_client_tag_hash),
      IsNull());
  ASSERT_THAT(
      bookmark_tracker->GetEntityForClientTagHash(bookmark2_client_tag_hash),
      IsNull());
  ASSERT_THAT(bookmark_tracker->TrackedEntitiesCountForTest(), 3U);

  // Mimic the folder becoming syncable by moving it from the managed node to
  // the bookmark bar. This isn't very realistic but is good enough for
  // unit-testing.
  EXPECT_CALL(*nudge_for_commit_closure(), Run()).Times(AtLeast(1));
  model.Move(folder_node, model.bookmark_bar_node(), /*index=*/0);
  ASSERT_TRUE(model.IsNodeSyncable(folder_node));
  ASSERT_TRUE(model.IsNodeSyncable(bookmark1_node));
  ASSERT_TRUE(model.IsNodeSyncable(bookmark2_node));

  EXPECT_THAT(bookmark_tracker->TrackedEntitiesCountForTest(), 6U);

  const SyncedBookmarkTrackerEntity* folder_entity =
      bookmark_tracker->GetEntityForClientTagHash(folder_client_tag_hash);
  ASSERT_THAT(folder_entity, NotNull());
  EXPECT_THAT(bookmark_tracker->GetEntityForBookmarkNode(folder_node),
              Eq(folder_entity));

  const SyncedBookmarkTrackerEntity* bookmark1_entity =
      bookmark_tracker->GetEntityForClientTagHash(bookmark1_client_tag_hash);
  ASSERT_THAT(bookmark1_entity, NotNull());
  EXPECT_THAT(bookmark_tracker->GetEntityForBookmarkNode(bookmark1_node),
              Eq(bookmark1_entity));

  const SyncedBookmarkTrackerEntity* bookmark2_entity =
      bookmark_tracker->GetEntityForClientTagHash(bookmark2_client_tag_hash);
  ASSERT_THAT(bookmark2_entity, NotNull());
  EXPECT_THAT(bookmark_tracker->GetEntityForBookmarkNode(bookmark2_node),
              Eq(bookmark2_entity));

  // Three pending creations should be tracked.
  EXPECT_TRUE(folder_entity->IsUnsynced());
  EXPECT_FALSE(folder_entity->metadata().is_deleted());
  EXPECT_THAT(folder_entity->bookmark_node(), Eq(folder_node));
  EXPECT_TRUE(bookmark1_entity->IsUnsynced());
  EXPECT_FALSE(bookmark1_entity->metadata().is_deleted());
  EXPECT_THAT(bookmark1_entity->bookmark_node(), Eq(bookmark1_node));
  EXPECT_TRUE(bookmark2_entity->IsUnsynced());
  EXPECT_FALSE(bookmark2_entity->metadata().is_deleted());
  EXPECT_THAT(bookmark2_entity->bookmark_node(), Eq(bookmark2_node));

  model.RemoveObserver(&observer);
}

TEST_P(BookmarkModelObserverImplTest,
       ReorderChildrenShouldUpdateTheTrackerAndNudgeForCommit) {
  std::vector<const bookmarks::BookmarkNode*> nodes =
      GenerateBookmarkNodes(/*num_bookmarks=*/4);

  SimulateCommitResponseForAllLocalChanges();

  // Reorder it to be (2 bookmarks have been moved):
  // bookmark_bar
  //  |- node1
  //  |- node3
  //  |- node0
  //  |- node2
  bookmark_model()->ReorderChildren(bookmark_model()->bookmark_bar_node(),
                                    {nodes[1], nodes[3], nodes[0], nodes[2]});
  EXPECT_TRUE(PositionOf(nodes[1]).LessThan(PositionOf(nodes[3])));
  EXPECT_TRUE(PositionOf(nodes[3]).LessThan(PositionOf(nodes[0])));
  EXPECT_TRUE(PositionOf(nodes[0]).LessThan(PositionOf(nodes[2])));

  // Only 2 moved nodes should have local changes to commit.
  EXPECT_THAT(bookmark_tracker()->GetEntitiesWithLocalChanges(), SizeIs(2));
}

TEST_P(BookmarkModelObserverImplTest,
       ShouldReorderChildrenAndUpdateOnlyMovedToRightBookmark) {
  std::vector<const bookmarks::BookmarkNode*> nodes =
      GenerateBookmarkNodes(/*num_bookmarks=*/4);

  SimulateCommitResponseForAllLocalChanges();

  // Reorder it to be:
  // bookmark_bar
  //  |- node1
  //  |- node2
  //  |- node0 (moved)
  //  |- node3
  bookmark_model()->ReorderChildren(bookmark_model()->bookmark_bar_node(),
                                    {nodes[1], nodes[2], nodes[0], nodes[3]});
  EXPECT_TRUE(PositionOf(nodes[1]).LessThan(PositionOf(nodes[2])));
  EXPECT_TRUE(PositionOf(nodes[2]).LessThan(PositionOf(nodes[0])));
  EXPECT_TRUE(PositionOf(nodes[0]).LessThan(PositionOf(nodes[3])));

  // Only one moved node should have local changes to commit.
  EXPECT_THAT(bookmark_tracker()->GetEntitiesWithLocalChanges(),
              UnorderedElementsAre(HasBookmarkNode(nodes[0])));
}

TEST_P(BookmarkModelObserverImplTest,
       ShouldReorderChildrenAndUpdateOnlyMovedToLeftBookmark) {
  std::vector<const bookmarks::BookmarkNode*> nodes =
      GenerateBookmarkNodes(/*num_bookmarks=*/4);

  SimulateCommitResponseForAllLocalChanges();

  // Reorder it to be:
  // bookmark_bar
  //  |- node0
  //  |- node3 (moved)
  //  |- node1
  //  |- node2
  bookmark_model()->ReorderChildren(bookmark_model()->bookmark_bar_node(),
                                    {nodes[0], nodes[3], nodes[1], nodes[2]});
  EXPECT_TRUE(PositionOf(nodes[0]).LessThan(PositionOf(nodes[3])));
  EXPECT_TRUE(PositionOf(nodes[3]).LessThan(PositionOf(nodes[1])));
  EXPECT_TRUE(PositionOf(nodes[1]).LessThan(PositionOf(nodes[2])));

  // Only one moved node should have local changes to commit.
  EXPECT_THAT(bookmark_tracker()->GetEntitiesWithLocalChanges(),
              UnorderedElementsAre(HasBookmarkNode(nodes[3])));
}

TEST_P(BookmarkModelObserverImplTest,
       ShouldReorderWhenBookmarkMovedToLastPosition) {
  std::vector<const bookmarks::BookmarkNode*> nodes =
      GenerateBookmarkNodes(/*num_bookmarks=*/4);

  SimulateCommitResponseForAllLocalChanges();

  // Reorder it to be:
  // bookmark_bar
  //  |- node1
  //  |- node2
  //  |- node3
  //  |- node0 (moved)
  bookmark_model()->ReorderChildren(bookmark_model()->bookmark_bar_node(),
                                    {nodes[1], nodes[2], nodes[3], nodes[0]});
  EXPECT_TRUE(PositionOf(nodes[1]).LessThan(PositionOf(nodes[2])));
  EXPECT_TRUE(PositionOf(nodes[2]).LessThan(PositionOf(nodes[3])));
  EXPECT_TRUE(PositionOf(nodes[3]).LessThan(PositionOf(nodes[0])));

  // Only one moved node should have local changes to commit.
  EXPECT_THAT(bookmark_tracker()->GetEntitiesWithLocalChanges(),
              UnorderedElementsAre(HasBookmarkNode(nodes[0])));
}

TEST_P(BookmarkModelObserverImplTest,
       ShouldReorderWhenBookmarkMovedToFirstPosition) {
  std::vector<const bookmarks::BookmarkNode*> nodes =
      GenerateBookmarkNodes(/*num_bookmarks=*/4);

  SimulateCommitResponseForAllLocalChanges();

  // Reorder it to be:
  // bookmark_bar
  //  |- node3 (moved)
  //  |- node0
  //  |- node1
  //  |- node2
  bookmark_model()->ReorderChildren(bookmark_model()->bookmark_bar_node(),
                                    {nodes[3], nodes[0], nodes[1], nodes[2]});
  EXPECT_TRUE(PositionOf(nodes[3]).LessThan(PositionOf(nodes[0])));
  EXPECT_TRUE(PositionOf(nodes[0]).LessThan(PositionOf(nodes[1])));
  EXPECT_TRUE(PositionOf(nodes[1]).LessThan(PositionOf(nodes[2])));

  // Only one moved node should have local changes to commit.
  EXPECT_THAT(bookmark_tracker()->GetEntitiesWithLocalChanges(),
              UnorderedElementsAre(HasBookmarkNode(nodes[3])));
}

TEST_P(BookmarkModelObserverImplTest, ShouldReorderWhenAllBookmarksReversed) {
  // In this case almost all the bookmarks should be updated apart from only one
  // bookmark.
  std::vector<const bookmarks::BookmarkNode*> nodes =
      GenerateBookmarkNodes(/*num_bookmarks=*/4);

  SimulateCommitResponseForAllLocalChanges();

  // Reorder it to be (all nodes are moved):
  // bookmark_bar
  //  |- node3
  //  |- node2
  //  |- node1
  //  |- node0
  bookmark_model()->ReorderChildren(bookmark_model()->bookmark_bar_node(),
                                    {nodes[3], nodes[2], nodes[1], nodes[0]});
  EXPECT_TRUE(PositionOf(nodes[3]).LessThan(PositionOf(nodes[2])));
  EXPECT_TRUE(PositionOf(nodes[2]).LessThan(PositionOf(nodes[1])));
  EXPECT_TRUE(PositionOf(nodes[1]).LessThan(PositionOf(nodes[0])));

  // Do not verify which nodes exactly have been updated, it depends on the
  // implementation and any of the nodes may become a base node to calculate
  // relative positions of all other nodes.
  EXPECT_THAT(bookmark_tracker()->GetEntitiesWithLocalChanges(), SizeIs(3));
}

TEST_P(BookmarkModelObserverImplTest,
       ShouldNotReorderIfAllBookmarksStillOrdered) {
  std::vector<const bookmarks::BookmarkNode*> nodes =
      GenerateBookmarkNodes(/*num_bookmarks=*/4);

  SimulateCommitResponseForAllLocalChanges();

  // Keep the original order.
  bookmark_model()->ReorderChildren(bookmark_model()->bookmark_bar_node(),
                                    {nodes[0], nodes[1], nodes[2], nodes[3]});
  EXPECT_TRUE(PositionOf(nodes[0]).LessThan(PositionOf(nodes[1])));
  EXPECT_TRUE(PositionOf(nodes[1]).LessThan(PositionOf(nodes[2])));
  EXPECT_TRUE(PositionOf(nodes[2]).LessThan(PositionOf(nodes[3])));

  // The bookmarks remain in the same order, nothing to commit.
  EXPECT_THAT(bookmark_tracker()->GetEntitiesWithLocalChanges(), IsEmpty());
}

TEST_P(BookmarkModelObserverImplTest,
       BookmarkRemovalShouldUpdateTheTrackerAndNudgeForCommit) {
  // Build this structure:
  // bookmark_bar
  //  |- folder1
  //      |- bookmark1
  //      |- folder2
  //          |- bookmark2
  //          |- bookmark3

  // and then delete folder2.
  const GURL kUrl("http://www.url1.com");

  const bookmarks::BookmarkNode* bookmark_bar_node =
      bookmark_model()->bookmark_bar_node();
  const bookmarks::BookmarkNode* folder1_node = bookmark_model()->AddFolder(
      /*parent=*/bookmark_bar_node, /*index=*/0, u"folder1");
  const bookmarks::BookmarkNode* bookmark1_node = bookmark_model()->AddURL(
      /*parent=*/folder1_node, /*index=*/0, u"bookmark1", kUrl);
  const bookmarks::BookmarkNode* folder2_node = bookmark_model()->AddFolder(
      /*parent=*/folder1_node, /*index=*/1, u"folder2");
  const bookmarks::BookmarkNode* bookmark2_node = bookmark_model()->AddURL(
      /*parent=*/folder2_node, /*index=*/0, u"bookmark2", kUrl);
  const bookmarks::BookmarkNode* bookmark3_node = bookmark_model()->AddURL(
      /*parent=*/folder2_node, /*index=*/1, u"bookmark3", kUrl);

  // All bookmarks should be tracked now.
  ASSERT_THAT(bookmark_tracker()->TrackedEntitiesCountForTest(), 8U);

  SimulateCommitResponseForAllLocalChanges();

  // There should be no local changes now.
  ASSERT_TRUE(bookmark_tracker()->GetEntitiesWithLocalChanges().empty());

  const SyncedBookmarkTrackerEntity* folder2_entity =
      bookmark_tracker()->GetEntityForBookmarkNode(folder2_node);
  const SyncedBookmarkTrackerEntity* bookmark2_entity =
      bookmark_tracker()->GetEntityForBookmarkNode(bookmark2_node);
  const SyncedBookmarkTrackerEntity* bookmark3_entity =
      bookmark_tracker()->GetEntityForBookmarkNode(bookmark3_node);

  ASSERT_FALSE(folder2_entity->metadata().is_deleted());
  ASSERT_FALSE(bookmark2_entity->metadata().is_deleted());
  ASSERT_FALSE(bookmark3_entity->metadata().is_deleted());

  const std::string& folder2_entity_id = folder2_entity->metadata().server_id();
  const std::string& bookmark2_entity_id =
      bookmark2_entity->metadata().server_id();
  const std::string& bookmark3_entity_id =
      bookmark3_entity->metadata().server_id();
  // Delete folder2.
  EXPECT_CALL(*nudge_for_commit_closure(), Run());
  bookmark_model()->Remove(folder2_node, FROM_HERE);

  // folder2, bookmark2, and bookmark3 should be marked deleted.
  EXPECT_TRUE(bookmark_tracker()
                  ->GetEntityForSyncId(folder2_entity_id)
                  ->metadata()
                  .is_deleted());
  EXPECT_TRUE(bookmark_tracker()
                  ->GetEntityForSyncId(bookmark2_entity_id)
                  ->metadata()
                  .is_deleted());
  EXPECT_TRUE(bookmark_tracker()
                  ->GetEntityForSyncId(bookmark3_entity_id)
                  ->metadata()
                  .is_deleted());

  // folder2, bookmark2, and bookmark3 should be in the local changes to be
  // committed and folder2 deletion should be the last one (after all children
  // deletions).
  EXPECT_THAT(
      bookmark_tracker()->GetEntitiesWithLocalChanges(),
      ElementsAre(bookmark_tracker()->GetEntityForSyncId(bookmark2_entity_id),
                  bookmark_tracker()->GetEntityForSyncId(bookmark3_entity_id),
                  bookmark_tracker()->GetEntityForSyncId(folder2_entity_id)));

  // folder1 and bookmark1 are still tracked.
  EXPECT_TRUE(bookmark_tracker()->GetEntityForBookmarkNode(folder1_node));
  EXPECT_TRUE(bookmark_tracker()->GetEntityForBookmarkNode(bookmark1_node));
}

TEST_P(BookmarkModelObserverImplTest,
       BookmarkCreationAndRemovalShouldRequireTwoCommitResponsesBeforeRemoval) {
  const bookmarks::BookmarkNode* bookmark_bar_node =
      bookmark_model()->bookmark_bar_node();
  const bookmarks::BookmarkNode* folder_node = bookmark_model()->AddFolder(
      /*parent=*/bookmark_bar_node, /*index=*/0, u"folder");

  // Node should be tracked now.
  ASSERT_THAT(bookmark_tracker()->TrackedEntitiesCountForTest(), 4U);
  const SyncedBookmarkTrackerEntity* entity =
      bookmark_tracker()->GetEntityForBookmarkNode(folder_node);
  const std::string id = entity->metadata().server_id();
  ASSERT_THAT(bookmark_tracker()->GetEntitiesWithLocalChanges().size(), 1U);

  bookmark_tracker()->MarkCommitMayHaveStarted(entity);

  // Remove the folder.
  bookmark_model()->Remove(folder_node, FROM_HERE);

  // Simulate a commit response for the first commit request (the creation).
  // Don't simulate change in id for simplicity.
  bookmark_tracker()->UpdateUponCommitResponse(entity, id,
                                               /*server_version=*/1,
                                               /*acked_sequence_number=*/1);

  // There should still be one local change (the deletion).
  EXPECT_THAT(bookmark_tracker()->GetEntitiesWithLocalChanges().size(), 1U);

  // Entity is still tracked.
  EXPECT_THAT(bookmark_tracker()->TrackedEntitiesCountForTest(), 4U);

  // Commit the deletion.
  bookmark_tracker()->UpdateUponCommitResponse(entity, id,
                                               /*server_version=*/2,
                                               /*acked_sequence_number=*/2);
  // Entity should have been dropped.
  EXPECT_THAT(bookmark_tracker()->TrackedEntitiesCountForTest(), 3U);
}

TEST_P(BookmarkModelObserverImplTest,
       BookmarkCreationAndRemovalBeforeCommitRequestShouldBeRemovedDirectly) {
  const bookmarks::BookmarkNode* bookmark_bar_node =
      bookmark_model()->bookmark_bar_node();
  const bookmarks::BookmarkNode* folder_node = bookmark_model()->AddFolder(
      /*parent=*/bookmark_bar_node, /*index=*/0, u"folder");

  // Node should be tracked now.
  ASSERT_THAT(bookmark_tracker()->TrackedEntitiesCountForTest(), 4U);
  const std::string id = bookmark_tracker()
                             ->GetEntityForBookmarkNode(folder_node)
                             ->metadata()
                             .server_id();
  ASSERT_THAT(bookmark_tracker()->GetEntitiesWithLocalChanges().size(), 1U);

  // Remove the folder.
  bookmark_model()->Remove(folder_node, FROM_HERE);

  // Entity should have been dropped.
  EXPECT_THAT(bookmark_tracker()->TrackedEntitiesCountForTest(), 3U);
}

TEST_P(BookmarkModelObserverImplTest, ShouldPositionSiblings) {
  const std::string kTitle = "title";
  const std::string kUrl = "http://www.url.com";

  // Build this structure:
  // bookmark_bar
  //  |- node1
  //  |- node2
  // Expectation:
  //  p1 < p2

  const bookmarks::BookmarkNode* bookmark_bar_node =
      bookmark_model()->bookmark_bar_node();
  const bookmarks::BookmarkNode* bookmark_node1 = bookmark_model()->AddURL(
      /*parent=*/bookmark_bar_node, /*index=*/0, base::UTF8ToUTF16(kTitle),
      GURL(kUrl));

  const bookmarks::BookmarkNode* bookmark_node2 = bookmark_model()->AddURL(
      /*parent=*/bookmark_bar_node, /*index=*/1, base::UTF8ToUTF16(kTitle),
      GURL(kUrl));

  EXPECT_TRUE(PositionOf(bookmark_node1).LessThan(PositionOf(bookmark_node2)));

  // Now insert node3 at index 1 to build this structure:
  // bookmark_bar
  //  |- node1
  //  |- node3
  //  |- node2
  // Expectation:
  //  p1 < p2 (still holds)
  //  p1 < p3
  //  p3 < p2

  const bookmarks::BookmarkNode* bookmark_node3 = bookmark_model()->AddURL(
      /*parent=*/bookmark_bar_node, /*index=*/1, base::UTF8ToUTF16(kTitle),
      GURL(kUrl));
  EXPECT_THAT(bookmark_tracker()->TrackedEntitiesCountForTest(), Eq(6U));

  EXPECT_TRUE(PositionOf(bookmark_node1).LessThan(PositionOf(bookmark_node2)));
  EXPECT_TRUE(PositionOf(bookmark_node1).LessThan(PositionOf(bookmark_node3)));
  EXPECT_TRUE(PositionOf(bookmark_node3).LessThan(PositionOf(bookmark_node2)));
}

TEST_P(BookmarkModelObserverImplTest, ShouldNotSyncUnsyncableBookmarks) {
  auto client = std::make_unique<bookmarks::TestBookmarkClient>();
  bookmarks::BookmarkNode* managed_node = client->EnableManagedNode();
  TestBookmarkModelView model(GetParam(), std::move(client));
  model.EnsurePermanentNodesExist();

  std::unique_ptr<SyncedBookmarkTracker> bookmark_tracker =
      SyncedBookmarkTracker::CreateEmpty(sync_pb::DataTypeState());
  AddPermanentFoldersToTracker(&model, bookmark_tracker.get());

  BookmarkModelObserverImpl observer(
      &model, nudge_for_commit_closure()->Get(),
      /*on_bookmark_model_being_deleted_closure=*/base::DoNothing(),
      bookmark_tracker.get());
  model.AddObserver(&observer);

  EXPECT_CALL(*nudge_for_commit_closure(), Run()).Times(0);
  // In the TestBookmarkClient, descendants of managed nodes shouldn't be
  // synced.
  const bookmarks::BookmarkNode* unsyncable_node =
      model.AddURL(/*parent=*/managed_node, /*index=*/0, u"Title",
                   GURL("http://www.url.com"));
  // Only permanent folders should be tracked.
  EXPECT_THAT(bookmark_tracker->TrackedEntitiesCountForTest(), 3U);

  EXPECT_CALL(*nudge_for_commit_closure(), Run()).Times(0);
  // In the TestBookmarkClient, descendants of managed nodes shouldn't be
  // synced.
  model.SetTitle(unsyncable_node, u"NewTitle");
  // Only permanent folders should be tracked.
  EXPECT_THAT(bookmark_tracker->TrackedEntitiesCountForTest(), 3U);

  EXPECT_CALL(*nudge_for_commit_closure(), Run()).Times(0);
  // In the TestBookmarkClient, descendants of managed nodes shouldn't be
  // synced.
  model.Remove(unsyncable_node, FROM_HERE);

  // Only permanent folders should be tracked.
  EXPECT_THAT(bookmark_tracker->TrackedEntitiesCountForTest(), 3U);
  model.RemoveObserver(&observer);
}

TEST_P(BookmarkModelObserverImplTest, ShouldAddChildrenInArbitraryOrder) {
  std::unique_ptr<SyncedBookmarkTracker> bookmark_tracker =
      SyncedBookmarkTracker::CreateEmpty(sync_pb::DataTypeState());
  AddPermanentFoldersToTracker(bookmark_model(), bookmark_tracker.get());

  BookmarkModelObserverImpl observer(
      bookmark_model(),
      /*nudge_for_commit_closure=*/base::DoNothing(),
      /*on_bookmark_model_being_deleted_closure=*/base::DoNothing(),
      bookmark_tracker.get());
  const bookmarks::BookmarkNode* bookmark_bar_node =
      bookmark_model()->bookmark_bar_node();

  ASSERT_THAT(bookmark_tracker->TrackedEntitiesCountForTest(), 3U);

  // Build this structure:
  // bookmark_bar
  //  |- folder0
  //  |- folder1
  //  |- folder2
  //  |- folder3
  //  |- folder4

  std::array<const bookmarks::BookmarkNode*, 5> nodes;
  for (size_t i = 0; i < 5; i++) {
    nodes[i] = bookmark_model()->AddFolder(
        /*parent=*/bookmark_bar_node, /*index=*/i,
        base::UTF8ToUTF16("folder" + base::NumberToString(i)));
  }

  // Now simulate calling the observer as if the nodes are added in that order.
  // 4,0,2,3,1.
  observer.BookmarkNodeAdded(bookmark_bar_node, 4, false);
  observer.BookmarkNodeAdded(bookmark_bar_node, 0, false);
  observer.BookmarkNodeAdded(bookmark_bar_node, 2, false);
  observer.BookmarkNodeAdded(bookmark_bar_node, 3, false);
  observer.BookmarkNodeAdded(bookmark_bar_node, 1, false);

  EXPECT_THAT(bookmark_tracker->TrackedEntitiesCountForTest(), 8U);

  // Check that position information match the children order.
  EXPECT_TRUE(PositionOf(nodes[0]).LessThan(PositionOf(nodes[1])));
  EXPECT_TRUE(PositionOf(nodes[1]).LessThan(PositionOf(nodes[2])));
  EXPECT_TRUE(PositionOf(nodes[2]).LessThan(PositionOf(nodes[3])));
  EXPECT_TRUE(PositionOf(nodes[3]).LessThan(PositionOf(nodes[4])));
}

TEST_P(BookmarkModelObserverImplTest,
       ShouldCallOnBookmarkModelBeingDeletedClosure) {
  std::unique_ptr<SyncedBookmarkTracker> bookmark_tracker =
      SyncedBookmarkTracker::CreateEmpty(sync_pb::DataTypeState());

  NiceMock<base::MockCallback<base::OnceClosure>>
      on_bookmark_model_being_deleted_closure_mock;

  BookmarkModelObserverImpl observer(
      bookmark_model(),
      /*nudge_for_commit_closure=*/base::DoNothing(),
      on_bookmark_model_being_deleted_closure_mock.Get(),
      bookmark_tracker.get());

  EXPECT_CALL(on_bookmark_model_being_deleted_closure_mock, Run());
  observer.BookmarkModelBeingDeleted();
}

TEST_P(BookmarkModelObserverImplTest, ShouldNotIssueCommitUponFaviconLoad) {
  const GURL kBookmarkUrl("http://www.url.com");
  const GURL kIconUrl("http://www.url.com/favicon.ico");
  const SkColor kColor = SK_ColorRED;

  const bookmarks::BookmarkNode* bookmark_bar_node =
      bookmark_model()->bookmark_bar_node();
  const bookmarks::BookmarkNode* bookmark_node = bookmark_model()->AddURL(
      /*parent=*/bookmark_bar_node, /*index=*/0, u"title", kBookmarkUrl);

  ASSERT_TRUE(bookmark_client()->SimulateFaviconLoaded(
      kBookmarkUrl, kIconUrl, CreateTestImage(kColor)));
  SimulateCommitResponseForAllLocalChanges();
  ASSERT_THAT(bookmark_tracker()->GetEntitiesWithLocalChanges(), IsEmpty());

  const SyncedBookmarkTrackerEntity* entity =
      bookmark_tracker()->GetEntityForBookmarkNode(bookmark_node);
  ASSERT_THAT(entity, NotNull());
  ASSERT_TRUE(entity->metadata().has_bookmark_favicon_hash());
  const uint32_t initial_favicon_hash =
      entity->metadata().bookmark_favicon_hash();

  // Clear the specifics hash (as if the proto definition would have changed).
  // This is needed because otherwise the commit is trivially optimized away
  // (i.e. literally nothing changed).
  bookmark_tracker()->ClearSpecificsHashForTest(entity);

  // Mimic the very same favicon being loaded again (similar to a startup
  // scenario). Note that OnFaviconsChanged() needs no icon URL to invalidate
  // the favicon of a bookmark.
  EXPECT_CALL(*nudge_for_commit_closure(), Run()).Times(0);
  bookmark_model()->underlying_model()->OnFaviconsChanged(
      /*page_urls=*/{kBookmarkUrl},
      /*icon_url=*/GURL());
  ASSERT_TRUE(bookmark_node->is_favicon_loading());
  ASSERT_TRUE(bookmark_client()->SimulateFaviconLoaded(
      kBookmarkUrl, kIconUrl, CreateTestImage(kColor)));

  EXPECT_TRUE(entity->metadata().has_bookmark_favicon_hash());
  EXPECT_THAT(entity->metadata().bookmark_favicon_hash(),
              Eq(initial_favicon_hash));
  EXPECT_THAT(bookmark_tracker()->GetEntitiesWithLocalChanges(), IsEmpty());
}

TEST_P(BookmarkModelObserverImplTest, ShouldCommitLocalFaviconChange) {
  const GURL kBookmarkUrl("http://www.url.com");
  const GURL kInitialIconUrl("http://www.url.com/initial.ico");
  const GURL kFinalIconUrl("http://www.url.com/final.ico");

  const bookmarks::BookmarkNode* bookmark_bar_node =
      bookmark_model()->bookmark_bar_node();
  const bookmarks::BookmarkNode* bookmark_node = bookmark_model()->AddURL(
      /*parent=*/bookmark_bar_node, /*index=*/0, u"title", kBookmarkUrl);

  ASSERT_TRUE(bookmark_node->is_favicon_loading());
  ASSERT_TRUE(bookmark_client()->SimulateFaviconLoaded(
      kBookmarkUrl, kInitialIconUrl, CreateTestImage(SK_ColorRED)));
  SimulateCommitResponseForAllLocalChanges();
  ASSERT_THAT(bookmark_tracker()->GetEntitiesWithLocalChanges(), IsEmpty());

  const SyncedBookmarkTrackerEntity* entity =
      bookmark_tracker()->GetEntityForBookmarkNode(bookmark_node);
  ASSERT_THAT(entity, NotNull());
  ASSERT_TRUE(entity->metadata().has_bookmark_favicon_hash());
  const uint32_t initial_favicon_hash =
      entity->metadata().bookmark_favicon_hash();

  // A favicon change should trigger a commit nudge once the favicon loads, but
  // not earlier. Note that OnFaviconsChanged() needs no icon URL to invalidate
  // the favicon of a bookmark.
  EXPECT_CALL(*nudge_for_commit_closure(), Run()).Times(0);
  bookmark_model()->underlying_model()->OnFaviconsChanged(
      /*page_urls=*/{kBookmarkUrl},
      /*icon_url=*/GURL());
  ASSERT_TRUE(bookmark_node->is_favicon_loading());

  EXPECT_CALL(*nudge_for_commit_closure(), Run());
  ASSERT_TRUE(bookmark_client()->SimulateFaviconLoaded(
      kBookmarkUrl, kFinalIconUrl, CreateTestImage(SK_ColorBLUE)));

  EXPECT_TRUE(entity->metadata().has_bookmark_favicon_hash());
  EXPECT_THAT(entity->metadata().bookmark_favicon_hash(),
              Ne(initial_favicon_hash));
  EXPECT_THAT(bookmark_tracker()->GetEntitiesWithLocalChanges(),
              ElementsAre(HasBookmarkNode(bookmark_node)));
}

TEST_P(BookmarkModelObserverImplTest,
       ShouldNudgeForCommitOnFaviconLoadAfterRestart) {
  const GURL kBookmarkUrl("http://www.url.com");
  const GURL kIconUrl("http://www.url.com/favicon.ico");
  const SkColor kColor = SK_ColorRED;

  // Simulate work after restart. Add a new bookmark to a model and its
  // specifics to the tracker without loading favicon.
  bookmark_model()->RemoveObserver(observer());

  // Add a new node with specifics and mark it unsynced.
  const bookmarks::BookmarkNode* bookmark_bar_node =
      bookmark_model()->bookmark_bar_node();
  const bookmarks::BookmarkNode* bookmark_node = bookmark_model()->AddURL(
      /*parent=*/bookmark_bar_node, /*index=*/0, u"title", kBookmarkUrl);

  sync_pb::EntitySpecifics specifics = CreateSpecificsFromBookmarkNode(
      bookmark_node, bookmark_model(),
      syncer::UniquePosition::InitialPosition(
          syncer::UniquePosition::RandomSuffix())
          .ToProto(),
      /*force_favicon_load=*/false);
  const gfx::Image favicon_image = CreateTestImage(kColor);
  scoped_refptr<base::RefCountedMemory> favicon_bytes =
      favicon_image.As1xPNGBytes();
  specifics.mutable_bookmark()->set_favicon(favicon_bytes->front(),
                                            favicon_bytes->size());
  specifics.mutable_bookmark()->set_icon_url(kIconUrl.spec());
  *specifics.mutable_bookmark()->mutable_unique_position() =
      syncer::UniquePosition::InitialPosition(
          syncer::UniquePosition::RandomSuffix())
          .ToProto();

  const SyncedBookmarkTrackerEntity* entity = bookmark_tracker()->Add(
      bookmark_node, "id", /*server_version=*/1, base::Time::Now(), specifics);
  bookmark_tracker()->IncrementSequenceNumber(entity);

  // Restore state.
  bookmark_model()->AddObserver(observer());

  // Currently there is the unsynced |entity| which has no loaded favicon.
  ASSERT_FALSE(bookmark_node->is_favicon_loaded());
  ASSERT_TRUE(entity->IsUnsynced());

  EXPECT_CALL(*nudge_for_commit_closure(), Run());
  bookmark_model()->GetFavicon(bookmark_node);
  ASSERT_TRUE(bookmark_client()->SimulateFaviconLoaded(
      kBookmarkUrl, kIconUrl, CreateTestImage(SK_ColorRED)));
}

TEST_P(BookmarkModelObserverImplTest,
       ShouldAddRestoredBookmarkWhenTombstoneCommitMayHaveStarted) {
  const bookmarks::BookmarkNode* bookmark_bar_node =
      bookmark_model()->bookmark_bar_node();
  const bookmarks::BookmarkNode* folder =
      bookmark_model()->AddFolder(bookmark_bar_node, 0, u"Title");
  const syncer::ClientTagHash folder_client_tag_hash =
      SyncedBookmarkTracker::GetClientTagHashFromUuid(folder->uuid());
  // Check that the bookmark was added by observer.
  const SyncedBookmarkTrackerEntity* folder_entity =
      bookmark_tracker()->GetEntityForBookmarkNode(folder);
  ASSERT_THAT(folder_entity, NotNull());
  ASSERT_TRUE(folder_entity->IsUnsynced());
  SimulateCommitResponseForAllLocalChanges();
  ASSERT_FALSE(folder_entity->IsUnsynced());

  // Now delete the entity and restore it with the same bookmark node.
  bookmark_model()->Remove(folder, FROM_HERE);

  // The removed bookmark must be saved in the undo service.
  ASSERT_GE(undo_manager()->undo_count(), 1u);
  ASSERT_THAT(bookmark_tracker()->GetEntityForBookmarkNode(folder), IsNull());

  // Check that the entity is a tombstone now.
  const std::vector<const SyncedBookmarkTrackerEntity*> local_changes =
      bookmark_tracker()->GetEntitiesWithLocalChanges();
  ASSERT_THAT(local_changes, ElementsAre(folder_entity));
  ASSERT_TRUE(folder_entity->metadata().is_deleted());
  ASSERT_EQ(
      bookmark_tracker()->GetEntityForClientTagHash(folder_client_tag_hash),
      folder_entity);

  // Restore the removed bookmark.
  undo_manager()->Undo();

  EXPECT_EQ(folder_entity,
            bookmark_tracker()->GetEntityForBookmarkNode(folder));
  EXPECT_EQ(
      bookmark_tracker()->GetEntityForClientTagHash(folder_client_tag_hash),
      folder_entity);
  EXPECT_TRUE(folder_entity->IsUnsynced());
  EXPECT_FALSE(folder_entity->metadata().is_deleted());
  EXPECT_EQ(folder_entity->bookmark_node(), folder);
}

// Tests that the bookmark entity will be committed if its favicon is deleted.
TEST_P(BookmarkModelObserverImplTest, ShouldCommitOnDeleteFavicon) {
  const GURL kBookmarkUrl("http://www.url.com");
  const GURL kIconUrl("http://www.url.com/favicon.ico");

  // Add a new node with specifics.
  const bookmarks::BookmarkNode* bookmark_bar_node =
      bookmark_model()->bookmark_bar_node();
  const bookmarks::BookmarkNode* bookmark_node = bookmark_model()->AddURL(
      /*parent=*/bookmark_bar_node, /*index=*/0, u"title", kBookmarkUrl);

  ASSERT_TRUE(bookmark_node->is_favicon_loading());
  ASSERT_TRUE(bookmark_client()->SimulateFaviconLoaded(
      kBookmarkUrl, kIconUrl, CreateTestImage(SK_ColorRED)));

  const SyncedBookmarkTrackerEntity* entity =
      bookmark_tracker()->GetEntityForBookmarkNode(bookmark_node);
  ASSERT_THAT(entity, NotNull());
  ASSERT_TRUE(entity->IsUnsynced());

  SimulateCommitResponseForAllLocalChanges();

  ASSERT_FALSE(bookmark_tracker()->HasLocalChanges());

  // Delete favicon and check that its deletion is committed.
  bookmark_model()->underlying_model()->OnFaviconsChanged({kBookmarkUrl},
                                                          GURL());
  ASSERT_TRUE(bookmark_node->is_favicon_loading());
  ASSERT_TRUE(bookmark_client()->SimulateEmptyFaviconLoaded(kBookmarkUrl));

  EXPECT_TRUE(entity->IsUnsynced());
}

INSTANTIATE_TEST_SUITE_P(
    ViewType,
    BookmarkModelObserverImplTest,
    testing::Values(TestBookmarkModelView::ViewType::kLocalOrSyncableNodes,
                    TestBookmarkModelView::ViewType::kAccountNodes));

}  // namespace

}  // namespace sync_bookmarks
