// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/bookmarks/controllers/bookmark_bar_ui_controller_impl.h"

#include <map>
#include <memory>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/bookmarks/controllers/adapters/bookmark_bar_action_adapter.h"
#include "chrome/browser/ui/bookmarks/controllers/adapters/bookmark_bar_prefs_adapter.h"
#include "chrome/browser/ui/bookmarks/controllers/bookmark_bar_ui_client.h"
#include "chrome/browser/ui/bookmarks/controllers/bookmark_bar_ui_controller_injector.h"
#include "components/bookmarks/common/bookmark_pref_names.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/window_open_disposition.h"

namespace {

class MockBookmarkBarUIClient : public BookmarkBarUIClient {
 public:
  MOCK_METHOD(void, SetAppsPageShortcutVisibility, (bool), (override));
  MOCK_METHOD(void, SetSavedTabGroupsVisibility, (bool), (override));
  MOCK_METHOD(void, SetManagedBookmarksFolderVisibility, (bool), (override));
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
  MOCK_METHOD(void, OpenBookmark, (int64_t, WindowOpenDisposition), (override));
};

class MockBookmarkBarUIControllerInjector
    : public BookmarkBarUIControllerInjector {
 public:
  MOCK_METHOD(BookmarkBarPrefsAdapter*, GetPrefsAdapter, (), (override));
  MOCK_METHOD(BookmarkBarActionAdapter*, GetActionAdapter, (), (override));
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

    controller_ =
        std::make_unique<BookmarkBarUIControllerImpl>(std::move(injector));
  }

  void TearDown() override {
    mock_injector_ = nullptr;
    controller_.reset();
  }

  testing::NiceMock<MockBookmarkBarPrefsAdapter> mock_prefs_adapter_;
  testing::NiceMock<MockBookmarkBarActionAdapter> mock_action_adapter_;
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
  EXPECT_CALL(mock_action_adapter_,
              OpenBookmark(42, WindowOpenDisposition::NEW_WINDOW));
  controller_->OpenBookmark(42, WindowOpenDisposition::NEW_WINDOW);
}

}  // namespace
