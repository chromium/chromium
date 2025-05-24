// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/side_panel/bookmarks/bookmarks_page_handler.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/metrics/histogram_base.h"
#include "base/run_loop.h"
#include "base/strings/to_string.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "chrome/browser/bookmarks/bookmark_merged_surface_service.h"
#include "chrome/browser/bookmarks/bookmark_merged_surface_service_factory.h"
#include "chrome/browser/bookmarks/bookmark_test_utils.h"
#include "chrome/browser/extensions/api/bookmark_manager_private/bookmark_manager_private_api.h"
#include "chrome/browser/ui/bookmarks/bookmark_drag_drop.h"
#include "chrome/browser/ui/webui/bookmarks/bookmark_prefs.h"
#include "chrome/browser/ui/webui/side_panel/bookmarks/bookmarks.mojom.h"
#include "chrome/browser/ui/webui/side_panel/bookmarks/bookmarks_side_panel_ui.h"
#include "chrome/browser/ui/webui/webui_embedding_context.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "components/bookmarks/browser/bookmark_node_data.h"
#include "components/bookmarks/managed/managed_bookmark_service.h"
#include "components/bookmarks/test/bookmark_test_helpers.h"
#include "components/bookmarks/test/test_bookmark_client.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/browser/browser_context.h"
#include "content/public/test/test_web_ui.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/widget/any_widget_observer.h"

namespace {

using bookmarks::test::AddNodesFromModelString;
using ::testing::Contains;
using ::testing::ElementsAre;
using ::testing::Pointer;
using ::testing::SizeIs;
using ::testing::UnorderedElementsAre;

// Compares a `side_panel::mojom::BookmarksTreeNodePtr` (arg) with a
// `bookmarks::BookmarkNode` (node) by comparing their `id` and the ids of their
// children recursively.
// Note: the matching node `arg` may contain more children than the expected
// `node` since it could be the representation of a merged node. Therefore we
// only make sure that `node`'s children are a subset of `arg`'s children if
// `arg` actually contains any children.
MATCHER_P(TreeNodeMatchesNode, node, "") {
  const std::string expected_id =
      node->is_folder() ? GetFolderSidePanelIDForTesting(
                              BookmarkParentFolder::FromFolderNode(node))
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
                Contains(TreeNodeMatchesNode(child.get())))
        << "Some child nodes of '" << node->GetTitle()
        << "' are not contained in the matching node with the same id.";
  }

  return true;
}

MATCHER_P(MatchesNode, node, "") {
  return arg->type() == node->type() && arg->GetTitle() == node->GetTitle() &&
         arg->url() == node->url();
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
  MOCK_METHOD(void, OnBookmarkNodesRemoved, (const std::vector<std::string>&));
  MOCK_METHOD(void,
              OnBookmarkParentFolderChildrenReordered,
              (const std::string&, const std::vector<std::string>&));
  MOCK_METHOD(void,
              OnBookmarkNodeMoved,
              (const std::string&, uint32_t, const std::string&, uint32_t));
  MOCK_METHOD(void,
              OnBookmarkNodeChanged,
              (const std::string&, const std::string&, const std::string&));

 private:
  mojo::Receiver<side_panel::mojom::BookmarksPage> receiver_{this};
};

// Helper class to handle the return value of
// `BookmarksPageHandler::CreateFolder()` through a callback. It resets
// internally after retrieving the results so that it can be used multiple times
// within the same instance.
class CreateFolderFunctionHelper {
 public:
  std::string GetResultWhenReady() {
    if (!result_) {
      run_loop_.Run();
    }

    return ClearAndReturnResult();
  }

  void OnCreateFolderResult(const std::string& result) {
    result_ = result;

    if (run_loop_.running()) {
      run_loop_.Quit();
    }
  }

 private:
  std::string ClearAndReturnResult() {
    CHECK(result_.has_value());
    std::string ret_value = result_.value();
    result_.reset();
    return ret_value;
  }

  std::optional<std::string> result_;
  base::RunLoop run_loop_;
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

  TestingProfile::TestingFactories GetTestingFactories() override {
    return {TestingProfile::TestingFactory{
        BookmarkModelFactory::GetInstance(),
        BookmarkModelFactory::GetDefaultFactory()}};
  }

  BookmarksPageHandler* handler() { return handler_.get(); }
  bookmarks::BookmarkModel* model() { return bookmark_model_.get(); }
  bookmarks::ManagedBookmarkService* managed_bookmark_service() {
    return managed_bookmark_service_.get();
  }
  BookmarkMergedSurfaceService* service() { return bookmark_merged_service_; }
  testing::NiceMock<MockBookmarksPage>& mock_bookmarks_page() {
    return mock_bookmarks_page_;
  }
  content::WebContents* side_panel_web_contents() {
    return fake_side_panel_web_contents_.get();
  }

  void CreateHandler(int managed_bookmarks_count = 0) {
    CHECK(!handler_);

    prefs_ = std::make_unique<sync_preferences::TestingPrefServiceSyncable>();

    managed_bookmark_service_ =
        CreateManagedBookmarkService(prefs_.get(), managed_bookmarks_count);
    bookmark_model_ = std::make_unique<bookmarks::BookmarkModel>(
        std::make_unique<TestBookmarkClientWithManagedService>(
            managed_bookmark_service_.get()));

    auto bookmark_merged_service =
        std::make_unique<BookmarkMergedSurfaceService>(
            bookmark_model_.get(), managed_bookmark_service_.get());
    bookmark_merged_service_ = bookmark_merged_service.get();

    BookmarkMergedSurfaceServiceFactory::GetInstance()->SetTestingFactory(
        profile(), base::BindRepeating(
                       [](BookmarkMergedSurfaceService* service,
                          content::BrowserContext* context)
                           -> std::unique_ptr<KeyedService> {
                         return base::WrapUnique(service);
                       },
                       bookmark_merged_service.release()));

    // This is not the actual side panel web contents, but random contents
    // created in order to attach them to the `bookmarks_ui_`.
    fake_side_panel_web_contents_ =
        content::WebContentsTester::CreateTestWebContents(
            profile(), content::SiteInstance::Create(profile()));
    web_ui_.set_web_contents(fake_side_panel_web_contents_.get());
    webui::SetBrowserWindowInterface(fake_side_panel_web_contents_.get(),
                                     browser());

    bookmarks_ui_ = std::make_unique<BookmarksSidePanelUI>(&web_ui_);

    extensions::BookmarkManagerPrivateDragEventRouter::CreateForWebContents(
        fake_side_panel_web_contents_.get());

    handler_ = std::make_unique<BookmarksPageHandler>(
        mojo::PendingReceiver<side_panel::mojom::BookmarksPageHandler>(),
        mock_bookmarks_page_.BindAndGetRemote(),
        /*bookmarks_ui=*/bookmarks_ui_.get(), &web_ui_);
  }

  void LoadBookmarkModel() {
    bookmark_model_->LoadEmptyForTest();
    bookmark_merged_service_->LoadForTesting({});
  }

  void ClearHandler() {
    handler_.reset();
    mock_bookmarks_page_.Reset();
    bookmarks_ui_.reset();
    web_ui_.set_web_contents(nullptr);
    fake_side_panel_web_contents_.reset();
    bookmark_merged_service_ = nullptr;
    BookmarkMergedSurfaceServiceFactory::GetInstance()->SetTestingFactory(
        profile(), {});
    bookmark_model_.reset();
    managed_bookmark_service_.reset();
    prefs_.reset();
  }

  int64_t GetBookmarkIdFromString(const std::string& string_id) {
    int64_t bookmark_id;
    CHECK(base::StringToInt64(string_id, &bookmark_id));
    return bookmark_id;
  }

 private:
  std::unique_ptr<sync_preferences::TestingPrefServiceSyncable> prefs_;

  std::unique_ptr<bookmarks::ManagedBookmarkService> managed_bookmark_service_;
  std::unique_ptr<bookmarks::BookmarkModel> bookmark_model_;
  raw_ptr<BookmarkMergedSurfaceService> bookmark_merged_service_;
  std::unique_ptr<BookmarksSidePanelUI> bookmarks_ui_;
  std::unique_ptr<content::WebContents> fake_side_panel_web_contents_;
  content::TestWebUI web_ui_;

  testing::NiceMock<MockBookmarksPage> mock_bookmarks_page_;

  std::unique_ptr<BookmarksPageHandler> handler_;

  base::test::ScopedFeatureList scoped_feature_list_{
      switches::kSyncEnableBookmarksInTransportMode};
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
        EXPECT_THAT(nodes, Contains(TreeNodeMatchesNode(bookmark_bar)));
        // Other node is empty but still pushed.
        ASSERT_TRUE(other_node->children().empty());
        EXPECT_THAT(nodes, Contains(TreeNodeMatchesNode(other_node)));
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
        EXPECT_THAT(nodes, Contains(TreeNodeMatchesNode(bookmark_bar)));
        EXPECT_THAT(nodes, Contains(TreeNodeMatchesNode(other_node)));
        EXPECT_THAT(nodes, Contains(TreeNodeMatchesNode(mobile_node)));
        EXPECT_THAT(nodes, Contains(TreeNodeMatchesNode(managed_node)));
      });
  handler()->GetAllBookmarks(mock_callback.Get());
}

TEST_F(BookmarksPageHandlerTest, GetAllBookmarksContentWithAccountNodes) {
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
        EXPECT_THAT(nodes, Contains(TreeNodeMatchesNode(bookmark_bar)));
        EXPECT_THAT(nodes, Contains(TreeNodeMatchesNode(account_bookmark_bar)));
        // Both `other_node` and `account_other_node` are merged into one.
        EXPECT_THAT(nodes, Contains(TreeNodeMatchesNode(other_node)));
        EXPECT_THAT(nodes, Contains(TreeNodeMatchesNode(account_other_node)));
        // Both `mobile_node` and `account_mobile_node` are merged into one.
        // Even if `mobile_node` is empty, it is still represented since there
        // are at least one total mobile node.
        EXPECT_THAT(nodes, Contains(TreeNodeMatchesNode(account_mobile_node)));
        ASSERT_TRUE(mobile_node->children().empty());
        EXPECT_THAT(nodes, Contains(TreeNodeMatchesNode(mobile_node)));
      });
  handler()->GetAllBookmarks(mock_callback.Get());
}

TEST_F(BookmarksPageHandlerTest, OnBookmarkNodeAdded) {
  // Make sure that nodes added directly to the bookmark_bar folder have the
  // correct parent id computed for the side panel.
  EXPECT_CALL(mock_bookmarks_page(), OnBookmarkNodeAdded(testing::_))
      .Times(3)
      .WillRepeatedly([&](side_panel::mojom::BookmarksTreeNodePtr mojo_node) {
        EXPECT_EQ(kSidePanelBookmarkBarID, mojo_node->parent_id);
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
          EXPECT_NE(kSidePanelBookmarkBarID, mojo_node->parent_id);
        } else {
          // Folder but should not have children as it will be sent with
          // separate notification.
          EXPECT_FALSE(mojo_node->children.has_value());
          EXPECT_EQ(kSidePanelBookmarkBarID, mojo_node->parent_id);
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
        EXPECT_EQ(kSidePanelBookmarkBarID, mojo_node->parent_id);
      });
  AddNodesFromModelString(model(), model()->account_bookmark_bar_node(),
                          "a1 a2 ");
  mock_bookmarks_page().FlushForTesting();
}

TEST_F(BookmarksPageHandlerTest, OnBookmarkNodesRemoved) {
  model()->CreateAccountPermanentFolders();

  const bookmarks::BookmarkNode* other_node = model()->other_node();
  const bookmarks::BookmarkNode* account_other_node =
      model()->account_other_node();

  AddNodesFromModelString(model(), other_node, "1 2:[ 3 ]");
  AddNodesFromModelString(model(), account_other_node, "4 5 6 ");

  const bookmarks::BookmarkNode* folder_in_other =
      other_node->children()[1].get();
  ASSERT_TRUE(folder_in_other->is_folder());

  // Only folder id expected; without its children ids.
  EXPECT_CALL(mock_bookmarks_page(),
              OnBookmarkNodesRemoved(
                  UnorderedElementsAre(base::ToString(folder_in_other->id()))));
  model()->Remove(folder_in_other,
                  bookmarks::metrics::BookmarkEditSource::kOther, FROM_HERE);
  mock_bookmarks_page().FlushForTesting();
  testing::Mock::VerifyAndClearExpectations(&mock_bookmarks_page());

  const bookmarks::BookmarkNode* first_account_node =
      account_other_node->children()[1].get();
  ASSERT_TRUE(first_account_node->is_url());

  EXPECT_CALL(mock_bookmarks_page(),
              OnBookmarkNodesRemoved(UnorderedElementsAre(
                  base::ToString(first_account_node->id()))));
  model()->Remove(first_account_node,
                  bookmarks::metrics::BookmarkEditSource::kOther, FROM_HERE);
  mock_bookmarks_page().FlushForTesting();
  testing::Mock::VerifyAndClearExpectations(&mock_bookmarks_page());

  // 2 remaining nodes in `account_other_node`.
  EXPECT_CALL(
      mock_bookmarks_page(),
      OnBookmarkNodesRemoved(UnorderedElementsAre(
          base::ToString(account_other_node->children()[0].get()->id()),
          base::ToString(account_other_node->children()[1].get()->id()))));
  model()->RemoveAccountPermanentFolders();
  mock_bookmarks_page().FlushForTesting();
}

TEST_F(BookmarksPageHandlerTest,
       OnBookmarkParentFolderChildrenReorderedFromLocalNode) {
  model()->CreateAccountPermanentFolders();

  const bookmarks::BookmarkNode* other_node = model()->other_node();
  const bookmarks::BookmarkNode* account_other_node =
      model()->account_other_node();

  AddNodesFromModelString(model(), other_node, "2 1:[ 3 ]");
  std::string first_other_node_id =
      base::ToString(other_node->children()[0].get()->id());
  std::string second_other_node_id =
      base::ToString(other_node->children()[1].get()->id());
  AddNodesFromModelString(model(), account_other_node, "5 4 ");
  std::string first_account_other_node_id =
      base::ToString(account_other_node->children()[0].get()->id());
  std::string second_account_other_node_id =
      base::ToString(account_other_node->children()[1].get()->id());

  // All merged nodes are part of the notification, but only local nodes are
  // reorderd.
  EXPECT_CALL(
      mock_bookmarks_page(),
      OnBookmarkParentFolderChildrenReordered(
          kSidePanelOtherBookmarksID,
          ElementsAre(first_account_other_node_id, second_account_other_node_id,
                      second_other_node_id, first_other_node_id)));
  // Sort local nodes.
  model()->SortChildren(other_node);
  mock_bookmarks_page().FlushForTesting();
}

TEST_F(BookmarksPageHandlerTest,
       OnBookmarkParentFolderChildrenReorderedFromAccountNode) {
  model()->CreateAccountPermanentFolders();

  const bookmarks::BookmarkNode* other_node = model()->other_node();
  const bookmarks::BookmarkNode* account_other_node =
      model()->account_other_node();

  AddNodesFromModelString(model(), other_node, "2 1:[ 3 ]");
  std::string first_other_node_id =
      base::ToString(other_node->children()[0].get()->id());
  std::string second_other_node_id =
      base::ToString(other_node->children()[1].get()->id());
  AddNodesFromModelString(model(), account_other_node, "5 4 ");
  std::string first_account_other_node_id =
      base::ToString(account_other_node->children()[0].get()->id());
  std::string second_account_other_node_id =
      base::ToString(account_other_node->children()[1].get()->id());

  // All merged nodes are part of the notification, but only account nodes are
  // reorderd.
  EXPECT_CALL(
      mock_bookmarks_page(),
      OnBookmarkParentFolderChildrenReordered(
          kSidePanelOtherBookmarksID,
          ElementsAre(second_account_other_node_id, first_account_other_node_id,
                      first_other_node_id, second_other_node_id)));
  // Sort account nodes.
  model()->SortChildren(account_other_node);
  mock_bookmarks_page().FlushForTesting();
}

TEST_F(BookmarksPageHandlerTest, OnBookmarkNodeMoved) {
  base::test::ScopedFeatureList scoped_feature_list{
      switches::kSyncEnableBookmarksInTransportMode};

  model()->CreateAccountPermanentFolders();

  const bookmarks::BookmarkNode* other_node = model()->other_node();
  const bookmarks::BookmarkNode* account_bookmark_bar_node =
      model()->account_bookmark_bar_node();

  AddNodesFromModelString(model(), other_node, "2 1:[ 3 ]");
  const bookmarks::BookmarkNode* node_to_move = other_node->children()[0].get();
  AddNodesFromModelString(model(), account_bookmark_bar_node, "5 4 ");

  EXPECT_CALL(mock_bookmarks_page(),
              OnBookmarkNodeMoved(kSidePanelOtherBookmarksID, testing::_,
                                  kSidePanelBookmarkBarID, testing::_));

  model()->Move(node_to_move, account_bookmark_bar_node, 0);
  mock_bookmarks_page().FlushForTesting();
}

TEST_F(BookmarksPageHandlerTest, BookmarkCurrentTabInFolder) {
  // Adds tab content that will be used to be bookmarked.
  AddTab(browser(), GURL("http://www.google.com/"));

  const bookmarks::BookmarkNode* other_node = model()->other_node();
  ASSERT_TRUE(other_node->children().empty());

  // Using side panel id, merged node between local and account nodes.
  handler()->BookmarkCurrentTabInFolder(kSidePanelOtherBookmarksID);
  // Without account nodes, the local node is the default one for merged nodes.
  EXPECT_EQ(1u, other_node->children().size());

  // Initiates the account nodes.
  model()->CreateAccountPermanentFolders();
  const bookmarks::BookmarkNode* account_other_node =
      model()->account_other_node();
  ASSERT_TRUE(account_other_node->children().empty());

  // Account node is now the default one.
  handler()->BookmarkCurrentTabInFolder(kSidePanelOtherBookmarksID);
  EXPECT_EQ(1u, account_other_node->children().size());
  EXPECT_EQ(1u, other_node->children().size());

  // Creating a non merged folder.
  const bookmarks::BookmarkNode* new_folder =
      model()->AddFolder(other_node, 0, u"New Local Folder");
  ASSERT_TRUE(new_folder->children().empty());
  ASSERT_EQ(2u, other_node->children().size());
  // A non meregd folder is directly given as input and impacted.
  handler()->BookmarkCurrentTabInFolder(base::ToString(new_folder->id()));
  EXPECT_EQ(1u, new_folder->children().size());
  EXPECT_EQ(1u, account_other_node->children().size());
  EXPECT_EQ(2u, other_node->children().size());
}

TEST_F(BookmarksPageHandlerTest, CreateFolder) {
  CreateFolderFunctionHelper helper;
  const std::string folder_title = "new_folder_title";

  const bookmarks::BookmarkNode* other_node = model()->other_node();
  ASSERT_TRUE(other_node->children().empty());

  // Using side panel id, merged node between local and account nodes.
  handler()->CreateFolder(
      kSidePanelOtherBookmarksID, folder_title,
      base::BindOnce(&CreateFolderFunctionHelper::OnCreateFolderResult,
                     base::Unretained(&helper)));
  std::string added_local_folder_id = helper.GetResultWhenReady();
  // Without account nodes, the local node is the default one for merged nodes.
  ASSERT_EQ(1u, other_node->children().size());
  EXPECT_EQ(GetBookmarkIdFromString(added_local_folder_id),
            other_node->children()[0].get()->id());

  // Initiates the account nodes.
  model()->CreateAccountPermanentFolders();
  const bookmarks::BookmarkNode* account_other_node =
      model()->account_other_node();
  ASSERT_TRUE(account_other_node->children().empty());
  // Account node is now the default one.
  handler()->CreateFolder(
      kSidePanelOtherBookmarksID, folder_title,
      base::BindOnce(&CreateFolderFunctionHelper::OnCreateFolderResult,
                     base::Unretained(&helper)));
  std::string added_account_folder_id = helper.GetResultWhenReady();
  ASSERT_EQ(1u, account_other_node->children().size());
  EXPECT_EQ(GetBookmarkIdFromString(added_account_folder_id),
            account_other_node->children()[0].get()->id());
  EXPECT_EQ(1u, other_node->children().size());

  // Creating a non merged folder.
  const bookmarks::BookmarkNode* new_folder =
      model()->AddFolder(other_node, 0, u"New Local Folder");
  ASSERT_TRUE(new_folder->children().empty());
  ASSERT_EQ(2u, other_node->children().size());
  // A non meregd folder is directly given as input and impacted.
  handler()->CreateFolder(
      base::ToString(new_folder->id()), folder_title,
      base::BindOnce(&CreateFolderFunctionHelper::OnCreateFolderResult,
                     base::Unretained(&helper)));
  std::string added_regular_folder_id = helper.GetResultWhenReady();
  ASSERT_EQ(1u, new_folder->children().size());
  EXPECT_EQ(GetBookmarkIdFromString(added_regular_folder_id),
            new_folder->children()[0].get()->id());
  EXPECT_EQ(1u, account_other_node->children().size());
  EXPECT_EQ(2u, other_node->children().size());
}

// Should test any handler function that accepts a string as node ID.
// Making sure that the test does not crash and has no effect.
TEST_F(BookmarksPageHandlerTest, HandlerWithInvalidInputs) {
  const std::string invalid_id_string = "random_invalid_id";
  model()->CreateAccountPermanentFolders();

  ASSERT_FALSE(model()->HasBookmarks());

  handler()->BookmarkCurrentTabInFolder(invalid_id_string);

  EXPECT_FALSE(model()->HasBookmarks());

  CreateFolderFunctionHelper helper;
  handler()->CreateFolder(
      invalid_id_string, "folder_title",
      base::BindOnce(&CreateFolderFunctionHelper::OnCreateFolderResult,
                     base::Unretained(&helper)));
  std::string result = helper.GetResultWhenReady();
  EXPECT_TRUE(result.empty());

  EXPECT_FALSE(model()->HasBookmarks());
}

TEST_F(BookmarksPageHandlerTest, DropBookmarks) {
  const bookmarks::BookmarkNode* node1 =
      model()->AddURL(model()->bookmark_bar_node(), 0, u"title",
                      GURL("http://www.google.com/"));
  const bookmarks::BookmarkNode* node2 =
      model()->AddURL(model()->bookmark_bar_node(), 1, u"title",
                      GURL("http://www.google.com/"));

  // Create and prepare the bookmark node data to be dropped.
  bookmarks::BookmarkNodeData data({node1, node2});
  data.SetOriginatingProfilePath(browser()->profile()->GetPath());
  extensions::BookmarkManagerPrivateDragEventRouter::FromWebContents(
      side_panel_web_contents())
      ->OnDrop(data);


  // Drop the data. This should move it.
  base::MockCallback<BookmarksPageHandler::DropBookmarksCallback> mock_callback;
  EXPECT_CALL(mock_callback, Run());
  // Using side panel id, merged node between local and account nodes.
  handler()->DropBookmarks(kSidePanelOtherBookmarksID, mock_callback.Get());

  EXPECT_TRUE(model()->bookmark_bar_node()->children().empty());
  EXPECT_THAT(model()->other_node()->children(),
              ElementsAre(MatchesNode(node1), MatchesNode(node2)));
}

TEST_F(BookmarksPageHandlerTest, DropManagedBookmark) {
  const bookmarks::BookmarkNode* node1 =
      model()->AddURL(managed_bookmark_service()->managed_node(), 0, u"title",
                      GURL("http://www.google.com/"));
  const bookmarks::BookmarkNode* node2 =
      model()->AddURL(model()->bookmark_bar_node(), 0, u"title",
                      GURL("http://www.google.com/"));

  // Create and prepare the bookmark node data to be dropped.
  bookmarks::BookmarkNodeData data({node1, node2});
  data.SetOriginatingProfilePath(browser()->profile()->GetPath());
  extensions::BookmarkManagerPrivateDragEventRouter::FromWebContents(
      side_panel_web_contents())
      ->OnDrop(data);

  // Drop the data. This should not move either of the bookmarks.
  // TODO(crbug.com/409284055): The data should be copied instead.
  base::MockCallback<BookmarksPageHandler::DropBookmarksCallback> mock_callback;
  EXPECT_CALL(mock_callback, Run());
  // Using side panel id, merged node between local and account nodes.
  handler()->DropBookmarks(kSidePanelOtherBookmarksID, mock_callback.Get());

  EXPECT_THAT(managed_bookmark_service()->managed_node()->children(),
              ElementsAre(MatchesNode(node1)));
  EXPECT_THAT(model()->bookmark_bar_node()->children(),
              ElementsAre(MatchesNode(node2)));
  EXPECT_TRUE(model()->other_node()->children().empty());
}

TEST_F(BookmarksPageHandlerTest, DropBookmarksInDifferentProfile) {
  base::HistogramTester histogram_tester;

  // Create and prepare another profile.
  TestingProfile::Builder profile_builder;
  profile_builder.AddTestingFactories(GetTestingFactories());
  std::unique_ptr<TestingProfile> different_profile = profile_builder.Build();
  bookmarks::BookmarkModel* different_bookmark_model =
      BookmarkModelFactory::GetForBrowserContext(different_profile.get());
  bookmarks::test::WaitForBookmarkModelToLoad(different_bookmark_model);

  const bookmarks::BookmarkNode* node1 = different_bookmark_model->AddURL(
      different_bookmark_model->bookmark_bar_node(), 0, u"title1",
      GURL("http://www.google1.com/"));
  const bookmarks::BookmarkNode* node2 = different_bookmark_model->AddURL(
      different_bookmark_model->bookmark_bar_node(), 1, u"title2",
      GURL("http://www.google2.com/"));

  // Create and prepare the bookmark node data to be dropped.
  bookmarks::BookmarkNodeData data({node1, node2});
  data.SetOriginatingProfilePath(different_profile->GetPath());
  extensions::BookmarkManagerPrivateDragEventRouter::FromWebContents(
      side_panel_web_contents())
      ->OnDrop(data);


  // Drop the data in the other browser. This should copy it.
  base::MockCallback<BookmarksPageHandler::DropBookmarksCallback> mock_callback;
  EXPECT_CALL(mock_callback, Run());
  // Using side panel id, merged node between local and account nodes.
  handler()->DropBookmarks(kSidePanelOtherBookmarksID, mock_callback.Get());

  EXPECT_THAT(different_bookmark_model->bookmark_bar_node()->children(),
              ElementsAre(MatchesNode(node1), MatchesNode(node2)));
  EXPECT_THAT(model()->other_node()->children(),
              ElementsAre(MatchesNode(node1), MatchesNode(node2)));

  histogram_tester.ExpectTotalCount("Bookmarks.AddedPerProfileType", 1);
}

TEST_F(BookmarksPageHandlerTest, DropBookmarksWithAccountNodes) {
  model()->CreateAccountPermanentFolders();

  const bookmarks::BookmarkNode* node1 =
      model()->AddURL(model()->account_bookmark_bar_node(), 0, u"title",
                      GURL("http://www.google.com/"));
  const bookmarks::BookmarkNode* node2 =
      model()->AddURL(model()->account_bookmark_bar_node(), 1, u"title",
                      GURL("http://www.google.com/"));

  // Create and prepare the bookmark node data to be dropped.
  bookmarks::BookmarkNodeData data({node1, node2});
  data.SetOriginatingProfilePath(browser()->profile()->GetPath());
  extensions::BookmarkManagerPrivateDragEventRouter::FromWebContents(
      side_panel_web_contents())
      ->OnDrop(data);

  // Drop the data. This should move it.
  base::MockCallback<BookmarksPageHandler::DropBookmarksCallback> mock_callback;
  EXPECT_CALL(mock_callback, Run());
  // Using side panel id, merged node between local and account nodes.
  handler()->DropBookmarks(kSidePanelOtherBookmarksID, mock_callback.Get());

  EXPECT_TRUE(model()->account_bookmark_bar_node()->children().empty());
  EXPECT_THAT(model()->account_other_node()->children(),
              ElementsAre(MatchesNode(node1), MatchesNode(node2)));
}

TEST_F(BookmarksPageHandlerTest,
       DropBookmarksInDifferentProfileWithAccountNodes) {
  base::HistogramTester histogram_tester;
  model()->CreateAccountPermanentFolders();

  // Create and prepare another profile.
  TestingProfile::Builder profile_builder;
  profile_builder.AddTestingFactories(GetTestingFactories());
  std::unique_ptr<TestingProfile> different_profile = profile_builder.Build();
  bookmarks::BookmarkModel* different_bookmark_model =
      BookmarkModelFactory::GetForBrowserContext(different_profile.get());
  bookmarks::test::WaitForBookmarkModelToLoad(different_bookmark_model);

  const bookmarks::BookmarkNode* node1 = different_bookmark_model->AddURL(
      different_bookmark_model->bookmark_bar_node(), 0, u"title1",
      GURL("http://www.google1.com/"));
  const bookmarks::BookmarkNode* node2 = different_bookmark_model->AddURL(
      different_bookmark_model->bookmark_bar_node(), 1, u"title2",
      GURL("http://www.google2.com/"));

  // Create and prepare the bookmark node data to be dropped.
  bookmarks::BookmarkNodeData data({node1, node2});
  data.SetOriginatingProfilePath(different_profile->GetPath());
  extensions::BookmarkManagerPrivateDragEventRouter::FromWebContents(
      side_panel_web_contents())
      ->OnDrop(data);

  // Drop the data in the other browser. This should copy it.
  base::MockCallback<BookmarksPageHandler::DropBookmarksCallback> mock_callback;
  EXPECT_CALL(mock_callback, Run());
  // Using side panel id, merged node between local and account nodes.
  handler()->DropBookmarks(kSidePanelOtherBookmarksID, mock_callback.Get());

  // When available, the bookmark is saved to the account storage.
  EXPECT_THAT(different_bookmark_model->bookmark_bar_node()->children(),
              ElementsAre(MatchesNode(node1), MatchesNode(node2)));
  EXPECT_THAT(model()->account_other_node()->children(),
              ElementsAre(MatchesNode(node1), MatchesNode(node2)));

  histogram_tester.ExpectTotalCount("Bookmarks.AddedPerProfileType", 1);
}

}  // namespace
