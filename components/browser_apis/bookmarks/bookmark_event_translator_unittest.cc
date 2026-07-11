// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browser_apis/bookmarks/bookmark_event_translator.h"

#include <memory>
#include <optional>
#include <vector>

#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/uuid.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/test/test_bookmark_client.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace bookmarks_api {

namespace {

base::test::ScopedFeatureList EnableAccountBookmarkStorage() {
  base::test::ScopedFeatureList features;
  features.InitFromCommandLine("SyncEnableBookmarksInTransportMode", "");
  return features;
}

}  // namespace

class BookmarkEventTranslatorTest : public testing::Test,
                                    public BookmarkEventTranslator::Subscriber {
 public:
  BookmarkEventTranslatorTest() {
    model_ = bookmarks::TestBookmarkClient::CreateModel();
    translator_ = std::make_unique<BookmarkEventTranslator>(
        model_.get(), /*managed=*/nullptr, this);
  }

  // BookmarkEventTranslator::Subscriber:
  void OnBookmarkEvents(
      const std::vector<mojom::BookmarksEventPtr>& events) override {
    for (const auto& event : events) {
      events_.push_back(event.Clone());
    }
  }

  void ClearEvents() { events_.clear(); }

 protected:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<bookmarks::BookmarkModel> model_;
  std::unique_ptr<BookmarkEventTranslator> translator_;
  std::vector<mojom::BookmarksEventPtr> events_;
};

TEST_F(BookmarkEventTranslatorTest, AddBookmark) {
  const bookmarks::BookmarkNode* parent = model_->bookmark_bar_node();
  model_->AddURL(parent, 0, u"Title", GURL("http://example.com"));

  ASSERT_EQ(events_.size(), 1u);
  ASSERT_TRUE(events_[0]->is_added());
  const auto& added = events_[0]->get_added();
  ASSERT_EQ(added->parent_id, parent->uuid());
  ASSERT_EQ(added->index, 0);
  ASSERT_TRUE(added->node->is_url());
  ASSERT_TRUE(added->node->get_url()->id.has_value());
  ASSERT_EQ(added->node->get_url()->title, "Title");
  ASSERT_EQ(added->node->get_url()->url, GURL("http://example.com"));
}

TEST_F(BookmarkEventTranslatorTest, RemoveBookmark) {
  const bookmarks::BookmarkNode* parent = model_->bookmark_bar_node();
  const bookmarks::BookmarkNode* node =
      model_->AddURL(parent, 0, u"Title", GURL("http://example.com"));
  base::Uuid node_uuid = node->uuid();
  ClearEvents();

  model_->Remove(node, bookmarks::metrics::BookmarkEditSource::kUser,
                 FROM_HERE);

  ASSERT_EQ(events_.size(), 1u);
  ASSERT_TRUE(events_[0]->is_removed());
  ASSERT_EQ(events_[0]->get_removed()->id, node_uuid);
}

TEST_F(BookmarkEventTranslatorTest, MoveBookmark) {
  const bookmarks::BookmarkNode* parent = model_->bookmark_bar_node();
  const bookmarks::BookmarkNode* other_parent = model_->other_node();
  const bookmarks::BookmarkNode* node =
      model_->AddURL(parent, 0, u"Title", GURL("http://example.com"));
  ClearEvents();

  model_->Move(node, other_parent, 0);

  ASSERT_EQ(events_.size(), 1u);
  ASSERT_TRUE(events_[0]->is_moved());
  const auto& moved = events_[0]->get_moved();
  ASSERT_EQ(moved->old_parent_id, parent->uuid());
  ASSERT_EQ(moved->old_index, 0);
  ASSERT_EQ(moved->new_parent_id, other_parent->uuid());
  ASSERT_EQ(moved->new_index, 0);
}

TEST_F(BookmarkEventTranslatorTest, ChangeBookmark) {
  const bookmarks::BookmarkNode* parent = model_->bookmark_bar_node();
  const bookmarks::BookmarkNode* node =
      model_->AddURL(parent, 0, u"Title", GURL("http://example.com"));
  base::Uuid node_uuid = node->uuid();
  ClearEvents();

  model_->SetTitle(node, u"New Title",
                   bookmarks::metrics::BookmarkEditSource::kUser);

  ASSERT_EQ(events_.size(), 1u);
  ASSERT_TRUE(events_[0]->is_changed());
  const auto& changed = events_[0]->get_changed();
  ASSERT_TRUE(changed->node->is_url());
  ASSERT_EQ(changed->node->get_url()->id, node_uuid);
  ASSERT_EQ(changed->node->get_url()->title, "New Title");
}

TEST_F(BookmarkEventTranslatorTest, ReorderChildren) {
  const bookmarks::BookmarkNode* parent = model_->bookmark_bar_node();
  const bookmarks::BookmarkNode* node1 =
      model_->AddURL(parent, 0, u"Title 1", GURL("http://example1.com"));
  const bookmarks::BookmarkNode* node2 =
      model_->AddURL(parent, 1, u"Title 2", GURL("http://example2.com"));
  ClearEvents();

  std::vector<const bookmarks::BookmarkNode*> new_order = {node2, node1};
  model_->ReorderChildren(parent, new_order);

  // Reordering [node1, node2] -> [node2, node1] should move node2 to index 0.
  ASSERT_EQ(events_.size(), 1u);
  ASSERT_TRUE(events_[0]->is_moved());
  const auto& moved = events_[0]->get_moved();
  ASSERT_EQ(moved->old_parent_id, parent->uuid());
  ASSERT_EQ(moved->old_index, 1);
  ASSERT_EQ(moved->new_parent_id, parent->uuid());
  ASSERT_EQ(moved->new_index, 0);
}

TEST_F(BookmarkEventTranslatorTest, ReorderNestedFolderChildren) {
  // Reorder must still work in a nested folder after adds and after a sibling
  // subtree is removed.
  const bookmarks::BookmarkNode* bar = model_->bookmark_bar_node();
  const bookmarks::BookmarkNode* folder = model_->AddFolder(bar, 0, u"Folder");
  const bookmarks::BookmarkNode* doomed = model_->AddFolder(bar, 1, u"Doomed");
  const bookmarks::BookmarkNode* node1 =
      model_->AddURL(folder, 0, u"Title 1", GURL("http://example1.com"));
  const bookmarks::BookmarkNode* node2 =
      model_->AddURL(folder, 1, u"Title 2", GURL("http://example2.com"));
  model_->Remove(doomed, bookmarks::metrics::BookmarkEditSource::kUser,
                 FROM_HERE);
  ClearEvents();

  std::vector<const bookmarks::BookmarkNode*> new_order = {node2, node1};
  model_->ReorderChildren(folder, new_order);

  ASSERT_EQ(events_.size(), 1u);
  ASSERT_TRUE(events_[0]->is_moved());
  const auto& moved = events_[0]->get_moved();
  ASSERT_EQ(moved->old_parent_id, folder->uuid());
  ASSERT_EQ(moved->old_index, 1);
  ASSERT_EQ(moved->new_parent_id, folder->uuid());
  ASSERT_EQ(moved->new_index, 0);
}

TEST_F(BookmarkEventTranslatorTest, ReorderAfterMoveBetweenFolders) {
  // A reorder after moving a node into a new parent must map to the correct
  // move, because the snapshot is captured on demand from the parent's current
  // children when the reorder begins.
  const bookmarks::BookmarkNode* bar = model_->bookmark_bar_node();
  const bookmarks::BookmarkNode* other = model_->other_node();
  const bookmarks::BookmarkNode* a =
      model_->AddURL(other, 0, u"A", GURL("http://a.com"));
  const bookmarks::BookmarkNode* b =
      model_->AddURL(bar, 0, u"B", GURL("http://b.com"));
  model_->Move(a, bar, 1);
  ClearEvents();

  std::vector<const bookmarks::BookmarkNode*> new_order = {a, b};
  model_->ReorderChildren(bar, new_order);

  ASSERT_EQ(events_.size(), 1u);
  ASSERT_TRUE(events_[0]->is_moved());
  ASSERT_EQ(events_[0]->get_moved()->old_index, 1);
  ASSERT_EQ(events_[0]->get_moved()->new_index, 0);
}

TEST_F(BookmarkEventTranslatorTest, ReorderAfterSameParentMove) {
  // A reorder after moving a node within the same folder must map each node
  // exactly once.
  const bookmarks::BookmarkNode* bar = model_->bookmark_bar_node();
  const bookmarks::BookmarkNode* a =
      model_->AddURL(bar, 0, u"A", GURL("http://a.com"));
  const bookmarks::BookmarkNode* b =
      model_->AddURL(bar, 1, u"B", GURL("http://b.com"));
  const bookmarks::BookmarkNode* c =
      model_->AddURL(bar, 2, u"C", GURL("http://c.com"));
  model_->Move(a, bar, 3);  // Order becomes B, C, A.
  ClearEvents();

  std::vector<const bookmarks::BookmarkNode*> new_order = {a, b, c};
  model_->ReorderChildren(bar, new_order);

  ASSERT_EQ(events_.size(), 1u);
  ASSERT_TRUE(events_[0]->is_moved());
  ASSERT_EQ(events_[0]->get_moved()->old_index, 2);
  ASSERT_EQ(events_[0]->get_moved()->new_index, 0);
}

TEST_F(BookmarkEventTranslatorTest, ReorderDuringExtensiveChanges) {
  // Sync applies remote updates inside an extensive-changes batch: it can add
  // children and then reorder them before any notification is delivered.
  // Because the snapshot is captured on demand in OnWillReorderBookmarkNode(),
  // the reorder still maps to the correct move even though the adds were never
  // used to maintain the snapshot. All events are delivered together when the
  // batch ends.
  const bookmarks::BookmarkNode* bar = model_->bookmark_bar_node();

  model_->BeginExtensiveChanges();
  const bookmarks::BookmarkNode* node1 =
      model_->AddURL(bar, 0, u"Title 1", GURL("http://example1.com"));
  const bookmarks::BookmarkNode* node2 =
      model_->AddURL(bar, 1, u"Title 2", GURL("http://example2.com"));

  // Events are queued, not delivered, while the batch is in progress.
  ASSERT_TRUE(events_.empty());

  std::vector<const bookmarks::BookmarkNode*> new_order = {node2, node1};
  model_->ReorderChildren(bar, new_order);
  ASSERT_TRUE(events_.empty());

  model_->EndExtensiveChanges();

  // The two adds and the single reorder move are flushed together, in order.
  ASSERT_EQ(events_.size(), 3u);
  ASSERT_TRUE(events_[0]->is_added());
  ASSERT_TRUE(events_[1]->is_added());
  ASSERT_TRUE(events_[2]->is_moved());
  const auto& moved = events_[2]->get_moved();
  ASSERT_EQ(moved->old_parent_id, bar->uuid());
  ASSERT_EQ(moved->old_index, 1);
  ASSERT_EQ(moved->new_parent_id, bar->uuid());
  ASSERT_EQ(moved->new_index, 0);
}

TEST_F(BookmarkEventTranslatorTest, RemoveAllUserBookmarks) {
  const bookmarks::BookmarkNode* parent = model_->bookmark_bar_node();
  const bookmarks::BookmarkNode* other_parent = model_->other_node();
  const bookmarks::BookmarkNode* node1 =
      model_->AddURL(parent, 0, u"Title 1", GURL("http://example1.com"));
  const bookmarks::BookmarkNode* node2 =
      model_->AddURL(other_parent, 0, u"Title 2", GURL("http://example2.com"));
  base::Uuid uuid1 = node1->uuid();
  base::Uuid uuid2 = node2->uuid();
  ClearEvents();

  model_->RemoveAllUserBookmarks(FROM_HERE);

  // Should get 2 removed events: node1 (bookmark_bar) and node2 (other).
  // Order should be bookmark_bar then other node.
  ASSERT_EQ(events_.size(), 2u);
  ASSERT_TRUE(events_[0]->is_removed());
  ASSERT_EQ(events_[0]->get_removed()->id, uuid1);
  ASSERT_TRUE(events_[1]->is_removed());
  ASSERT_EQ(events_[1]->get_removed()->id, uuid2);
}

// Fixture that additionally enables account bookmark storage so that both local
// and account permanent folders exist. Account and local permanent folders
// share the same fixed UUIDs, which is why the snapshot is keyed by node
// pointer rather than UUID.
class BookmarkEventTranslatorAccountTest
    : public testing::Test,
      public BookmarkEventTranslator::Subscriber {
 public:
  BookmarkEventTranslatorAccountTest() {
    model_ = bookmarks::TestBookmarkClient::CreateModel();
    model_->CreateAccountPermanentFolders();
    translator_ = std::make_unique<BookmarkEventTranslator>(
        model_.get(), /*managed=*/nullptr, this);
  }

  // BookmarkEventTranslator::Subscriber:
  void OnBookmarkEvents(
      const std::vector<mojom::BookmarksEventPtr>& events) override {
    for (const auto& event : events) {
      events_.push_back(event.Clone());
    }
  }

  void ClearEvents() { events_.clear(); }

 protected:
  base::test::ScopedFeatureList features_ = EnableAccountBookmarkStorage();
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<bookmarks::BookmarkModel> model_;
  std::unique_ptr<BookmarkEventTranslator> translator_;
  std::vector<mojom::BookmarksEventPtr> events_;
};

TEST_F(BookmarkEventTranslatorAccountTest,
       RemoveAllUserBookmarksIncludesAccountFolders) {
  // RemoveAllUserBookmarks() clears every permanent folder across both local
  // and account storage but fires a single notification, so the translator must
  // emit a removed event for the account bookmark as well as the local one.
  const bookmarks::BookmarkNode* local_bar = model_->bookmark_bar_node();
  const bookmarks::BookmarkNode* account_bar =
      model_->account_bookmark_bar_node();
  ASSERT_NE(account_bar, nullptr);

  const bookmarks::BookmarkNode* local_node =
      model_->AddURL(local_bar, 0, u"Local", GURL("http://local.com"));
  const bookmarks::BookmarkNode* account_node =
      model_->AddURL(account_bar, 0, u"Account", GURL("http://account.com"));
  const base::Uuid local_uuid = local_node->uuid();
  const base::Uuid account_uuid = account_node->uuid();
  ClearEvents();

  model_->RemoveAllUserBookmarks(FROM_HERE);

  // Root children are ordered local folders first, then account folders, so the
  // local bookmark is removed before the account one.
  ASSERT_EQ(events_.size(), 2u);
  ASSERT_TRUE(events_[0]->is_removed());
  ASSERT_EQ(events_[0]->get_removed()->id, local_uuid);
  ASSERT_TRUE(events_[1]->is_removed());
  ASSERT_EQ(events_[1]->get_removed()->id, account_uuid);
}

TEST_F(BookmarkEventTranslatorAccountTest,
       ReorderAfterCrossStorageFolderMoveWithUuidReassignment) {
  // Moving a folder between storages can reassign UUIDs across the moved
  // subtree to avoid collisions in the destination. The snapshot is captured on
  // demand when the reorder begins, so it must pick up the reassigned child
  // UUIDs; otherwise the reorder could not find them and would CHECK-fail.
  const bookmarks::BookmarkNode* local_bar = model_->bookmark_bar_node();
  const bookmarks::BookmarkNode* account_bar =
      model_->account_bookmark_bar_node();
  ASSERT_NE(account_bar, nullptr);

  // Give an account bookmark and a soon-to-be-moved local bookmark the same
  // UUID to force a collision when the local subtree moves into account
  // storage.
  const base::Uuid shared_uuid = base::Uuid::GenerateRandomV4();
  model_->AddURL(account_bar, 0, u"AccountDup", GURL("http://dup.com"),
                 /*meta_info=*/nullptr, /*creation_time=*/std::nullopt,
                 shared_uuid);

  const bookmarks::BookmarkNode* folder =
      model_->AddFolder(local_bar, 0, u"Folder");
  const bookmarks::BookmarkNode* child0 = model_->AddURL(
      folder, 0, u"Child 0", GURL("http://child0.com"),
      /*meta_info=*/nullptr, /*creation_time=*/std::nullopt, shared_uuid);
  const bookmarks::BookmarkNode* child1 =
      model_->AddURL(folder, 1, u"Child 1", GURL("http://child1.com"));

  // Move the folder from local storage into account storage. child0's UUID
  // collides with the account bookmark and is reassigned during the move.
  model_->Move(folder, account_bar, 1);
  ASSERT_NE(child0->uuid(), shared_uuid);
  ClearEvents();

  // Reorder the moved folder's children: [child0, child1] -> [child1, child0].
  std::vector<const bookmarks::BookmarkNode*> new_order = {child1, child0};
  model_->ReorderChildren(folder, new_order);

  ASSERT_EQ(events_.size(), 1u);
  ASSERT_TRUE(events_[0]->is_moved());
  const auto& moved = events_[0]->get_moved();
  ASSERT_EQ(moved->old_parent_id, folder->uuid());
  ASSERT_EQ(moved->new_parent_id, folder->uuid());
  ASSERT_EQ(moved->old_index, 1);
  ASSERT_EQ(moved->new_index, 0);
}

}  // namespace bookmarks_api
