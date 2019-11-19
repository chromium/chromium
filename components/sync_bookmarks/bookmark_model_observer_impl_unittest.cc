// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_bookmarks/bookmark_model_observer_impl.h"

#include <algorithm>
#include <memory>
#include <utility>
#include <vector>

#include "base/strings/utf_string_conversions.h"
#include "base/test/mock_callback.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/test/test_bookmark_client.h"
#include "components/sync/base/time.h"
#include "components/sync/base/unique_position.h"
#include "components/sync/driver/sync_driver_switches.h"
#include "components/sync_bookmarks/synced_bookmark_tracker.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace sync_bookmarks {

namespace {

using testing::Eq;
using testing::Ne;
using testing::NiceMock;
using testing::NotNull;
using testing::ElementsAre;

const char kBookmarkBarId[] = "bookmark_bar_id";
const char kBookmarkBarTag[] = "bookmark_bar";
const char kOtherBookmarksId[] = "other_bookmarks_id";
const char kOtherBookmarksTag[] = "other_bookmarks";
const char kMobileBookmarksId[] = "synced_bookmarks_id";
const char kMobileBookmarksTag[] = "synced_bookmarks";
const size_t kMaxEntries = 1000;

class BookmarkModelObserverImplTest : public testing::Test {
 public:
  BookmarkModelObserverImplTest()
      : bookmark_tracker_(std::vector<NodeMetadataPair>(),
                          std::make_unique<sync_pb::ModelTypeState>()),
        observer_(nudge_for_commit_closure_.Get(),
                  /*on_bookmark_model_being_deleted_closure=*/base::DoNothing(),
                  &bookmark_tracker_),
        bookmark_model_(bookmarks::TestBookmarkClient::CreateModel()) {
    bookmark_model_->AddObserver(&observer_);
    sync_pb::EntitySpecifics specifics;
    specifics.mutable_bookmark()->set_title(kBookmarkBarTag);
    bookmark_tracker_.Add(
        /*sync_id=*/kBookmarkBarId,
        /*bookmark_node=*/bookmark_model()->bookmark_bar_node(),
        /*server_version=*/0, /*creation_time=*/base::Time::Now(),
        syncer::UniquePosition::InitialPosition(
            syncer::UniquePosition::RandomSuffix())
            .ToProto(),
        specifics);
    specifics.mutable_bookmark()->set_title(kOtherBookmarksTag);
    bookmark_tracker_.Add(
        /*sync_id=*/kOtherBookmarksId,
        /*bookmark_node=*/bookmark_model()->other_node(),
        /*server_version=*/0, /*creation_time=*/base::Time::Now(),
        syncer::UniquePosition::InitialPosition(
            syncer::UniquePosition::RandomSuffix())
            .ToProto(),
        specifics);
    specifics.mutable_bookmark()->set_title(kMobileBookmarksTag);
    bookmark_tracker_.Add(
        /*sync_id=*/kMobileBookmarksId,
        /*bookmark_node=*/bookmark_model()->mobile_node(),
        /*server_version=*/0, /*creation_time=*/base::Time::Now(),
        syncer::UniquePosition::InitialPosition(
            syncer::UniquePosition::RandomSuffix())
            .ToProto(),
        specifics);
  }

  ~BookmarkModelObserverImplTest() {
    bookmark_model_->RemoveObserver(&observer_);
  }

  void SimulateCommitResponseForAllLocalChanges() {
    for (const SyncedBookmarkTracker::Entity* entity :
         bookmark_tracker()->GetEntitiesWithLocalChanges(kMaxEntries)) {
      const std::string id = entity->metadata()->server_id();
      // Don't simulate change in id for simplicity.
      bookmark_tracker()->UpdateUponCommitResponse(id, id,
                                                   /*acked_sequence_number=*/1,
                                                   /*server_version=*/1);
    }
  }

  syncer::UniquePosition PositionOf(
      const bookmarks::BookmarkNode* bookmark_node) {
    const SyncedBookmarkTracker::Entity* entity =
        bookmark_tracker()->GetEntityForBookmarkNode(bookmark_node);
    return syncer::UniquePosition::FromProto(
        entity->metadata()->unique_position());
  }

  bookmarks::BookmarkModel* bookmark_model() { return bookmark_model_.get(); }
  SyncedBookmarkTracker* bookmark_tracker() { return &bookmark_tracker_; }
  BookmarkModelObserverImpl* observer() { return &observer_; }
  base::MockCallback<base::RepeatingClosure>* nudge_for_commit_closure() {
    return &nudge_for_commit_closure_;
  }

 private:
  NiceMock<base::MockCallback<base::RepeatingClosure>>
      nudge_for_commit_closure_;
  SyncedBookmarkTracker bookmark_tracker_;
  BookmarkModelObserverImpl observer_;
  std::unique_ptr<bookmarks::BookmarkModel> bookmark_model_;
};

TEST_F(BookmarkModelObserverImplTest,
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

  std::vector<const SyncedBookmarkTracker::Entity*> local_changes =
      bookmark_tracker()->GetEntitiesWithLocalChanges(kMaxEntries);
  ASSERT_THAT(local_changes.size(), 1U);
  EXPECT_THAT(local_changes[0]->bookmark_node(), Eq(bookmark_node));
  if (base::FeatureList::IsEnabled(switches::kMergeBookmarksUsingGUIDs)) {
    EXPECT_THAT(local_changes[0]->metadata()->server_id(),
                Eq(bookmark_node->guid()));
  }
}

TEST_F(BookmarkModelObserverImplTest,
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
  ASSERT_THAT(
      bookmark_tracker()->GetEntitiesWithLocalChanges(kMaxEntries).size(), 2U);

  SimulateCommitResponseForAllLocalChanges();

  // There should be no local changes now.
  ASSERT_TRUE(
      bookmark_tracker()->GetEntitiesWithLocalChanges(kMaxEntries).empty());

  // Now update the title of the 2nd node.
  EXPECT_CALL(*nudge_for_commit_closure(), Run());
  bookmark_model()->SetTitle(bookmark_node2, base::UTF8ToUTF16(kNewTitle2));
  // Node 2 should be in the local changes list.
  std::vector<const SyncedBookmarkTracker::Entity*> local_changes =
      bookmark_tracker()->GetEntitiesWithLocalChanges(kMaxEntries);
  ASSERT_THAT(local_changes.size(), 1U);
  EXPECT_THAT(local_changes[0]->bookmark_node(), Eq(bookmark_node2));

  // Now update the url of the 1st node.
  EXPECT_CALL(*nudge_for_commit_closure(), Run());
  bookmark_model()->SetURL(bookmark_node1, GURL(kNewUrl1));

  // Node 1 and 2 should be in the local changes list.
  local_changes = bookmark_tracker()->GetEntitiesWithLocalChanges(kMaxEntries);
  ASSERT_THAT(local_changes.size(), 2U);

  // Constuct a set of the bookmark nodes in the local changes.
  std::set<const bookmarks::BookmarkNode*> nodes_in_local_changes;
  for (const SyncedBookmarkTracker::Entity* entity : local_changes) {
    nodes_in_local_changes.insert(entity->bookmark_node());
  }
  // Both bookmarks should exist in the set.
  EXPECT_TRUE(nodes_in_local_changes.find(bookmark_node1) !=
              nodes_in_local_changes.end());
  EXPECT_TRUE(nodes_in_local_changes.find(bookmark_node2) !=
              nodes_in_local_changes.end());

  // Now update metainfo of the 1st node.
  EXPECT_CALL(*nudge_for_commit_closure(), Run());
  bookmark_model()->SetNodeMetaInfo(bookmark_node1, "key", "value");
}

TEST_F(BookmarkModelObserverImplTest,
       BookmarkMovedShouldUpdateTheTrackerAndNudgeForCommit) {
  // Build this structure:
  // bookmark_bar
  //  |- folder1
  //      |- bookmark1
  const GURL kUrl("http://www.url1.com");

  const bookmarks::BookmarkNode* bookmark_bar_node =
      bookmark_model()->bookmark_bar_node();
  const bookmarks::BookmarkNode* folder1_node = bookmark_model()->AddFolder(
      /*parent=*/bookmark_bar_node, /*index=*/0, base::UTF8ToUTF16("folder1"));
  const bookmarks::BookmarkNode* bookmark1_node = bookmark_model()->AddURL(
      /*parent=*/folder1_node, /*index=*/0, base::UTF8ToUTF16("bookmark1"),
      kUrl);

  // Verify number of entities local changes. Should be the same as number of
  // new nodes.
  ASSERT_THAT(
      bookmark_tracker()->GetEntitiesWithLocalChanges(kMaxEntries).size(), 2U);

  // All bookmarks should be tracked now.
  ASSERT_THAT(bookmark_tracker()->TrackedEntitiesCountForTest(), 5U);

  SimulateCommitResponseForAllLocalChanges();

  // There should be no local changes now.
  ASSERT_TRUE(
      bookmark_tracker()->GetEntitiesWithLocalChanges(kMaxEntries).empty());

  // Now change it to this structure.
  // Build this structure:
  // bookmark_bar
  //  |- bookmark1
  //  |- folder1

  EXPECT_CALL(*nudge_for_commit_closure(), Run());
  bookmark_model()->Move(bookmark1_node, bookmark_bar_node, 0);
  EXPECT_TRUE(PositionOf(bookmark1_node).LessThan(PositionOf(folder1_node)));
}

TEST_F(BookmarkModelObserverImplTest,
       ReorderChildrenShouldUpdateTheTrackerAndNudgeForCommit) {
  const std::string kTitle = "title";
  const std::string kUrl = "http://www.url.com";

  // Build this structure:
  // bookmark_bar
  //  |- node0
  //  |- node1
  //  |- node2
  //  |- node3
  const bookmarks::BookmarkNode* bookmark_bar_node =
      bookmark_model()->bookmark_bar_node();
  std::vector<const bookmarks::BookmarkNode*> nodes;
  for (size_t i = 0; i < 4; ++i) {
    nodes.push_back(bookmark_model()->AddURL(
        /*parent=*/bookmark_bar_node, /*index=*/i, base::UTF8ToUTF16(kTitle),
        GURL(kUrl)));
  }

  // Verify number of entities local changes. Should be the same as number of
  // new nodes.
  ASSERT_THAT(
      bookmark_tracker()->GetEntitiesWithLocalChanges(kMaxEntries).size(), 4U);

  // All bookmarks should be tracked now.
  ASSERT_THAT(bookmark_tracker()->TrackedEntitiesCountForTest(), 7U);

  SimulateCommitResponseForAllLocalChanges();

  // Reorder it to be:
  // bookmark_bar
  //  |- node1
  //  |- node3
  //  |- node0
  //  |- node2
  bookmark_model()->ReorderChildren(bookmark_bar_node,
                                    {nodes[1], nodes[3], nodes[0], nodes[2]});
  EXPECT_TRUE(PositionOf(nodes[1]).LessThan(PositionOf(nodes[3])));
  EXPECT_TRUE(PositionOf(nodes[3]).LessThan(PositionOf(nodes[0])));
  EXPECT_TRUE(PositionOf(nodes[0]).LessThan(PositionOf(nodes[2])));

  std::vector<const SyncedBookmarkTracker::Entity*> local_changes =
      bookmark_tracker()->GetEntitiesWithLocalChanges(kMaxEntries);
  ASSERT_THAT(local_changes.size(), nodes.size());

  // Constuct a set of the bookmark nodes in the local changes.
  std::set<const bookmarks::BookmarkNode*> nodes_in_local_changes;
  for (const SyncedBookmarkTracker::Entity* entity : local_changes) {
    nodes_in_local_changes.insert(entity->bookmark_node());
  }

  // All reordered nodes should exist in the set of local changes to be
  // committed.
  for (const bookmarks::BookmarkNode* node : nodes) {
    EXPECT_THAT(nodes_in_local_changes.count(node), Ne(0U));
  }
}

TEST_F(BookmarkModelObserverImplTest,
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
      /*parent=*/bookmark_bar_node, /*index=*/0, base::UTF8ToUTF16("folder1"));
  const bookmarks::BookmarkNode* bookmark1_node = bookmark_model()->AddURL(
      /*parent=*/folder1_node, /*index=*/0, base::UTF8ToUTF16("bookmark1"),
      kUrl);
  const bookmarks::BookmarkNode* folder2_node = bookmark_model()->AddFolder(
      /*parent=*/folder1_node, /*index=*/1, base::UTF8ToUTF16("folder2"));
  const bookmarks::BookmarkNode* bookmark2_node = bookmark_model()->AddURL(
      /*parent=*/folder2_node, /*index=*/0, base::UTF8ToUTF16("bookmark2"),
      kUrl);
  const bookmarks::BookmarkNode* bookmark3_node = bookmark_model()->AddURL(
      /*parent=*/folder2_node, /*index=*/1, base::UTF8ToUTF16("bookmark3"),
      kUrl);

  // All bookmarks should be tracked now.
  ASSERT_THAT(bookmark_tracker()->TrackedEntitiesCountForTest(), 8U);

  SimulateCommitResponseForAllLocalChanges();

  // There should be no local changes now.
  ASSERT_TRUE(
      bookmark_tracker()->GetEntitiesWithLocalChanges(kMaxEntries).empty());

  const SyncedBookmarkTracker::Entity* folder2_entity =
      bookmark_tracker()->GetEntityForBookmarkNode(folder2_node);
  const SyncedBookmarkTracker::Entity* bookmark2_entity =
      bookmark_tracker()->GetEntityForBookmarkNode(bookmark2_node);
  const SyncedBookmarkTracker::Entity* bookmark3_entity =
      bookmark_tracker()->GetEntityForBookmarkNode(bookmark3_node);

  ASSERT_FALSE(folder2_entity->metadata()->is_deleted());
  ASSERT_FALSE(bookmark2_entity->metadata()->is_deleted());
  ASSERT_FALSE(bookmark3_entity->metadata()->is_deleted());

  const std::string& folder2_entity_id =
      folder2_entity->metadata()->server_id();
  const std::string& bookmark2_entity_id =
      bookmark2_entity->metadata()->server_id();
  const std::string& bookmark3_entity_id =
      bookmark3_entity->metadata()->server_id();
  // Delete folder2.
  EXPECT_CALL(*nudge_for_commit_closure(), Run());
  bookmark_model()->Remove(folder2_node);

  // folder2, bookmark2, and bookmark3 should be marked deleted.
  EXPECT_TRUE(bookmark_tracker()
                  ->GetEntityForSyncId(folder2_entity_id)
                  ->metadata()
                  ->is_deleted());
  EXPECT_TRUE(bookmark_tracker()
                  ->GetEntityForSyncId(bookmark2_entity_id)
                  ->metadata()
                  ->is_deleted());
  EXPECT_TRUE(bookmark_tracker()
                  ->GetEntityForSyncId(bookmark3_entity_id)
                  ->metadata()
                  ->is_deleted());

  // folder2, bookmark2, and bookmark3 should be in the local changes list.
  std::vector<const SyncedBookmarkTracker::Entity*> local_changes =
      bookmark_tracker()->GetEntitiesWithLocalChanges(kMaxEntries);
  ASSERT_THAT(local_changes.size(), 3U);

  // All deleted nodes entities should exist in the set of local changes to be
  // committed and folder2 deletion should be the last one (after all children
  // deletions).
  EXPECT_THAT(
      local_changes,
      ElementsAre(bookmark_tracker()->GetEntityForSyncId(bookmark2_entity_id),
                  bookmark_tracker()->GetEntityForSyncId(bookmark3_entity_id),
                  bookmark_tracker()->GetEntityForSyncId(folder2_entity_id)));

  // folder1 and bookmark1 are still tracked.
  EXPECT_TRUE(bookmark_tracker()->GetEntityForBookmarkNode(folder1_node));
  EXPECT_TRUE(bookmark_tracker()->GetEntityForBookmarkNode(bookmark1_node));
}

TEST_F(BookmarkModelObserverImplTest,
       BookmarkCreationAndRemovalShouldRequireTwoCommitResponsesBeforeRemoval) {
  const bookmarks::BookmarkNode* bookmark_bar_node =
      bookmark_model()->bookmark_bar_node();
  const bookmarks::BookmarkNode* folder_node = bookmark_model()->AddFolder(
      /*parent=*/bookmark_bar_node, /*index=*/0, base::UTF8ToUTF16("folder"));

  // Node should be tracked now.
  ASSERT_THAT(bookmark_tracker()->TrackedEntitiesCountForTest(), 4U);
  const std::string id = bookmark_tracker()
                             ->GetEntityForBookmarkNode(folder_node)
                             ->metadata()
                             ->server_id();
  ASSERT_THAT(
      bookmark_tracker()->GetEntitiesWithLocalChanges(kMaxEntries).size(), 1U);

  bookmark_tracker()->MarkCommitMayHaveStarted(id);

  // Remove the folder.
  bookmark_model()->Remove(folder_node);

  // Simulate a commit response for the first commit request (the creation).
  // Don't simulate change in id for simplicity.
  bookmark_tracker()->UpdateUponCommitResponse(id, id,
                                               /*acked_sequence_number=*/1,
                                               /*server_version=*/1);

  // There should still be one local change (the deletion).
  EXPECT_THAT(
      bookmark_tracker()->GetEntitiesWithLocalChanges(kMaxEntries).size(), 1U);

  // Entity is still tracked.
  EXPECT_THAT(bookmark_tracker()->TrackedEntitiesCountForTest(), 4U);

  // Commit the deletion.
  bookmark_tracker()->UpdateUponCommitResponse(id, id,
                                               /*acked_sequence_number=*/2,
                                               /*server_version=*/2);
  // Entity should have been dropped.
  EXPECT_THAT(bookmark_tracker()->TrackedEntitiesCountForTest(), 3U);
}

TEST_F(BookmarkModelObserverImplTest,
       BookmarkCreationAndRemovalBeforeCommitRequestShouldBeRemovedDirectly) {
  const bookmarks::BookmarkNode* bookmark_bar_node =
      bookmark_model()->bookmark_bar_node();
  const bookmarks::BookmarkNode* folder_node = bookmark_model()->AddFolder(
      /*parent=*/bookmark_bar_node, /*index=*/0, base::UTF8ToUTF16("folder"));

  // Node should be tracked now.
  ASSERT_THAT(bookmark_tracker()->TrackedEntitiesCountForTest(), 4U);
  const std::string id = bookmark_tracker()
                             ->GetEntityForBookmarkNode(folder_node)
                             ->metadata()
                             ->server_id();
  ASSERT_THAT(
      bookmark_tracker()->GetEntitiesWithLocalChanges(kMaxEntries).size(), 1U);

  // Remove the folder.
  bookmark_model()->Remove(folder_node);

  // Entity should have been dropped.
  EXPECT_THAT(bookmark_tracker()->TrackedEntitiesCountForTest(), 3U);
}

TEST_F(BookmarkModelObserverImplTest, ShouldPositionSiblings) {
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

TEST_F(BookmarkModelObserverImplTest, ShouldNotSyncUnsyncableBookmarks) {
  auto owned_managed_node = std::make_unique<bookmarks::BookmarkPermanentNode>(
      100, bookmarks::BookmarkNode::FOLDER);
  bookmarks::BookmarkPermanentNode* managed_node = owned_managed_node.get();
  auto client = std::make_unique<bookmarks::TestBookmarkClient>();
  client->SetManagedNodeToLoad(std::move(owned_managed_node));

  std::unique_ptr<bookmarks::BookmarkModel> model =
      bookmarks::TestBookmarkClient::CreateModelWithClient(std::move(client));

  SyncedBookmarkTracker bookmark_tracker(
      std::vector<NodeMetadataPair>(),
      std::make_unique<sync_pb::ModelTypeState>());
  sync_pb::EntitySpecifics specifics;
  specifics.mutable_bookmark()->set_title(kBookmarkBarTag);
  bookmark_tracker.Add(
      /*sync_id=*/kBookmarkBarId,
      /*bookmark_node=*/model->bookmark_bar_node(),
      /*server_version=*/0, /*creation_time=*/base::Time::Now(),
      syncer::UniquePosition::InitialPosition(
          syncer::UniquePosition::RandomSuffix())
          .ToProto(),
      specifics);
  specifics.mutable_bookmark()->set_title(kOtherBookmarksTag);
  bookmark_tracker.Add(
      /*sync_id=*/kOtherBookmarksId,
      /*bookmark_node=*/model->other_node(),
      /*server_version=*/0, /*creation_time=*/base::Time::Now(),
      syncer::UniquePosition::InitialPosition(
          syncer::UniquePosition::RandomSuffix())
          .ToProto(),
      specifics);
  specifics.mutable_bookmark()->set_title(kMobileBookmarksTag);
  bookmark_tracker.Add(
      /*sync_id=*/kMobileBookmarksId,
      /*bookmark_node=*/model->mobile_node(),
      /*server_version=*/0, /*creation_time=*/base::Time::Now(),
      syncer::UniquePosition::InitialPosition(
          syncer::UniquePosition::RandomSuffix())
          .ToProto(),
      specifics);
  BookmarkModelObserverImpl observer(
      nudge_for_commit_closure()->Get(),
      /*on_bookmark_model_being_deleted_closure=*/base::DoNothing(),
      &bookmark_tracker);

  model->AddObserver(&observer);

  EXPECT_CALL(*nudge_for_commit_closure(), Run()).Times(0);
  // In the TestBookmarkClient, descendants of managed nodes shouldn't be
  // synced.
  const bookmarks::BookmarkNode* unsyncable_node =
      model->AddURL(/*parent=*/managed_node, /*index=*/0,
                    base::ASCIIToUTF16("Title"), GURL("http://www.url.com"));
  // Only permanent folders should be tracked.
  EXPECT_THAT(bookmark_tracker.TrackedEntitiesCountForTest(), 3U);

  EXPECT_CALL(*nudge_for_commit_closure(), Run()).Times(0);
  // In the TestBookmarkClient, descendants of managed nodes shouldn't be
  // synced.
  model->SetTitle(unsyncable_node, base::ASCIIToUTF16("NewTitle"));
  // Only permanent folders should be tracked.
  EXPECT_THAT(bookmark_tracker.TrackedEntitiesCountForTest(), 3U);

  EXPECT_CALL(*nudge_for_commit_closure(), Run()).Times(0);
  // In the TestBookmarkClient, descendants of managed nodes shouldn't be
  // synced.
  model->Remove(unsyncable_node);

  // Only permanent folders should be tracked.
  EXPECT_THAT(bookmark_tracker.TrackedEntitiesCountForTest(), 3U);
  model->RemoveObserver(&observer);
}

TEST_F(BookmarkModelObserverImplTest, ShouldAddChildrenInArbitraryOrder) {
  SyncedBookmarkTracker bookmark_tracker(
      std::vector<NodeMetadataPair>(),
      std::make_unique<sync_pb::ModelTypeState>());
  BookmarkModelObserverImpl observer(
      /*nudge_for_commit_closure=*/base::DoNothing(),
      /*on_bookmark_model_being_deleted_closure=*/base::DoNothing(),
      &bookmark_tracker);
  const bookmarks::BookmarkNode* bookmark_bar_node =
      bookmark_model()->bookmark_bar_node();
  // Add the bookmark bar to the tracker.
  sync_pb::EntitySpecifics specifics;
  specifics.mutable_bookmark()->set_title(kBookmarkBarTag);
  bookmark_tracker.Add(
      /*sync_id=*/kBookmarkBarId,
      /*bookmark_node=*/bookmark_model()->bookmark_bar_node(),
      /*server_version=*/0, /*creation_time=*/base::Time::Now(),
      syncer::UniquePosition::InitialPosition(
          syncer::UniquePosition::RandomSuffix())
          .ToProto(),
      specifics);

  // Build this structure:
  // bookmark_bar
  //  |- folder0
  //  |- folder1
  //  |- folder2
  //  |- folder3
  //  |- folder4

  const bookmarks::BookmarkNode* nodes[5];
  for (size_t i = 0; i < 5; i++) {
    nodes[i] = bookmark_model()->AddFolder(
        /*parent=*/bookmark_bar_node, /*index=*/i,
        base::UTF8ToUTF16("folder" + std::to_string(i)));
  }

  // Now simulate calling the observer as if the nodes are added in that order.
  // 4,0,2,3,1.
  observer.BookmarkNodeAdded(bookmark_model(), bookmark_bar_node, 4);
  observer.BookmarkNodeAdded(bookmark_model(), bookmark_bar_node, 0);
  observer.BookmarkNodeAdded(bookmark_model(), bookmark_bar_node, 2);
  observer.BookmarkNodeAdded(bookmark_model(), bookmark_bar_node, 3);
  observer.BookmarkNodeAdded(bookmark_model(), bookmark_bar_node, 1);

  ASSERT_THAT(bookmark_tracker.TrackedEntitiesCountForTest(), 6U);

  // Check that position information match the children order.
  EXPECT_TRUE(PositionOf(nodes[0]).LessThan(PositionOf(nodes[1])));
  EXPECT_TRUE(PositionOf(nodes[1]).LessThan(PositionOf(nodes[2])));
  EXPECT_TRUE(PositionOf(nodes[2]).LessThan(PositionOf(nodes[3])));
  EXPECT_TRUE(PositionOf(nodes[3]).LessThan(PositionOf(nodes[4])));
}

TEST_F(BookmarkModelObserverImplTest,
       ShouldCallOnBookmarkModelBeingDeletedClosure) {
  SyncedBookmarkTracker bookmark_tracker(
      std::vector<NodeMetadataPair>(),
      std::make_unique<sync_pb::ModelTypeState>());

  NiceMock<base::MockCallback<base::OnceClosure>>
      on_bookmark_model_being_deleted_closure_mock;

  BookmarkModelObserverImpl observer(
      /*nudge_for_commit_closure=*/base::DoNothing(),
      on_bookmark_model_being_deleted_closure_mock.Get(), &bookmark_tracker);

  EXPECT_CALL(on_bookmark_model_being_deleted_closure_mock, Run());
  observer.BookmarkModelBeingDeleted(/*model=*/nullptr);
}

}  // namespace

}  // namespace sync_bookmarks
