// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/bookmarks/bookmark_account_storage_move_dialog.h"

#include "base/functional/callback_helpers.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/browser/ui/views/bookmarks/bookmark_test_utils.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/signin/public/base/signin_switches.h"
#include "content/public/test/browser_test.h"

namespace {

// Tests that show the dialog, take a screenshot and compare against a baseline.
// Click handling is tested in
// `BookmarkAccountStorageMoveDialogInteractiveTest`.
class BookmarkAccountStorageMoveDialogPixelTest : public DialogBrowserTest {
 public:
  BookmarkAccountStorageMoveDialogPixelTest() = default;

  BookmarkAccountStorageMoveDialogPixelTest(
      const BookmarkAccountStorageMoveDialogPixelTest&) = delete;
  BookmarkAccountStorageMoveDialogPixelTest& operator=(
      const BookmarkAccountStorageMoveDialogPixelTest&) = delete;

  ~BookmarkAccountStorageMoveDialogPixelTest() override = default;

  void SetUpOnMainThread() override {
    DialogBrowserTest::SetUpOnMainThread();
    SignInAndEnableAccountBookmarkNodes(browser()->profile());
  }

  void TearDownOnMainThread() override {
    // Avoid dangling references.
    node_ = nullptr;
    target_folder_ = nullptr;
    DialogBrowserTest::TearDownOnMainThread();
  }

  // Setters for the dialog parameters. Call these before ShowAndVerifyUi().
  void set_node(const bookmarks::BookmarkNode* node) { node_ = node; }
  void set_target_folder(const bookmarks::BookmarkNode* target_folder) {
    target_folder_ = target_folder;
  }

  void ShowUi(const std::string& name) override {
    ASSERT_TRUE(node_) << "Must call set_node() before showing the dialog";
    ASSERT_TRUE(target_folder_)
        << "Must call set_target_folder() before showing the dialog";
    ShowBookmarkAccountStorageMoveDialog(browser(), node_, target_folder_,
                                         /*index=*/0);
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_{
      switches::kSyncEnableBookmarksInTransportMode};
  raw_ptr<const bookmarks::BookmarkNode> node_ = nullptr;
  raw_ptr<const bookmarks::BookmarkNode> target_folder_ = nullptr;
};

IN_PROC_BROWSER_TEST_F(BookmarkAccountStorageMoveDialogPixelTest,
                       InvokeUi_ShowMoveBookmarkToAccount) {
  bookmarks::BookmarkModel* bookmark_model =
      BookmarkModelFactory::GetForBrowserContext(browser()->profile());
  set_node(bookmark_model->AddURL(bookmark_model->bookmark_bar_node(),
                                  /*index=*/0, u"Local Bookmark",
                                  GURL("https://local.com")));
  set_target_folder(
      bookmark_model->AddFolder(bookmark_model->account_bookmark_bar_node(),
                                /*index=*/0, u"Account Folder"));

  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(BookmarkAccountStorageMoveDialogPixelTest,
                       InvokeUi_ShowMoveFolderToAccount) {
  bookmarks::BookmarkModel* bookmark_model =
      BookmarkModelFactory::GetForBrowserContext(browser()->profile());
  set_node(bookmark_model->AddFolder(bookmark_model->bookmark_bar_node(),
                                     /*index=*/0, u"Local Folder"));
  set_target_folder(
      bookmark_model->AddFolder(bookmark_model->account_bookmark_bar_node(),
                                /*index=*/0, u"Account Folder"));

  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(BookmarkAccountStorageMoveDialogPixelTest,
                       InvokeUi_ShowMoveBookmarkToDevice) {
  set_baseline("5895535");
  bookmarks::BookmarkModel* bookmark_model =
      BookmarkModelFactory::GetForBrowserContext(browser()->profile());
  set_node(bookmark_model->AddURL(bookmark_model->account_bookmark_bar_node(),
                                  /*index=*/0, u"Account Bookmark",
                                  GURL("https://account.com")));
  set_target_folder(
      bookmark_model->AddFolder(bookmark_model->bookmark_bar_node(),
                                /*index=*/0, u"Local Folder"));

  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(BookmarkAccountStorageMoveDialogPixelTest,
                       InvokeUi_ShowMoveFolderToDevice) {
  set_baseline("5895535");
  bookmarks::BookmarkModel* bookmark_model =
      BookmarkModelFactory::GetForBrowserContext(browser()->profile());
  set_node(
      bookmark_model->AddFolder(bookmark_model->account_bookmark_bar_node(),
                                /*index=*/0, u"Account Folder"));
  set_target_folder(
      bookmark_model->AddFolder(bookmark_model->bookmark_bar_node(),
                                /*index=*/0, u"Local Folder"));

  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(BookmarkAccountStorageMoveDialogPixelTest,
                       InvokeUi_TruncateLongFolderName) {
  set_baseline("6653664");
  bookmarks::BookmarkModel* bookmark_model =
      BookmarkModelFactory::GetForBrowserContext(browser()->profile());
  set_node(
      bookmark_model->AddFolder(bookmark_model->account_bookmark_bar_node(),
                                /*index=*/0, u"Account Folder"));
  set_target_folder(bookmark_model->AddFolder(
      bookmark_model->bookmark_bar_node(),
      /*index=*/0, u"Long long long long long local Folder"));

  ShowAndVerifyUi();
}

class SingleBookmarkUploadDialogPixelTest
    : public BookmarkAccountStorageMoveDialogPixelTest {
  void ShowUi(const std::string& name) override {
    ASSERT_TRUE(node_) << "Must call set_node() before showing the dialog";
    ASSERT_FALSE(target_folder_);
    ShowBookmarkAccountStorageUploadDialog(browser(), node_);
  }
};

IN_PROC_BROWSER_TEST_F(SingleBookmarkUploadDialogPixelTest, InvokeUi) {
  set_baseline("6653664");
  bookmarks::BookmarkModel* bookmark_model =
      BookmarkModelFactory::GetForBrowserContext(browser()->profile());
  set_node(bookmark_model->AddURL(bookmark_model->bookmark_bar_node(),
                                  /*index=*/0, u"Local Bookmark",
                                  GURL("https://local.com")));

  ShowAndVerifyUi();
}

}  // namespace
