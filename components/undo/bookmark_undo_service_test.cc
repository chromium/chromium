// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/undo/bookmark_undo_service.h"

#include <stddef.h>

#include <memory>

#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/common/bookmark_metrics.h"
#include "components/bookmarks/test/bookmark_test_helpers.h"
#include "components/bookmarks/test/test_bookmark_client.h"
#include "components/sync/base/features.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::ASCIIToUTF16;
using bookmarks::BookmarkModel;
using bookmarks::BookmarkNode;

namespace {

// TestBookmarkClient that supports undoing removals.
class TestBookmarkClientWithUndo : public bookmarks::TestBookmarkClient {
 public:
  explicit TestBookmarkClientWithUndo(BookmarkUndoService* undo_service)
      : undo_service_(undo_service) {}

  ~TestBookmarkClientWithUndo() override = default;

  // BookmarkClient overrides.
  void OnBookmarkNodeRemovedUndoable(
      const BookmarkNode* parent,
      size_t index,
      std::unique_ptr<BookmarkNode> node) override {
    undo_service_->AddUndoEntryForRemovedNode(parent, index, std::move(node));
  }

 private:
  const raw_ptr<BookmarkUndoService, DanglingUntriaged> undo_service_;
};

class BookmarkUndoServiceTest : public testing::Test {
 public:
  BookmarkUndoServiceTest();

  BookmarkUndoServiceTest(const BookmarkUndoServiceTest&) = delete;
  BookmarkUndoServiceTest& operator=(const BookmarkUndoServiceTest&) = delete;

  void SetUp() override;
  void TearDown() override;

  BookmarkModel* GetModel();
  BookmarkUndoService* GetUndoService();

 private:
  base::test::ScopedFeatureList features_{
      syncer::kSyncEnableBookmarksInTransportMode};
  std::unique_ptr<BookmarkUndoService> bookmark_undo_service_;
  std::unique_ptr<bookmarks::BookmarkModel> bookmark_model_;
};

BookmarkUndoServiceTest::BookmarkUndoServiceTest() = default;

void BookmarkUndoServiceTest::SetUp() {
  DCHECK(!bookmark_model_);
  DCHECK(!bookmark_undo_service_);
  bookmark_undo_service_ = std::make_unique<BookmarkUndoService>();
  bookmark_model_ = bookmarks::TestBookmarkClient::CreateModelWithClient(
      std::make_unique<TestBookmarkClientWithUndo>(
          bookmark_undo_service_.get()));
  bookmark_undo_service_->StartObservingBookmarkModel(bookmark_model_.get());
  bookmarks::test::WaitForBookmarkModelToLoad(bookmark_model_.get());
}

BookmarkModel* BookmarkUndoServiceTest::GetModel() {
  return bookmark_model_.get();
}

BookmarkUndoService* BookmarkUndoServiceTest::GetUndoService() {
  return bookmark_undo_service_.get();
}

void BookmarkUndoServiceTest::TearDown() {
  // Implement two-phase KeyedService shutdown for test KeyedServices.
  bookmark_undo_service_->Shutdown();
  bookmark_model_->Shutdown();
  bookmark_undo_service_.reset();
  bookmark_model_.reset();
}

TEST_F(BookmarkUndoServiceTest, AddBookmark) {
  BookmarkModel* model = GetModel();
  BookmarkUndoService* undo_service = GetUndoService();

  const BookmarkNode* parent = model->other_node();
  model->AddURL(parent, 0, u"foo", GURL("http://www.bar.com"));

  // Undo bookmark creation and test for no bookmarks.
  undo_service->undo_manager()->Undo();
  EXPECT_EQ(0u, model->other_node()->children().size());

  // Redo bookmark creation and ensure bookmark information is valid.
  undo_service->undo_manager()->Redo();
  const BookmarkNode* node = parent->children().front().get();
  EXPECT_EQ(node->GetTitle(), u"foo");
  EXPECT_EQ(node->url(), GURL("http://www.bar.com"));
}

// Test that a bookmark removal action can be undone and redone.
TEST_F(BookmarkUndoServiceTest, UndoBookmarkRemove) {
  BookmarkModel* model = GetModel();
  BookmarkUndoService* undo_service = GetUndoService();

  const BookmarkNode* parent = model->other_node();
  model->AddURL(parent, 0, u"foo", GURL("http://www.bar.com"));
  model->Remove(parent->children().front().get(),
                bookmarks::metrics::BookmarkEditSource::kOther, FROM_HERE);

  EXPECT_EQ(2U, undo_service->undo_manager()->undo_count());
  EXPECT_EQ(0U, undo_service->undo_manager()->redo_count());

  // Undo the deletion of the only bookmark and check the bookmark values.
  undo_service->undo_manager()->Undo();
  EXPECT_EQ(1u, model->other_node()->children().size());
  const BookmarkNode* node = parent->children().front().get();
  EXPECT_EQ(node->GetTitle(), u"foo");
  EXPECT_EQ(node->url(), GURL("http://www.bar.com"));

  EXPECT_EQ(1U, undo_service->undo_manager()->undo_count());
  EXPECT_EQ(1U, undo_service->undo_manager()->redo_count());

  // Redo the deletion and check that there are no bookmarks left.
  undo_service->undo_manager()->Redo();
  EXPECT_EQ(0u, model->other_node()->children().size());

  EXPECT_EQ(2U, undo_service->undo_manager()->undo_count());
  EXPECT_EQ(0U, undo_service->undo_manager()->redo_count());
}

// Ensure the undo/redo works for editing of bookmark information grouped into
// one action.
TEST_F(BookmarkUndoServiceTest, UndoBookmarkGroupedAction) {
  BookmarkModel* model = GetModel();
  BookmarkUndoService* undo_service = GetUndoService();

  const BookmarkNode* n1 =
      model->AddURL(model->other_node(), 0, u"foo", GURL("http://www.foo.com"));
  undo_service->undo_manager()->StartGroupingActions();
  model->SetTitle(n1, u"bar", bookmarks::metrics::BookmarkEditSource::kOther);
  model->SetURL(n1, GURL("http://www.bar.com"),
                bookmarks::metrics::BookmarkEditSource::kOther);
  undo_service->undo_manager()->EndGroupingActions();

  EXPECT_EQ(2U, undo_service->undo_manager()->undo_count());
  EXPECT_EQ(0U, undo_service->undo_manager()->redo_count());

  // Undo the modification of the bookmark and check for the original values.
  undo_service->undo_manager()->Undo();
  EXPECT_EQ(1u, model->other_node()->children().size());
  const BookmarkNode* node = model->other_node()->children().front().get();
  EXPECT_EQ(node->GetTitle(), u"foo");
  EXPECT_EQ(node->url(), GURL("http://www.foo.com"));

  // Redo the modifications and ensure the newer values are present.
  undo_service->undo_manager()->Redo();
  EXPECT_EQ(1u, model->other_node()->children().size());
  node = model->other_node()->children().front().get();
  EXPECT_EQ(node->GetTitle(), u"bar");
  EXPECT_EQ(node->url(), GURL("http://www.bar.com"));

  EXPECT_EQ(2U, undo_service->undo_manager()->undo_count());
  EXPECT_EQ(0U, undo_service->undo_manager()->redo_count());
}

// Test moving bookmarks within a folder and between folders.
TEST_F(BookmarkUndoServiceTest, UndoBookmarkMoveWithinFolder) {
  BookmarkModel* model = GetModel();
  BookmarkUndoService* undo_service = GetUndoService();

  const BookmarkNode* n1 =
      model->AddURL(model->other_node(), 0, u"foo", GURL("http://www.foo.com"));
  const BookmarkNode* n2 =
      model->AddURL(model->other_node(), 1, u"moo", GURL("http://www.moo.com"));
  const BookmarkNode* n3 =
      model->AddURL(model->other_node(), 2, u"bar", GURL("http://www.bar.com"));
  model->Move(n1, model->other_node(), 3);

  // Undo the move and check that the nodes are in order.
  undo_service->undo_manager()->Undo();
  EXPECT_EQ(model->other_node()->children()[0].get(), n1);
  EXPECT_EQ(model->other_node()->children()[1].get(), n2);
  EXPECT_EQ(model->other_node()->children()[2].get(), n3);

  // Redo the move and check that the first node is in the last position.
  undo_service->undo_manager()->Redo();
  EXPECT_EQ(model->other_node()->children()[0].get(), n2);
  EXPECT_EQ(model->other_node()->children()[1].get(), n3);
  EXPECT_EQ(model->other_node()->children()[2].get(), n1);
}

// Test moving bookmarks across NodeTypeForUuidLookup boundaries.
TEST_F(BookmarkUndoServiceTest, UndoBookmarkMoveAcrossNodeTypeForUuidLookup) {
  BookmarkModel* model = GetModel();
  BookmarkUndoService* undo_service = GetUndoService();

  model->CreateAccountPermanentFolders();
  ASSERT_NE(nullptr, model->account_other_node());
  ASSERT_NE(model->other_node(), model->account_other_node());

  const BookmarkNode* n1 =
      model->AddURL(model->other_node(), 0, u"foo", GURL("http://www.foo.com"));

  ASSERT_EQ(1u, model->other_node()->children().size());
  ASSERT_EQ(0u, model->account_other_node()->children().size());
  ASSERT_EQ(n1,
            model->GetNodeByUuid(
                n1->uuid(),
                BookmarkModel::NodeTypeForUuidLookup::kLocalOrSyncableNodes));
  ASSERT_EQ(nullptr, model->GetNodeByUuid(
                         n1->uuid(),
                         BookmarkModel::NodeTypeForUuidLookup::kAccountNodes));

  // Move from kLocalOrSyncableNodes to kAccountNodes.
  model->Move(n1, model->account_other_node(), 0);

  ASSERT_EQ(0u, model->other_node()->children().size());
  ASSERT_EQ(1u, model->account_other_node()->children().size());
  ASSERT_EQ(nullptr,
            model->GetNodeByUuid(
                n1->uuid(),
                BookmarkModel::NodeTypeForUuidLookup::kLocalOrSyncableNodes));
  ASSERT_EQ(
      n1, model->GetNodeByUuid(
              n1->uuid(), BookmarkModel::NodeTypeForUuidLookup::kAccountNodes));

  // Undo the move and check that it was moved back to kLocalOrSyncableNodes.
  undo_service->undo_manager()->Undo();

  EXPECT_EQ(1u, model->other_node()->children().size());
  EXPECT_EQ(0u, model->account_other_node()->children().size());
  EXPECT_EQ(n1,
            model->GetNodeByUuid(
                n1->uuid(),
                BookmarkModel::NodeTypeForUuidLookup::kLocalOrSyncableNodes));
  EXPECT_EQ(nullptr, model->GetNodeByUuid(
                         n1->uuid(),
                         BookmarkModel::NodeTypeForUuidLookup::kAccountNodes));

  // Redo the move and check that it moves again to kAccountNodes.
  undo_service->undo_manager()->Redo();

  EXPECT_EQ(0u, model->other_node()->children().size());
  EXPECT_EQ(1u, model->account_other_node()->children().size());
  EXPECT_EQ(nullptr,
            model->GetNodeByUuid(
                n1->uuid(),
                BookmarkModel::NodeTypeForUuidLookup::kLocalOrSyncableNodes));
  EXPECT_EQ(
      n1, model->GetNodeByUuid(
              n1->uuid(), BookmarkModel::NodeTypeForUuidLookup::kAccountNodes));
}

// Test undo of a bookmark moved to a different folder.
TEST_F(BookmarkUndoServiceTest, UndoBookmarkMoveToOtherFolder) {
  BookmarkModel* model = GetModel();
  BookmarkUndoService* undo_service = GetUndoService();

  const BookmarkNode* n1 =
      model->AddURL(model->other_node(), 0, u"foo", GURL("http://www.foo.com"));
  const BookmarkNode* n2 =
      model->AddURL(model->other_node(), 1, u"moo", GURL("http://www.moo.com"));
  const BookmarkNode* n3 =
      model->AddURL(model->other_node(), 2, u"bar", GURL("http://www.bar.com"));
  const BookmarkNode* f1 = model->AddFolder(model->other_node(), 3, u"folder");
  model->Move(n3, f1, 0);

  // Undo the move and check that the bookmark and folder are in place.
  undo_service->undo_manager()->Undo();
  ASSERT_EQ(4u, model->other_node()->children().size());
  EXPECT_EQ(model->other_node()->children()[0].get(), n1);
  EXPECT_EQ(model->other_node()->children()[1].get(), n2);
  EXPECT_EQ(model->other_node()->children()[2].get(), n3);
  EXPECT_EQ(model->other_node()->children()[3].get(), f1);
  EXPECT_EQ(0u, f1->children().size());

  // Redo the move back into the folder and check validity.
  undo_service->undo_manager()->Redo();
  ASSERT_EQ(3u, model->other_node()->children().size());
  EXPECT_EQ(model->other_node()->children()[0].get(), n1);
  EXPECT_EQ(model->other_node()->children()[1].get(), n2);
  EXPECT_EQ(model->other_node()->children()[2].get(), f1);
  ASSERT_EQ(1u, f1->children().size());
  EXPECT_EQ(f1->children().front().get(), n3);
}

// Tests the handling of multiple modifications that include renumbering of the
// bookmark identifiers.
TEST_F(BookmarkUndoServiceTest, UndoBookmarkRenameDelete) {
  BookmarkModel* model = GetModel();
  BookmarkUndoService* undo_service = GetUndoService();

  const BookmarkNode* f1 = model->AddFolder(model->other_node(), 0, u"folder");
  model->AddURL(f1, 0, u"foo", GURL("http://www.foo.com"));
  model->SetTitle(f1, u"Renamed",
                  bookmarks::metrics::BookmarkEditSource::kOther);
  model->Remove(model->other_node()->children().front().get(),
                bookmarks::metrics::BookmarkEditSource::kOther, FROM_HERE);

  // Undo the folder removal and ensure the folder and bookmark were restored.
  undo_service->undo_manager()->Undo();
  ASSERT_EQ(1u, model->other_node()->children().size());
  ASSERT_EQ(1u, model->other_node()->children().front()->children().size());
  const BookmarkNode* node = model->other_node()->children().front().get();
  EXPECT_EQ(node->GetTitle(), u"Renamed");

  node = model->other_node()->children().front()->children().front().get();
  EXPECT_EQ(node->GetTitle(), u"foo");
  EXPECT_EQ(node->url(), GURL("http://www.foo.com"));

  // Undo the title change and ensure the folder was updated even though the
  // id has changed.
  undo_service->undo_manager()->Undo();
  node = model->other_node()->children().front().get();
  EXPECT_EQ(node->GetTitle(), u"folder");

  // Undo bookmark creation and test for removal of bookmark.
  undo_service->undo_manager()->Undo();
  ASSERT_EQ(0u, model->other_node()->children().front()->children().size());

  // Undo folder creation and confirm the bookmark model is empty.
  undo_service->undo_manager()->Undo();
  ASSERT_EQ(0u, model->other_node()->children().size());

  // Redo all the actions and ensure the folder and bookmark are restored.
  undo_service->undo_manager()->Redo(); // folder creation
  undo_service->undo_manager()->Redo(); // bookmark creation
  undo_service->undo_manager()->Redo(); // bookmark title change
  ASSERT_EQ(1u, model->other_node()->children().size());
  ASSERT_EQ(1u, model->other_node()->children().front()->children().size());
  node = model->other_node()->children().front().get();
  EXPECT_EQ(node->GetTitle(), u"Renamed");
  node = model->other_node()->children().front()->children().front().get();
  EXPECT_EQ(node->GetTitle(), u"foo");
  EXPECT_EQ(node->url(), GURL("http://www.foo.com"));

  undo_service->undo_manager()->Redo(); // folder deletion
  EXPECT_EQ(0u, model->other_node()->children().size());
}

// Test the undo of SortChildren and ReorderChildren.
TEST_F(BookmarkUndoServiceTest, UndoBookmarkReorder) {
  BookmarkModel* model = GetModel();
  BookmarkUndoService* undo_service = GetUndoService();

  const BookmarkNode* parent = model->other_node();
  model->AddURL(parent, 0, u"foo", GURL("http://www.foo.com"));
  model->AddURL(parent, 1, u"moo", GURL("http://www.moo.com"));
  model->AddURL(parent, 2, u"bar", GURL("http://www.bar.com"));
  model->SortChildren(parent);

  // Test the undo of SortChildren.
  undo_service->undo_manager()->Undo();
  const BookmarkNode* node = parent->children()[0].get();
  EXPECT_EQ(node->GetTitle(), u"foo");
  EXPECT_EQ(node->url(), GURL("http://www.foo.com"));

  node = parent->children()[1].get();
  EXPECT_EQ(node->GetTitle(), u"moo");
  EXPECT_EQ(node->url(), GURL("http://www.moo.com"));

  node = parent->children()[2].get();
  EXPECT_EQ(node->GetTitle(), u"bar");
  EXPECT_EQ(node->url(), GURL("http://www.bar.com"));

  // Test the redo of SortChildren.
  undo_service->undo_manager()->Redo();
  node = parent->children()[0].get();
  EXPECT_EQ(node->GetTitle(), u"bar");
  EXPECT_EQ(node->url(), GURL("http://www.bar.com"));

  node = parent->children()[1].get();
  EXPECT_EQ(node->GetTitle(), u"foo");
  EXPECT_EQ(node->url(), GURL("http://www.foo.com"));

  node = parent->children()[2].get();
  EXPECT_EQ(node->GetTitle(), u"moo");
  EXPECT_EQ(node->url(), GURL("http://www.moo.com"));

}

TEST_F(BookmarkUndoServiceTest, UndoBookmarkRemoveAll) {
  BookmarkModel* model = GetModel();
  BookmarkUndoService* undo_service = GetUndoService();

  // Setup bookmarks in the Other Bookmarks and the Bookmark Bar.
  const BookmarkNode* new_folder;
  const BookmarkNode* parent = model->other_node();
  model->AddURL(parent, 0, u"foo", GURL("http://www.google.com"));
  new_folder = model->AddFolder(parent, 1, u"folder");
  model->AddURL(new_folder, 0, u"bar", GURL("http://www.bar.com"));

  parent = model->bookmark_bar_node();
  model->AddURL(parent, 0, u"a", GURL("http://www.a.com"));
  new_folder = model->AddFolder(parent, 1, u"folder");
  model->AddURL(new_folder, 0, u"b", GURL("http://www.b.com"));

  model->RemoveAllUserBookmarks(FROM_HERE);

  // Test that the undo of RemoveAllUserBookmarks restores all folders and
  // bookmarks.
  undo_service->undo_manager()->Undo();

  ASSERT_EQ(2u, model->other_node()->children().size());
  EXPECT_EQ(1u, model->other_node()->children()[1]->children().size());
  const BookmarkNode* node =
      model->other_node()->children()[1]->children()[0].get();
  EXPECT_EQ(node->GetTitle(), u"bar");
  EXPECT_EQ(node->url(), GURL("http://www.bar.com"));

  ASSERT_EQ(2u, model->bookmark_bar_node()->children().size());
  EXPECT_EQ(1u, model->bookmark_bar_node()->children()[1]->children().size());
  node = model->bookmark_bar_node()->children()[1]->children()[0].get();
  EXPECT_EQ(node->GetTitle(), u"b");
  EXPECT_EQ(node->url(), GURL("http://www.b.com"));

  // Test that the redo removes all folders and bookmarks.
  undo_service->undo_manager()->Redo();
  EXPECT_EQ(0u, model->other_node()->children().size());
  EXPECT_EQ(0u, model->bookmark_bar_node()->children().size());
}

TEST_F(BookmarkUndoServiceTest, UndoRemoveFolderWithBookmarks) {
  BookmarkModel* model = GetModel();
  BookmarkUndoService* undo_service = GetUndoService();

  // Setup bookmarks in the Other Bookmarks.
  const BookmarkNode* new_folder;
  const BookmarkNode* parent = model->other_node();
  new_folder = model->AddFolder(parent, 0, u"folder");
  model->AddURL(new_folder, 0, u"bar", GURL("http://www.bar.com"));

  model->Remove(parent->children().front().get(),
                bookmarks::metrics::BookmarkEditSource::kOther, FROM_HERE);

  // Test that the undo restores the bookmark and folder.
  undo_service->undo_manager()->Undo();

  ASSERT_EQ(1u, model->other_node()->children().size());
  new_folder = model->other_node()->children().front().get();
  EXPECT_EQ(1u, new_folder->children().size());
  const BookmarkNode* node = new_folder->children().front().get();
  EXPECT_EQ(node->GetTitle(), u"bar");
  EXPECT_EQ(node->url(), GURL("http://www.bar.com"));

  // Test that the redo restores the bookmark and folder.
  undo_service->undo_manager()->Redo();

  ASSERT_EQ(0u, model->other_node()->children().size());

  // Test that the undo after a redo restores the bookmark and folder.
  undo_service->undo_manager()->Undo();

  ASSERT_EQ(1u, model->other_node()->children().size());
  new_folder = model->other_node()->children().front().get();
  EXPECT_EQ(1u, new_folder->children().size());
  node = new_folder->children().front().get();
  EXPECT_EQ(node->GetTitle(), u"bar");
  EXPECT_EQ(node->url(), GURL("http://www.bar.com"));
}

TEST_F(BookmarkUndoServiceTest, UndoRemoveFolderWithSubfolders) {
  BookmarkModel* model = GetModel();
  BookmarkUndoService* undo_service = GetUndoService();

  // Setup bookmarks in the Other Bookmarks with the following structure:
  // folder
  //    subfolder1
  //    subfolder2
  //        bar - http://www.bar.com
  // This setup of multiple subfolders where the first subfolder has 0 children
  // is designed specifically to ensure we do not crash in this scenario and
  // that bookmarks are restored to the proper subfolder. See crbug.com/474123.
  const BookmarkNode* parent = model->other_node();
  const BookmarkNode* new_folder = model->AddFolder(parent, 0, u"folder");
  model->AddFolder(new_folder, 0, u"subfolder1");
  const BookmarkNode* sub_folder2 =
      model->AddFolder(new_folder, 1, u"subfolder2");
  model->AddURL(sub_folder2, 0, u"bar", GURL("http://www.bar.com"));

  model->Remove(parent->children()[0].get(),
                bookmarks::metrics::BookmarkEditSource::kOther, FROM_HERE);

  // Test that the undo restores the subfolders and their contents.
  undo_service->undo_manager()->Undo();

  ASSERT_EQ(1u, model->other_node()->children().size());
  const BookmarkNode* restored_new_folder =
      model->other_node()->children()[0].get();
  EXPECT_EQ(2u, restored_new_folder->children().size());

  const BookmarkNode* restored_sub_folder1 =
      restored_new_folder->children()[0].get();
  EXPECT_EQ(u"subfolder1", restored_sub_folder1->GetTitle());
  EXPECT_EQ(0u, restored_sub_folder1->children().size());

  const BookmarkNode* restored_sub_folder2 =
      restored_new_folder->children()[1].get();
  EXPECT_EQ(u"subfolder2", restored_sub_folder2->GetTitle());
  EXPECT_EQ(1u, restored_sub_folder2->children().size());

  const BookmarkNode* node = restored_sub_folder2->children()[0].get();
  EXPECT_EQ(node->GetTitle(), u"bar");
  EXPECT_EQ(node->url(), GURL("http://www.bar.com"));
}

TEST_F(BookmarkUndoServiceTest, TestUpperLimit) {
  BookmarkModel* model = GetModel();
  BookmarkUndoService* undo_service = GetUndoService();

  // This maximum is set in undo_manager.cc
  const size_t kMaxUndoGroups = 100;

  const BookmarkNode* parent = model->other_node();
  model->AddURL(parent, 0, u"foo", GURL("http://www.foo.com"));
  for (size_t i = 1; i < kMaxUndoGroups + 1; ++i)
    model->AddURL(parent, i, u"bar", GURL("http://www.bar.com"));

  EXPECT_EQ(kMaxUndoGroups, undo_service->undo_manager()->undo_count());

  // Undo as many operations as possible.
  while (undo_service->undo_manager()->undo_count())
    undo_service->undo_manager()->Undo();

  EXPECT_EQ(1u, parent->children().size());
  const BookmarkNode* node = model->other_node()->children().front().get();
  EXPECT_EQ(node->GetTitle(), u"foo");
  EXPECT_EQ(node->url(), GURL("http://www.foo.com"));
}

} // namespace
