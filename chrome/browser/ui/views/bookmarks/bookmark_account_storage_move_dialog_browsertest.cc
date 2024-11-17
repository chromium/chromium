// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/bookmarks/bookmark_account_storage_move_dialog.h"

#include "base/functional/callback_helpers.h"
#include "base/notreached.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/sync/base/features.h"
#include "content/public/test/browser_test.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/gfx/image/image_unittest_util.h"

namespace {

void SignInAndEnableAccountBookmarkNodes(Profile* profile) {
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);
  AccountInfo account_info = signin::MakePrimaryAccountAvailable(
      identity_manager, "foo@gmail.com", signin::ConsentLevel::kSignin);
  signin::SimulateAccountImageFetch(identity_manager, account_info.account_id,
                                    "https://avatar.com/avatar.png",
                                    gfx::test::CreateImage(/*size=*/32));
  // Normally done by sync, but sync is not fully up in this test.
  BookmarkModelFactory::GetForBrowserContext(profile)
      ->CreateAccountPermanentFolders();
}

// Tests that show the dialog, take a screenshot and compare against a baseline.
// Click handling is tested in a different suite.
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
                                         /*index=*/0, base::DoNothing());
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_{
      syncer::kSyncEnableBookmarksInTransportMode};
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

// Tests click handling. The dialog appearance is tested on a different suite.
class BookmarkAccountStorageMoveDialogInteractiveTest
    : public InteractiveBrowserTest {
 public:
  BookmarkAccountStorageMoveDialogInteractiveTest() = default;
  BookmarkAccountStorageMoveDialogInteractiveTest(
      const BookmarkAccountStorageMoveDialogInteractiveTest&) = delete;
  BookmarkAccountStorageMoveDialogInteractiveTest& operator=(
      const BookmarkAccountStorageMoveDialogInteractiveTest&) = delete;
  ~BookmarkAccountStorageMoveDialogInteractiveTest() override = default;

  void SetUpOnMainThread() override {
    InteractiveBrowserTest::SetUpOnMainThread();
    SignInAndEnableAccountBookmarkNodes(browser()->profile());
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_{
      syncer::kSyncEnableBookmarksInTransportMode};
};

IN_PROC_BROWSER_TEST_F(BookmarkAccountStorageMoveDialogInteractiveTest,
                       PressOKButton) {
  bookmarks::BookmarkModel* bookmark_model =
      BookmarkModelFactory::GetForBrowserContext(browser()->profile());
  const bookmarks::BookmarkNode* source_folder =
      bookmark_model->bookmark_bar_node();
  const bookmarks::BookmarkNode* node =
      bookmark_model->AddFolder(source_folder, 0, u"Local");
  const bookmarks::BookmarkNode* target_folder = bookmark_model->AddFolder(
      bookmark_model->account_bookmark_bar_node(), 0, u"Account");
  const bookmarks::BookmarkNode* first_target_folder_node =
      bookmark_model->AddFolder(target_folder, 0, u"First");
  const bookmarks::BookmarkNode* last_target_folder_node =
      bookmark_model->AddFolder(target_folder, 1, u"Last");
  base::test::TestFuture<void> closed_waiter;
  ShowBookmarkAccountStorageMoveDialog(
      browser(), node, target_folder, /*index=*/1, closed_waiter.GetCallback());

  RunTestSequence(PressButton(kBookmarkAccountStorageMoveDialogOkButton));

  ASSERT_TRUE(closed_waiter.Wait());
  ASSERT_EQ(target_folder->children().size(), 3u);
  EXPECT_EQ(target_folder->children()[0].get(), first_target_folder_node);
  EXPECT_EQ(target_folder->children()[1].get(), node);
  EXPECT_EQ(target_folder->children()[2].get(), last_target_folder_node);
  EXPECT_EQ(source_folder->children().size(), 0u);
}

IN_PROC_BROWSER_TEST_F(BookmarkAccountStorageMoveDialogInteractiveTest,
                       PressCancelButton) {
  bookmarks::BookmarkModel* bookmark_model =
      BookmarkModelFactory::GetForBrowserContext(browser()->profile());
  const bookmarks::BookmarkNode* source_folder =
      bookmark_model->bookmark_bar_node();
  const bookmarks::BookmarkNode* node =
      bookmark_model->AddFolder(source_folder, 0, u"Local");
  const bookmarks::BookmarkNode* target_folder = bookmark_model->AddFolder(
      bookmark_model->account_bookmark_bar_node(), 0, u"Account");
  const bookmarks::BookmarkNode* first_target_folder_node =
      bookmark_model->AddFolder(target_folder, 0, u"First");
  const bookmarks::BookmarkNode* last_target_folder_node =
      bookmark_model->AddFolder(target_folder, 1, u"Last");
  base::test::TestFuture<void> closed_waiter;
  ShowBookmarkAccountStorageMoveDialog(
      browser(), node, target_folder, /*index=*/1, closed_waiter.GetCallback());

  RunTestSequence(PressButton(kBookmarkAccountStorageMoveDialogCancelButton));

  ASSERT_TRUE(closed_waiter.Wait());
  ASSERT_EQ(target_folder->children().size(), 2u);
  EXPECT_EQ(target_folder->children()[0].get(), first_target_folder_node);
  EXPECT_EQ(target_folder->children()[1].get(), last_target_folder_node);
  ASSERT_EQ(source_folder->children().size(), 1u);
  EXPECT_EQ(source_folder->children()[0].get(), node);
}

}  // namespace
