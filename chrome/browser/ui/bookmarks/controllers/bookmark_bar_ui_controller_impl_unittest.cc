// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/bookmarks/controllers/bookmark_bar_ui_controller_impl.h"

#include <map>
#include <memory>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/mock_callback.h"
#include "chrome/browser/ui/bookmarks/controllers/adapters/bookmark_bar_action_adapter.h"
#include "chrome/browser/ui/bookmarks/controllers/adapters/bookmark_bar_model_adapter.h"
#include "chrome/browser/ui/bookmarks/controllers/adapters/bookmark_bar_prefs_adapter.h"
#include "chrome/browser/ui/bookmarks/controllers/bookmark_bar_ui_client.h"
#include "chrome/browser/ui/bookmarks/controllers/bookmark_bar_ui_controller_injector.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/common/bookmark_pref_names.h"
#include "components/bookmarks/test/test_bookmark_client.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/mojom/menu_source_type.mojom.h"
#include "ui/base/window_open_disposition.h"
#include "ui/gfx/geometry/point.h"

namespace {

class MockBookmarkBarUIClient : public BookmarkBarUIClient {
 public:
  MOCK_METHOD(void, SetAppsPageShortcutVisibility, (bool), (override));
  MOCK_METHOD(void, SetSavedTabGroupsVisibility, (bool), (override));
  MOCK_METHOD(void, SetManagedBookmarksFolderVisibility, (bool), (override));
  MOCK_METHOD(void,
              ShowFolderMenu,
              (const bookmarks::BookmarkNodeId&),
              (override));
};

class MockBookmarkBarPrefsAdapter : public BookmarkBarPrefsAdapter {
 public:
  MOCK_METHOD(bool, GetBoolean, (const std::string&), (const, override));

  void AddObserver(const std::string& pref_name,
                   PrefChangedCallback callback) override {
    observers_[pref_name] = callback;
  }

  void TriggerPrefChanged(const std::string& pref_name) {
    auto it = observers_.find(pref_name);
    if (it != observers_.end()) {
      it->second.Run();
    }
  }

 private:
  std::map<std::string, PrefChangedCallback> observers_;
};

class MockBookmarkBarActionAdapter : public BookmarkBarActionAdapter {
 public:
  MOCK_METHOD(void, OpenAppsPage, (WindowOpenDisposition), (override));
  MOCK_METHOD(void,
              OpenBookmark,
              (const bookmarks::BookmarkNode*, WindowOpenDisposition),
              (override));
  MOCK_METHOD(void, NotifyFolderOpened, (), (override));
  MOCK_METHOD(void,
              OpenFolderNodes,
              ((const std::vector<const bookmarks::BookmarkNode*>&),
               WindowOpenDisposition),
              (override));
  MOCK_METHOD(void,
              ShowContextMenu,
              ((const std::vector<const bookmarks::BookmarkNode*>&),
               const gfx::Point&,
               ui::mojom::MenuSourceType,
               bool,
               base::OnceClosure),
              (override));
};

class MockBookmarkBarModelAdapter : public BookmarkBarModelAdapter {
 public:
  MOCK_METHOD(bool, IsLoaded, (), (const, override));
  MOCK_METHOD(const bookmarks::BookmarkNode*,
              GetNodeById,
              (int64_t),
              (const, override));
  MOCK_METHOD((std::vector<const bookmarks::BookmarkNode*>),
              GetUnderlyingNodes,
              (const bookmarks::BookmarkNodeId&),
              (const, override));
  MOCK_METHOD(void,
              CanPasteFromClipboard,
              (const BookmarkParentFolder*, base::OnceCallback<void(bool)>),
              (override));
};

class MockBookmarkBarUIControllerInjector
    : public BookmarkBarUIControllerInjector {
 public:
  MOCK_METHOD(BookmarkBarPrefsAdapter*, GetPrefsAdapter, (), (override));
  MOCK_METHOD(BookmarkBarActionAdapter*, GetActionAdapter, (), (override));
  MOCK_METHOD(BookmarkBarModelAdapter*, GetModelAdapter, (), (override));
};

class BookmarkBarUIControllerImplTest : public testing::Test {
 protected:
  void SetUp() override {
    auto injector = std::make_unique<
        testing::NiceMock<MockBookmarkBarUIControllerInjector>>();
    mock_injector_ = injector.get();

    ON_CALL(*mock_injector_, GetPrefsAdapter())
        .WillByDefault(testing::Return(&mock_prefs_adapter_));
    ON_CALL(*mock_injector_, GetActionAdapter())
        .WillByDefault(testing::Return(&mock_action_adapter_));
    ON_CALL(*mock_injector_, GetModelAdapter())
        .WillByDefault(testing::Return(&mock_model_adapter_));

    controller_ =
        std::make_unique<BookmarkBarUIControllerImpl>(std::move(injector));
  }

  void TearDown() override {
    mock_injector_ = nullptr;
    controller_.reset();
  }

  testing::NiceMock<MockBookmarkBarPrefsAdapter> mock_prefs_adapter_;
  testing::NiceMock<MockBookmarkBarActionAdapter> mock_action_adapter_;
  testing::NiceMock<MockBookmarkBarModelAdapter> mock_model_adapter_;
  raw_ptr<testing::NiceMock<MockBookmarkBarUIControllerInjector>>
      mock_injector_;
  std::unique_ptr<BookmarkBarUIControllerImpl> controller_;
  testing::NiceMock<MockBookmarkBarUIClient> mock_client_;
};

TEST_F(BookmarkBarUIControllerImplTest, BindPushesInitialState) {
  // Set initial pref values in mock.
  EXPECT_CALL(mock_prefs_adapter_,
              GetBoolean(bookmarks::prefs::kShowAppsShortcutInBookmarkBar))
      .WillOnce(testing::Return(true));
  EXPECT_CALL(mock_prefs_adapter_,
              GetBoolean(bookmarks::prefs::kShowTabGroupsInBookmarkBar))
      .WillOnce(testing::Return(false));
  EXPECT_CALL(mock_prefs_adapter_,
              GetBoolean(bookmarks::prefs::kShowManagedBookmarksInBookmarkBar))
      .WillOnce(testing::Return(true));

  // Expect that binding immediately triggers calls to client.
  EXPECT_CALL(mock_client_, SetAppsPageShortcutVisibility(true));
  EXPECT_CALL(mock_client_, SetSavedTabGroupsVisibility(false));
  EXPECT_CALL(mock_client_, SetManagedBookmarksFolderVisibility(true));

  controller_->Bind(&mock_client_);
}

TEST_F(BookmarkBarUIControllerImplTest, PrefChangesPropagate) {
  // Setup default responses for Bind.
  ON_CALL(mock_prefs_adapter_, GetBoolean(testing::_))
      .WillByDefault(testing::Return(false));

  controller_->Bind(&mock_client_);

  // Apps Shortcut
  EXPECT_CALL(mock_prefs_adapter_,
              GetBoolean(bookmarks::prefs::kShowAppsShortcutInBookmarkBar))
      .WillOnce(testing::Return(true));
  EXPECT_CALL(mock_client_, SetAppsPageShortcutVisibility(true));
  mock_prefs_adapter_.TriggerPrefChanged(
      bookmarks::prefs::kShowAppsShortcutInBookmarkBar);
  testing::Mock::VerifyAndClearExpectations(&mock_client_);
  testing::Mock::VerifyAndClearExpectations(&mock_prefs_adapter_);

  // Saved Tab Groups
  EXPECT_CALL(mock_prefs_adapter_,
              GetBoolean(bookmarks::prefs::kShowTabGroupsInBookmarkBar))
      .WillOnce(testing::Return(true));
  EXPECT_CALL(mock_client_, SetSavedTabGroupsVisibility(true));
  mock_prefs_adapter_.TriggerPrefChanged(
      bookmarks::prefs::kShowTabGroupsInBookmarkBar);
  testing::Mock::VerifyAndClearExpectations(&mock_client_);
  testing::Mock::VerifyAndClearExpectations(&mock_prefs_adapter_);

  // Managed Bookmarks
  EXPECT_CALL(mock_prefs_adapter_,
              GetBoolean(bookmarks::prefs::kShowManagedBookmarksInBookmarkBar))
      .WillOnce(testing::Return(true));
  EXPECT_CALL(mock_client_, SetManagedBookmarksFolderVisibility(true));
  mock_prefs_adapter_.TriggerPrefChanged(
      bookmarks::prefs::kShowManagedBookmarksInBookmarkBar);
  testing::Mock::VerifyAndClearExpectations(&mock_client_);
  testing::Mock::VerifyAndClearExpectations(&mock_prefs_adapter_);
}

TEST_F(BookmarkBarUIControllerImplTest, OpenAppsPageDelegates) {
  EXPECT_CALL(mock_action_adapter_,
              OpenAppsPage(WindowOpenDisposition::NEW_WINDOW));
  controller_->OpenAppsPage(WindowOpenDisposition::NEW_WINDOW);
}

TEST_F(BookmarkBarUIControllerImplTest, OpenBookmarkDelegates) {
  auto bookmark_model = bookmarks::TestBookmarkClient::CreateModel();
  const bookmarks::BookmarkNode* node =
      bookmark_model->AddURL(bookmark_model->bookmark_bar_node(), 0, u"Title",
                             GURL("https://google.com"));

  EXPECT_CALL(mock_model_adapter_, GetNodeById(42))
      .WillOnce(testing::Return(node));
  EXPECT_CALL(mock_action_adapter_,
              OpenBookmark(node, WindowOpenDisposition::NEW_WINDOW));
  controller_->OpenBookmark(42, WindowOpenDisposition::NEW_WINDOW);
}

TEST_F(BookmarkBarUIControllerImplTest, OpenBookmarkNodeNotFound) {
  EXPECT_CALL(mock_model_adapter_, GetNodeById(42))
      .WillOnce(testing::Return(nullptr));
  EXPECT_CALL(mock_action_adapter_, OpenBookmark(testing::_, testing::_))
      .Times(0);
  controller_->OpenBookmark(42, WindowOpenDisposition::NEW_WINDOW);
}

TEST_F(BookmarkBarUIControllerImplTest, OpenFolderCurrentTabShowsMenu) {
  controller_->Bind(&mock_client_);
  bookmarks::BookmarkNodeId folder_id = int64_t{42};
  EXPECT_CALL(mock_action_adapter_, NotifyFolderOpened());
  EXPECT_CALL(mock_client_, ShowFolderMenu(folder_id));
  controller_->OpenFolder(folder_id, WindowOpenDisposition::CURRENT_TAB);
}

TEST_F(BookmarkBarUIControllerImplTest, OpenFolderOtherDispositionOpensNodes) {
  auto bookmark_model = bookmarks::TestBookmarkClient::CreateModel();
  const bookmarks::BookmarkNode* bar_node = bookmark_model->bookmark_bar_node();

  controller_->Bind(&mock_client_);
  bookmarks::BookmarkNodeId folder_id = int64_t{42};
  EXPECT_CALL(mock_model_adapter_, GetUnderlyingNodes(folder_id))
      .WillOnce(testing::Return(
          std::vector<const bookmarks::BookmarkNode*>{bar_node}));
  EXPECT_CALL(mock_action_adapter_,
              OpenFolderNodes(testing::ElementsAre(bar_node),
                              WindowOpenDisposition::NEW_WINDOW));
  EXPECT_CALL(mock_client_, ShowFolderMenu(testing::_)).Times(0);
  controller_->OpenFolder(folder_id, WindowOpenDisposition::NEW_WINDOW);
}

TEST_F(BookmarkBarUIControllerImplTest, OpenFolderOtherDispositionEmptyNodes) {
  controller_->Bind(&mock_client_);
  bookmarks::BookmarkNodeId folder_id = int64_t{42};
  EXPECT_CALL(mock_model_adapter_, GetUnderlyingNodes(folder_id))
      .WillOnce(testing::Return(std::vector<const bookmarks::BookmarkNode*>{}));
  EXPECT_CALL(mock_action_adapter_, OpenFolderNodes(testing::_, testing::_))
      .Times(0);
  EXPECT_CALL(mock_client_, ShowFolderMenu(testing::_)).Times(0);
  controller_->OpenFolder(folder_id, WindowOpenDisposition::NEW_WINDOW);
}

TEST_F(BookmarkBarUIControllerImplTest, ShowContextMenuDelegates) {
  auto bookmark_model = bookmarks::TestBookmarkClient::CreateModel();
  const bookmarks::BookmarkNode* node =
      bookmark_model->AddURL(bookmark_model->bookmark_bar_node(), 0, u"Title",
                             GURL("https://google.com"));

  bookmarks::BookmarkNodeId target = node->id();
  gfx::Point point(100, 200);
  ui::mojom::MenuSourceType source_type = ui::mojom::MenuSourceType::kMouse;

  EXPECT_CALL(mock_model_adapter_, GetNodeById(node->id()))
      .WillOnce(testing::Return(node));
  EXPECT_CALL(mock_model_adapter_,
              CanPasteFromClipboard(testing::_, testing::_))
      .WillOnce(base::test::RunOnceCallback<1>(true));

  base::MockCallback<base::OnceClosure> close_callback;
  EXPECT_CALL(mock_action_adapter_,
              ShowContextMenu(testing::ElementsAre(node), point, source_type,
                              true, testing::_));
  controller_->ShowContextMenu(target, point, source_type,
                               close_callback.Get());
}

TEST_F(BookmarkBarUIControllerImplTest, ShowContextMenuNodeNotFound) {
  bookmarks::BookmarkNodeId target = int64_t{42};
  gfx::Point point(100, 200);
  ui::mojom::MenuSourceType source_type = ui::mojom::MenuSourceType::kMouse;

  EXPECT_CALL(mock_model_adapter_, GetNodeById(42))
      .WillOnce(testing::Return(nullptr));
  EXPECT_CALL(mock_action_adapter_,
              ShowContextMenu(testing::_, testing::_, testing::_, testing::_,
                              testing::_))
      .Times(0);

  base::MockCallback<base::OnceClosure> close_callback;
  EXPECT_CALL(close_callback, Run());
  controller_->ShowContextMenu(target, point, source_type,
                               close_callback.Get());
}

}  // namespace
