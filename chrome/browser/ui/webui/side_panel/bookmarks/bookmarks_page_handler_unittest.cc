// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/side_panel/bookmarks/bookmarks_page_handler.h"

#include <memory>

#include "base/strings/utf_string_conversions.h"
#include "base/test/mock_callback.h"
#include "chrome/browser/bookmarks/bookmark_merged_surface_service.h"
#include "chrome/browser/bookmarks/bookmark_test_utils.h"
#include "chrome/browser/ui/webui/bookmarks/bookmark_prefs.h"
#include "chrome/browser/ui/webui/side_panel/bookmarks/bookmarks.mojom.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "components/bookmarks/managed/managed_bookmark_service.h"
#include "components/bookmarks/test/bookmark_test_helpers.h"
#include "components/bookmarks/test/test_bookmark_client.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using bookmarks::test::AddNodesFromModelString;
using ::testing::Contains;
using ::testing::SizeIs;

// Compares a `side_panel::mojom::BookmarksTreeNodePtr` (arg) with a
// `bookmarks::BookmarkNode` (node) by comparing their `id` and the ids of their
// children recursively.
// Note: the matching node `arg` may contain more children than the expected
// `node` since it could be the representation of a merged node. Therefore we
// only make sure that `node`'s children are a subset of `arg`'s children if
// `arg` actually contains any children.
MATCHER_P2(MatchesNode, node, service, "") {
  const std::string expected_id =
      node->is_folder()
          ? GetFolderSidePanelIDForTesting(
                *service, BookmarkParentFolder::FromFolderNode(node))
          : base::ToString(node->id());

  if (arg->id != expected_id) {
    return false;
  }

  // If the matching node has no children, then expected node shouldn't as well.
  if (!arg->children.has_value() || arg->children->empty()) {
    return node->children().empty();
  }

  // The expected node should have its children contained in the matching node
  // with the same id.
  for (const auto& child : node->children()) {
    EXPECT_THAT(arg->children.value(),
                Contains(MatchesNode(child.get(), service)))
        << "Some child nodes of '" << node->GetTitle()
        << "' are not contained in the matching node with the same id.";
  }

  return true;
}

class MockBookmarksPage : public side_panel::mojom::BookmarksPage {
 public:
  MockBookmarksPage() = default;
  ~MockBookmarksPage() override = default;

  mojo::PendingRemote<side_panel::mojom::BookmarksPage> BindAndGetRemote() {
    DCHECK(!receiver_.is_bound());
    return receiver_.BindNewPipeAndPassRemote();
  }

  void FlushForTesting() { receiver_.FlushForTesting(); }

  void Reset() { receiver_.reset(); }

  MOCK_METHOD(void,
              OnBookmarkNodeAdded,
              (side_panel::mojom::BookmarksTreeNodePtr));

 private:
  mojo::Receiver<side_panel::mojom::BookmarksPage> receiver_{this};
};

class BookmarksPageHandlerTest : public BrowserWithTestWindowTest {
 public:
  void SetUp() override {
    BrowserWithTestWindowTest::SetUp();
    CreateHandler();
    LoadBookmarkModel();
  }

  void TearDown() override {
    ClearHandler();
    BrowserWithTestWindowTest::TearDown();
  }

  BookmarksPageHandler* handler() { return handler_.get(); }
  bookmarks::BookmarkModel* model() { return bookmark_model_.get(); }
  bookmarks::ManagedBookmarkService* managed_bookmark_service() {
    return managed_bookmark_service_.get();
  }
  BookmarkMergedSurfaceService* service() {
    return bookmark_merged_service_.get();
  }
  testing::NiceMock<MockBookmarksPage>& mock_bookmarks_page() {
    return mock_bookmarks_page_;
  }

  void CreateHandler(int managed_bookmarks_count = 0) {
    CHECK(!handler_);

    prefs_ = std::make_unique<sync_preferences::TestingPrefServiceSyncable>();

    managed_bookmark_service_ =
        CreateManagedBookmarkService(prefs_.get(), managed_bookmarks_count);
    bookmark_model_ = std::make_unique<bookmarks::BookmarkModel>(
        std::make_unique<TestBookmarkClientWithManagedService>(
            managed_bookmark_service_.get()));

    bookmark_merged_service_ = std::make_unique<BookmarkMergedSurfaceService>(
        bookmark_model_.get(), managed_bookmark_service_.get());

    handler_ = std::make_unique<BookmarksPageHandler>(
        mojo::PendingReceiver<side_panel::mojom::BookmarksPageHandler>(),
        mock_bookmarks_page_.BindAndGetRemote(),
        /*bookmarks_ui=*/nullptr, bookmark_merged_service_.get());
  }

  void LoadBookmarkModel() {
    bookmark_model_->LoadEmptyForTest();
    bookmark_merged_service_->LoadForTesting({});
  }

  void ClearHandler() {
    handler_.reset();
    mock_bookmarks_page_.Reset();
    bookmark_merged_service_.reset();
    bookmark_model_.reset();
    managed_bookmark_service_.reset();
    prefs_.reset();
  }

 private:
  std::unique_ptr<sync_preferences::TestingPrefServiceSyncable> prefs_;

  std::unique_ptr<bookmarks::ManagedBookmarkService> managed_bookmark_service_;
  std::unique_ptr<bookmarks::BookmarkModel> bookmark_model_;
  std::unique_ptr<BookmarkMergedSurfaceService> bookmark_merged_service_;

  testing::NiceMock<MockBookmarksPage> mock_bookmarks_page_;

  std::unique_ptr<BookmarksPageHandler> handler_;
};

TEST_F(BookmarksPageHandlerTest, SetSortOrder) {
  auto sort_order = side_panel::mojom::SortOrder::kOldest;
  handler()->SetSortOrder(sort_order);
  PrefService* pref_service = profile()->GetPrefs();
  ASSERT_EQ(
      pref_service->GetInteger(bookmarks_webui::prefs::kBookmarksSortOrder),
      static_cast<int>(sort_order));
}

TEST_F(BookmarksPageHandlerTest, SetViewType) {
  auto view_type = side_panel::mojom::ViewType::kExpanded;
  handler()->SetViewType(view_type);
  PrefService* pref_service = profile()->GetPrefs();
  ASSERT_EQ(
      pref_service->GetInteger(bookmarks_webui::prefs::kBookmarksViewType),
      static_cast<int>(view_type));
}

TEST_F(BookmarksPageHandlerTest, GetAllBookmarksOnBookmarkModelLoaded) {
  // Reset the handler so that the model is not already loaded. Load it
  // explicitly late.
  ClearHandler();
  CreateHandler();

  base::MockCallback<BookmarksPageHandler::GetAllBookmarksCallback>
      mock_callback;

  EXPECT_CALL(mock_callback, Run(testing::_)).Times(0);
  handler()->GetAllBookmarks(mock_callback.Get());

  testing::Mock::VerifyAndClearExpectations(&mock_callback);

  EXPECT_CALL(mock_callback, Run(testing::_)).Times(1);
  LoadBookmarkModel();
}

TEST_F(BookmarksPageHandlerTest,
       GetAllBookmarksContentWithEmptyPermanentNodes) {
  const bookmarks::BookmarkNode* bookmark_bar = model()->bookmark_bar_node();
  const bookmarks::BookmarkNode* other_node = model()->other_node();

  base::MockCallback<BookmarksPageHandler::GetAllBookmarksCallback>
      mock_callback;
  EXPECT_CALL(mock_callback, Run(testing::_))
      .WillOnce([&](const std::vector<side_panel::mojom::BookmarksTreeNodePtr>&
                        nodes) {
        // Total count: 2 - BookmarkBar + Other node (even if empty).
        EXPECT_THAT(nodes, SizeIs(2));
        // Bookmark is empty but still pushed.
        ASSERT_TRUE(bookmark_bar->children().empty());
        EXPECT_THAT(nodes, Contains(MatchesNode(bookmark_bar, service())));
        // Other node is empty but still pushed.
        ASSERT_TRUE(other_node->children().empty());
        EXPECT_THAT(nodes, Contains(MatchesNode(other_node, service())));
      });
  handler()->GetAllBookmarks(mock_callback.Get());
}

TEST_F(BookmarksPageHandlerTest, GetAllBookmarksContentWithAllNodes) {
  // Clears the handler to recreate it with the managed bookmarks.
  ClearHandler();
  CreateHandler(/*managed_bookmarks_count=*/10);
  LoadBookmarkModel();

  const bookmarks::BookmarkNode* bookmark_bar = model()->bookmark_bar_node();
  AddNodesFromModelString(model(), bookmark_bar, "1 2 ");
  const bookmarks::BookmarkNode* other_node = model()->other_node();
  AddNodesFromModelString(model(), other_node, "3 f1:[ 4 5 ]");

  const bookmarks::BookmarkNode* mobile_node = model()->mobile_node();
  AddNodesFromModelString(model(), mobile_node, "6 f2:[ 7 8 ]");

  const bookmarks::BookmarkNode* managed_node =
      managed_bookmark_service()->managed_node();

  base::MockCallback<BookmarksPageHandler::GetAllBookmarksCallback>
      mock_callback;
  EXPECT_CALL(mock_callback, Run(testing::_))
      .WillOnce([&](const std::vector<side_panel::mojom::BookmarksTreeNodePtr>&
                        nodes) {
        // Total count: 4 - BookmarkBar + Other node + Mobile node + Managed.
        EXPECT_THAT(nodes, SizeIs(4));
        EXPECT_THAT(nodes, Contains(MatchesNode(bookmark_bar, service())));
        EXPECT_THAT(nodes, Contains(MatchesNode(other_node, service())));
        EXPECT_THAT(nodes, Contains(MatchesNode(mobile_node, service())));
        EXPECT_THAT(nodes, Contains(MatchesNode(managed_node, service())));
      });
  handler()->GetAllBookmarks(mock_callback.Get());
}

TEST_F(BookmarksPageHandlerTest, GetAllBookmarksContentWithAccountNodes) {
  base::test::ScopedFeatureList scoped_feature_list{
      switches::kSyncEnableBookmarksInTransportMode};
  model()->CreateAccountPermanentFolders();

  const bookmarks::BookmarkNode* bookmark_bar = model()->bookmark_bar_node();
  AddNodesFromModelString(model(), bookmark_bar, "1 2 ");

  const bookmarks::BookmarkNode* account_bookmark_bar =
      model()->account_bookmark_bar_node();
  AddNodesFromModelString(model(), account_bookmark_bar, "a1 a2 ");

  const bookmarks::BookmarkNode* other_node = model()->other_node();
  AddNodesFromModelString(model(), other_node, "3 4 ");

  const bookmarks::BookmarkNode* account_other_node =
      model()->account_other_node();
  AddNodesFromModelString(model(), account_other_node, "a3 a4 ");

  const bookmarks::BookmarkNode* account_mobile_node =
      model()->account_mobile_node();
  AddNodesFromModelString(model(), account_mobile_node, "a5 ");
  const bookmarks::BookmarkNode* mobile_node = model()->mobile_node();

  base::MockCallback<BookmarksPageHandler::GetAllBookmarksCallback>
      mock_callback;
  EXPECT_CALL(mock_callback, Run(testing::_))
      .WillOnce([&](const std::vector<side_panel::mojom::BookmarksTreeNodePtr>&
                        nodes) {
        // Total count: 3 - BookmarkBar (Merged) + Other node (Merged) + Mobile.
        EXPECT_THAT(nodes, SizeIs(3));
        // Both `bookmark_bar` and `account_bookmark_bar` are merged into one.
        EXPECT_THAT(nodes, Contains(MatchesNode(bookmark_bar, service())));
        EXPECT_THAT(nodes,
                    Contains(MatchesNode(account_bookmark_bar, service())));
        // Both `other_node` and `account_other_node` are merged into one.
        EXPECT_THAT(nodes, Contains(MatchesNode(other_node, service())));
        EXPECT_THAT(nodes,
                    Contains(MatchesNode(account_other_node, service())));
        // Both `mobile_node` and `account_mobile_node` are merged into one.
        // Even if `mobile_node` is empty, it is still represented since there
        // are at least one total mobile node.
        EXPECT_THAT(nodes,
                    Contains(MatchesNode(account_mobile_node, service())));
        ASSERT_TRUE(mobile_node->children().empty());
        EXPECT_THAT(nodes, Contains(MatchesNode(mobile_node, service())));
      });
  handler()->GetAllBookmarks(mock_callback.Get());
}

TEST_F(BookmarksPageHandlerTest, OnBookmarkNodeAdded) {
  base::test::ScopedFeatureList scoped_feature_list{
      switches::kSyncEnableBookmarksInTransportMode};

  const std::string expected_bookmark_side_panel_id =
      GetFolderSidePanelIDForTesting(*service(),
                                     BookmarkParentFolder::BookmarkBarFolder());

  // Make sure that nodes added directly to the bookmark_bar folder have the
  // correct parent id computed for the side panel.
  EXPECT_CALL(mock_bookmarks_page(), OnBookmarkNodeAdded(testing::_))
      .Times(3)
      .WillRepeatedly([&](side_panel::mojom::BookmarksTreeNodePtr mojo_node) {
        EXPECT_EQ(expected_bookmark_side_panel_id, mojo_node->parent_id);
      });
  AddNodesFromModelString(model(), model()->bookmark_bar_node(), "1 2 3 ");
  mock_bookmarks_page().FlushForTesting();
  testing::Mock::VerifyAndClearExpectations(&mock_bookmarks_page());

  // When adding a folder, only the folder should have this side panel id as the
  // parent id, sub elements should just have the new folder as parent id.
  EXPECT_CALL(mock_bookmarks_page(), OnBookmarkNodeAdded(testing::_))
      .Times(3)
      .WillRepeatedly([&](side_panel::mojom::BookmarksTreeNodePtr mojo_node) {
        if (mojo_node->url.has_value()) {
          EXPECT_NE(expected_bookmark_side_panel_id, mojo_node->parent_id);
        } else {
          // Folder but should not have children as it will be sent with
          // separate notification.
          EXPECT_FALSE(mojo_node->children.has_value());
          EXPECT_EQ(expected_bookmark_side_panel_id, mojo_node->parent_id);
        }
      });
  AddNodesFromModelString(model(), model()->bookmark_bar_node(), "f1:[ 4 5 ]");
  mock_bookmarks_page().FlushForTesting();
  testing::Mock::VerifyAndClearExpectations(&mock_bookmarks_page());

  model()->CreateAccountPermanentFolders();
  // Permanent nodes do not trigger a notification.
  EXPECT_CALL(mock_bookmarks_page(), OnBookmarkNodeAdded(testing::_)).Times(0);
  mock_bookmarks_page().FlushForTesting();
  testing::Mock::VerifyAndClearExpectations(&mock_bookmarks_page());

  // Adding direct children to the account node should also have the side panel
  // bookmark bar id as their parent id.
  EXPECT_CALL(mock_bookmarks_page(), OnBookmarkNodeAdded(testing::_))
      .Times(2)
      .WillRepeatedly([&](side_panel::mojom::BookmarksTreeNodePtr mojo_node) {
        EXPECT_EQ(expected_bookmark_side_panel_id, mojo_node->parent_id);
      });
  AddNodesFromModelString(model(), model()->account_bookmark_bar_node(),
                          "a1 a2 ");
  mock_bookmarks_page().FlushForTesting();
}

}  // namespace
