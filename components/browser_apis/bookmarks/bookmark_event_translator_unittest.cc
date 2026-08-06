// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <optional>
#include <vector>

#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/uuid.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/scoped_group_bookmark_actions.h"
#include "components/bookmarks/test/test_bookmark_client.h"
#include "components/browser_apis/bookmarks/bookmarks_view_observer.h"
#include "components/browser_apis/bookmarks/testing/default_bookmarks_view.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace bookmarks_api {

namespace {

base::test::ScopedFeatureList EnableAccountBookmarkStorage() {
  base::test::ScopedFeatureList features;
  features.InitFromCommandLine("SyncEnableBookmarksInTransportMode", "");
  return features;
}

}  // namespace

class DefaultBookmarksViewEventTest : public testing::Test,
                                      public BookmarksViewObserver {
 public:
  DefaultBookmarksViewEventTest() {
    model_ = bookmarks::TestBookmarkClient::CreateModel();
    view_ = std::make_unique<DefaultBookmarksView>(model_.get());
    view_->AddObserver(this);
  }

  ~DefaultBookmarksViewEventTest() override {
    if (view_) {
      view_->RemoveObserver(this);
    }
  }

  // BookmarksViewObserver:
  void OnBookmarksEvents(
      BookmarksView* view,
      const std::vector<mojom::BookmarksEventPtr>& events) override {
    CHECK_EQ(view, view_.get());
    for (const auto& event : events) {
      events_.push_back(event.Clone());
    }
  }

  void ClearEvents() { events_.clear(); }

 protected:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<bookmarks::BookmarkModel> model_;
  std::unique_ptr<DefaultBookmarksView> view_;
  std::vector<mojom::BookmarksEventPtr> events_;
};

TEST_F(DefaultBookmarksViewEventTest, AddBookmark) {
  const bookmarks::BookmarkNode* parent = model_->bookmark_bar_node();
  model_->AddURL(parent, 0, u"Title", GURL("http://example.com"));

  ASSERT_EQ(events_.size(), 1u);
  ASSERT_TRUE(events_[0]->is_added());
  const auto& added = events_[0]->get_added();
  ASSERT_EQ(added->parent_id, view_->GetUuid(parent));
  ASSERT_EQ(added->index, 0);
  ASSERT_TRUE(added->node->is_url());
  ASSERT_TRUE(added->node->get_url()->id.has_value());
  ASSERT_EQ(added->node->get_url()->title, "Title");
  ASSERT_EQ(added->node->get_url()->url, GURL("http://example.com"));
}

TEST_F(DefaultBookmarksViewEventTest, RemoveBookmark) {
  const bookmarks::BookmarkNode* parent = model_->bookmark_bar_node();
  const bookmarks::BookmarkNode* node =
      model_->AddURL(parent, 0, u"Title", GURL("http://example.com"));
  base::Uuid node_uuid = view_->GetUuid(node);
  ClearEvents();

  model_->Remove(node, bookmarks::metrics::BookmarkEditSource::kUser,
                 FROM_HERE);

  ASSERT_EQ(events_.size(), 1u);
  ASSERT_TRUE(events_[0]->is_removed());
  ASSERT_EQ(events_[0]->get_removed()->id, node_uuid);
}

TEST_F(DefaultBookmarksViewEventTest, MoveBookmark) {
  const bookmarks::BookmarkNode* parent = model_->bookmark_bar_node();
  const bookmarks::BookmarkNode* other_parent = model_->other_node();
  const bookmarks::BookmarkNode* node =
      model_->AddURL(parent, 0, u"Title", GURL("http://example.com"));
  ClearEvents();

  model_->Move(node, other_parent, 0);

  ASSERT_EQ(events_.size(), 1u);
  ASSERT_TRUE(events_[0]->is_moved());
  const auto& moved = events_[0]->get_moved();
  ASSERT_EQ(moved->old_parent_id, view_->GetUuid(parent));
  ASSERT_EQ(moved->new_parent_id, view_->GetUuid(other_parent));
  ASSERT_EQ(moved->old_index, 0);
  ASSERT_EQ(moved->new_index, 0);
}

TEST_F(DefaultBookmarksViewEventTest, ChangeBookmark) {
  const bookmarks::BookmarkNode* parent = model_->bookmark_bar_node();
  const bookmarks::BookmarkNode* node =
      model_->AddURL(parent, 0, u"Title", GURL("http://example.com"));
  ClearEvents();

  model_->SetTitle(node, u"New Title",
                   bookmarks::metrics::BookmarkEditSource::kUser);

  ASSERT_EQ(events_.size(), 1u);
  ASSERT_TRUE(events_[0]->is_changed());
  const auto& changed = events_[0]->get_changed();
  ASSERT_TRUE(changed->node->is_url());
  ASSERT_EQ(changed->node->get_url()->title, "New Title");
}

TEST_F(DefaultBookmarksViewEventTest, ReorderChildren) {
  const bookmarks::BookmarkNode* parent = model_->bookmark_bar_node();
  const bookmarks::BookmarkNode* child0 =
      model_->AddURL(parent, 0, u"Child 0", GURL("http://child0.com"));
  const bookmarks::BookmarkNode* child1 =
      model_->AddURL(parent, 1, u"Child 1", GURL("http://child1.com"));
  ClearEvents();

  std::vector<const bookmarks::BookmarkNode*> new_order = {child1, child0};
  model_->ReorderChildren(parent, new_order);

  ASSERT_EQ(events_.size(), 1u);
  ASSERT_TRUE(events_[0]->is_moved());
  const auto& moved = events_[0]->get_moved();
  ASSERT_EQ(moved->old_parent_id, view_->GetUuid(parent));
  ASSERT_EQ(moved->new_parent_id, view_->GetUuid(parent));
  ASSERT_EQ(moved->old_index, 1);
  ASSERT_EQ(moved->new_index, 0);
}

TEST_F(DefaultBookmarksViewEventTest, ReorderNestedFolderChildren) {
  const bookmarks::BookmarkNode* parent = model_->bookmark_bar_node();
  const bookmarks::BookmarkNode* folder =
      model_->AddFolder(parent, 0, u"Folder");
  const bookmarks::BookmarkNode* child0 =
      model_->AddURL(folder, 0, u"Child 0", GURL("http://child0.com"));
  const bookmarks::BookmarkNode* child1 =
      model_->AddURL(folder, 1, u"Child 1", GURL("http://child1.com"));
  ClearEvents();

  std::vector<const bookmarks::BookmarkNode*> new_order = {child1, child0};
  model_->ReorderChildren(folder, new_order);

  ASSERT_EQ(events_.size(), 1u);
  ASSERT_TRUE(events_[0]->is_moved());
  const auto& moved = events_[0]->get_moved();
  ASSERT_EQ(moved->old_parent_id, view_->GetUuid(folder));
  ASSERT_EQ(moved->new_parent_id, view_->GetUuid(folder));
  ASSERT_EQ(moved->old_index, 1);
  ASSERT_EQ(moved->new_index, 0);
}

TEST_F(DefaultBookmarksViewEventTest, ReorderAfterMoveBetweenFolders) {
  const bookmarks::BookmarkNode* parent = model_->bookmark_bar_node();
  const bookmarks::BookmarkNode* folder =
      model_->AddFolder(parent, 0, u"Folder");
  const bookmarks::BookmarkNode* child0 =
      model_->AddURL(parent, 1, u"Child 0", GURL("http://child0.com"));
  const bookmarks::BookmarkNode* child1 =
      model_->AddURL(parent, 2, u"Child 1", GURL("http://child1.com"));

  model_->Move(child0, folder, 0);
  model_->Move(child1, folder, 1);
  ClearEvents();

  std::vector<const bookmarks::BookmarkNode*> new_order = {child1, child0};
  model_->ReorderChildren(folder, new_order);

  ASSERT_EQ(events_.size(), 1u);
  ASSERT_TRUE(events_[0]->is_moved());
  const auto& moved = events_[0]->get_moved();
  ASSERT_EQ(moved->old_parent_id, view_->GetUuid(folder));
  ASSERT_EQ(moved->new_parent_id, view_->GetUuid(folder));
  ASSERT_EQ(moved->old_index, 1);
  ASSERT_EQ(moved->new_index, 0);
}

TEST_F(DefaultBookmarksViewEventTest, ReorderAfterSameParentMove) {
  const bookmarks::BookmarkNode* parent = model_->bookmark_bar_node();
  const bookmarks::BookmarkNode* child0 =
      model_->AddURL(parent, 0, u"Child 0", GURL("http://child0.com"));
  const bookmarks::BookmarkNode* child1 =
      model_->AddURL(parent, 1, u"Child 1", GURL("http://child1.com"));
  const bookmarks::BookmarkNode* child2 =
      model_->AddURL(parent, 2, u"Child 2", GURL("http://child2.com"));

  model_->Move(child0, parent, 3);  // Order becomes child1, child2, child0
  ClearEvents();

  std::vector<const bookmarks::BookmarkNode*> new_order = {child0, child1,
                                                           child2};
  model_->ReorderChildren(parent, new_order);

  ASSERT_EQ(events_.size(), 1u);
  ASSERT_TRUE(events_[0]->is_moved());
  const auto& moved = events_[0]->get_moved();
  ASSERT_EQ(moved->old_parent_id, view_->GetUuid(parent));
  ASSERT_EQ(moved->new_parent_id, view_->GetUuid(parent));
  ASSERT_EQ(moved->old_index, 2);
  ASSERT_EQ(moved->new_index, 0);
}

TEST_F(DefaultBookmarksViewEventTest, ReorderDuringExtensiveChanges) {
  const bookmarks::BookmarkNode* parent = model_->bookmark_bar_node();

  model_->BeginExtensiveChanges();
  const bookmarks::BookmarkNode* child0 =
      model_->AddURL(parent, 0, u"Child 0", GURL("http://child0.com"));
  const bookmarks::BookmarkNode* child1 =
      model_->AddURL(parent, 1, u"Child 1", GURL("http://child1.com"));

  // Events are queued, not delivered, while the batch is in progress.
  ASSERT_TRUE(events_.empty());

  std::vector<const bookmarks::BookmarkNode*> new_order = {child1, child0};
  model_->ReorderChildren(parent, new_order);
  ASSERT_TRUE(events_.empty());

  model_->EndExtensiveChanges();

  // The two adds and the single reorder move are flushed together, in order.
  ASSERT_EQ(events_.size(), 3u);
  ASSERT_TRUE(events_[0]->is_added());
  ASSERT_TRUE(events_[1]->is_added());
  ASSERT_TRUE(events_[2]->is_moved());
  const auto& moved = events_[2]->get_moved();
  ASSERT_EQ(moved->old_parent_id, view_->GetUuid(parent));
  ASSERT_EQ(moved->old_index, 1);
  ASSERT_EQ(moved->new_parent_id, view_->GetUuid(parent));
  ASSERT_EQ(moved->new_index, 0);
}

TEST_F(DefaultBookmarksViewEventTest, RemoveAllUserBookmarks) {
  const bookmarks::BookmarkNode* parent = model_->bookmark_bar_node();
  const bookmarks::BookmarkNode* node =
      model_->AddURL(parent, 0, u"Title", GURL("http://example.com"));
  base::Uuid node_uuid = view_->GetUuid(node);
  ClearEvents();

  model_->RemoveAllUserBookmarks(FROM_HERE);

  ASSERT_EQ(events_.size(), 1u);
  ASSERT_TRUE(events_[0]->is_removed());
  ASSERT_EQ(events_[0]->get_removed()->id, node_uuid);
}

class DefaultBookmarksViewEventAccountTest : public testing::Test,
                                             public BookmarksViewObserver {
 public:
  DefaultBookmarksViewEventAccountTest() {
    model_ = bookmarks::TestBookmarkClient::CreateModel();
    model_->CreateAccountPermanentFolders();
    view_ = std::make_unique<DefaultBookmarksView>(model_.get());
    view_->AddObserver(this);
  }

  ~DefaultBookmarksViewEventAccountTest() override {
    if (view_) {
      view_->RemoveObserver(this);
    }
  }

  // BookmarksViewObserver:
  void OnBookmarksEvents(
      BookmarksView* view,
      const std::vector<mojom::BookmarksEventPtr>& events) override {
    CHECK_EQ(view, view_.get());
    for (const auto& event : events) {
      events_.push_back(event.Clone());
    }
  }

  void ClearEvents() { events_.clear(); }

 protected:
  base::test::ScopedFeatureList features_ = EnableAccountBookmarkStorage();
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<bookmarks::BookmarkModel> model_;
  std::unique_ptr<DefaultBookmarksView> view_;
  std::vector<mojom::BookmarksEventPtr> events_;
};

TEST_F(DefaultBookmarksViewEventAccountTest,
       RemoveAllUserBookmarksIncludesAccountFolders) {
  const bookmarks::BookmarkNode* local_bar = model_->bookmark_bar_node();
  const bookmarks::BookmarkNode* account_bar =
      model_->account_bookmark_bar_node();
  ASSERT_NE(account_bar, nullptr);

  const bookmarks::BookmarkNode* local_node =
      model_->AddURL(local_bar, 0, u"Local", GURL("http://local.com"));
  const bookmarks::BookmarkNode* account_node =
      model_->AddURL(account_bar, 0, u"Account", GURL("http://account.com"));
  const base::Uuid local_uuid = view_->GetUuid(local_node);
  const base::Uuid account_uuid = view_->GetUuid(account_node);
  ClearEvents();

  model_->RemoveAllUserBookmarks(FROM_HERE);

  ASSERT_EQ(events_.size(), 2u);
  ASSERT_TRUE(events_[0]->is_removed());
  ASSERT_EQ(events_[0]->get_removed()->id, local_uuid);
  ASSERT_TRUE(events_[1]->is_removed());
  ASSERT_EQ(events_[1]->get_removed()->id, account_uuid);
}

TEST_F(DefaultBookmarksViewEventAccountTest,
       ReorderAfterCrossStorageFolderMoveWithUuidReassignment) {
  const bookmarks::BookmarkNode* local_bar = model_->bookmark_bar_node();
  const bookmarks::BookmarkNode* account_bar =
      model_->account_bookmark_bar_node();
  ASSERT_NE(account_bar, nullptr);

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

  model_->Move(folder, account_bar, 1);
  ASSERT_NE(child0->uuid(), shared_uuid);
  ClearEvents();

  std::vector<const bookmarks::BookmarkNode*> new_order = {child1, child0};
  model_->ReorderChildren(folder, new_order);

  ASSERT_EQ(events_.size(), 1u);
  ASSERT_TRUE(events_[0]->is_moved());
  const auto& moved = events_[0]->get_moved();
  ASSERT_EQ(moved->old_parent_id, view_->GetUuid(folder));
  ASSERT_EQ(moved->new_parent_id, view_->GetUuid(folder));
  ASSERT_EQ(moved->old_index, 1);
  ASSERT_EQ(moved->new_index, 0);
}

}  // namespace bookmarks_api
