// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/notreached.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/bookmarks/bookmark_merged_surface_service.h"
#include "chrome/browser/bookmarks/bookmark_merged_surface_service_factory.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_window.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/views/bookmarks/bookmark_account_storage_move_dialog.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_navigation_observer.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/gfx/image/image_unittest_util.h"

namespace {

// Tests click handling. The dialog appearance is tested in
// `BookmarkAccountStorageMoveDialogPixelTest`.
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
    SetUpTest();
  }

  BookmarkMergedSurfaceService* service() {
    return BookmarkMergedSurfaceServiceFactory::GetForProfile(
        browser()->profile());
  }

 protected:
  void SetUpTest() {
    signin::IdentityManager* identity_manager =
        IdentityManagerFactory::GetForProfile(browser()->profile());
    AccountInfo account_info = signin::MakePrimaryAccountAvailable(
        identity_manager, "foo@gmail.com", signin::ConsentLevel::kSignin);
    signin::SimulateAccountImageFetch(identity_manager, account_info.account_id,
                                      "https://avatar.com/avatar.png",
                                      gfx::test::CreateImage(/*size=*/32));
    BookmarkModelFactory::GetForBrowserContext(browser()->profile())
        ->CreateAccountPermanentFolders();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_{
      switches::kSyncEnableBookmarksInTransportMode};
};

IN_PROC_BROWSER_TEST_F(BookmarkAccountStorageMoveDialogInteractiveTest,
                       PressOKButton) {
  base::HistogramTester histogram_tester;

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
  ShowBookmarkAccountStorageMoveDialog(browser(), node, target_folder,
                                       /*index=*/1,
                                       closed_waiter.GetCallback());

  RunTestSequence(PressButton(kBookmarkAccountStorageMoveDialogOkButton));

  ASSERT_TRUE(closed_waiter.Wait());
  ASSERT_EQ(target_folder->children().size(), 3u);
  EXPECT_EQ(target_folder->children()[0].get(), first_target_folder_node);
  EXPECT_EQ(target_folder->children()[1].get(), node);
  EXPECT_EQ(target_folder->children()[2].get(), last_target_folder_node);
  EXPECT_EQ(source_folder->children().size(), 0u);

  histogram_tester.ExpectBucketCount(
      "BookmarkAccountStorageMoveDialog.Upload.Shown",
      BookmarkAccountStorageMoveDialogType::kDownloadOrUpload, 1);
  histogram_tester.ExpectBucketCount(
      "BookmarkAccountStorageMoveDialog.Upload.Accepted",
      BookmarkAccountStorageMoveDialogType::kDownloadOrUpload, 1);
  histogram_tester.ExpectBucketCount(
      "BookmarkAccountStorageMoveDialog.Upload.Declined",
      BookmarkAccountStorageMoveDialogType::kDownloadOrUpload, 0);
  histogram_tester.ExpectBucketCount(
      "BookmarkAccountStorageMoveDialog.Upload.ExplicitlyClosed",
      BookmarkAccountStorageMoveDialogType::kDownloadOrUpload, 0);

  histogram_tester.ExpectTotalCount(
      "BookmarkAccountStorageMoveDialog.Download.Shown", 0);
  histogram_tester.ExpectTotalCount(
      "BookmarkAccountStorageMoveDialog.Download.Accepted", 0);
  histogram_tester.ExpectTotalCount(
      "BookmarkAccountStorageMoveDialog.Download.Declined", 0);
  histogram_tester.ExpectTotalCount(
      "BookmarkAccountStorageMoveDialog.Download.ExplicitlyClosed", 0);
}

IN_PROC_BROWSER_TEST_F(BookmarkAccountStorageMoveDialogInteractiveTest,
                       PressCancelButton) {
  base::HistogramTester histogram_tester;

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
  ShowBookmarkAccountStorageMoveDialog(browser(), node, target_folder,
                                       /*index=*/1,
                                       closed_waiter.GetCallback());

  RunTestSequence(PressButton(kBookmarkAccountStorageMoveDialogCancelButton));

  ASSERT_TRUE(closed_waiter.Wait());
  ASSERT_EQ(target_folder->children().size(), 2u);
  EXPECT_EQ(target_folder->children()[0].get(), first_target_folder_node);
  EXPECT_EQ(target_folder->children()[1].get(), last_target_folder_node);
  ASSERT_EQ(source_folder->children().size(), 1u);
  EXPECT_EQ(source_folder->children()[0].get(), node);

  histogram_tester.ExpectBucketCount(
      "BookmarkAccountStorageMoveDialog.Upload.Shown",
      BookmarkAccountStorageMoveDialogType::kDownloadOrUpload, 1);
  histogram_tester.ExpectBucketCount(
      "BookmarkAccountStorageMoveDialog.Upload.Accepted",
      BookmarkAccountStorageMoveDialogType::kDownloadOrUpload, 0);
  histogram_tester.ExpectBucketCount(
      "BookmarkAccountStorageMoveDialog.Upload.Declined",
      BookmarkAccountStorageMoveDialogType::kDownloadOrUpload, 1);
  histogram_tester.ExpectBucketCount(
      "BookmarkAccountStorageMoveDialog.Upload.ExplicitlyClosed",
      BookmarkAccountStorageMoveDialogType::kDownloadOrUpload, 0);

  histogram_tester.ExpectTotalCount(
      "BookmarkAccountStorageMoveDialog.Download.Shown", 0);
  histogram_tester.ExpectTotalCount(
      "BookmarkAccountStorageMoveDialog.Download.Accepted", 0);
  histogram_tester.ExpectTotalCount(
      "BookmarkAccountStorageMoveDialog.Download.Declined", 0);
  histogram_tester.ExpectTotalCount(
      "BookmarkAccountStorageMoveDialog.Download.ExplicitlyClosed", 0);
}

IN_PROC_BROWSER_TEST_F(BookmarkAccountStorageMoveDialogInteractiveTest,
                       FullFlowAcceptMoveFromAccountToLocalStorage) {
  base::HistogramTester histogram_tester;

  bookmarks::BookmarkModel* bookmark_model =
      BookmarkModelFactory::GetForBrowserContext(browser()->profile());
  const bookmarks::BookmarkNode* source_folder =
      bookmark_model->account_bookmark_bar_node();
  const bookmarks::BookmarkNode* node =
      bookmark_model->AddFolder(source_folder, 0, u"Account");
  const bookmarks::BookmarkNode* target_folder = bookmark_model->AddFolder(
      bookmark_model->bookmark_bar_node(), 0, u"Local");
  const bookmarks::BookmarkNode* first_target_folder_node =
      bookmark_model->AddFolder(target_folder, 0, u"First");
  const bookmarks::BookmarkNode* last_target_folder_node =
      bookmark_model->AddFolder(target_folder, 1, u"Last");

  BookmarkParentFolder destination =
      BookmarkParentFolder::FromFolderNode(target_folder);
  service()->Move(node, destination, 1, browser());
  RunTestSequence(PressButton(kBookmarkAccountStorageMoveDialogOkButton));

  ASSERT_EQ(target_folder->children().size(), 3u);
  EXPECT_EQ(target_folder->children()[0].get(), first_target_folder_node);
  EXPECT_EQ(target_folder->children()[1].get(), node);
  EXPECT_EQ(target_folder->children()[2].get(), last_target_folder_node);
  EXPECT_EQ(source_folder->children().size(), 0u);

  histogram_tester.ExpectTotalCount(
      "BookmarkAccountStorageMoveDialog.Upload.Shown", 0);
  histogram_tester.ExpectTotalCount(
      "BookmarkAccountStorageMoveDialog.Upload.Accepted", 0);
  histogram_tester.ExpectTotalCount(
      "BookmarkAccountStorageMoveDialog.Upload.Declined", 0);
  histogram_tester.ExpectTotalCount(
      "BookmarkAccountStorageMoveDialog.Upload.ExplicitlyClosed", 0);

  histogram_tester.ExpectTotalCount(
      "BookmarkAccountStorageMoveDialog.Download.Shown", 1);
  histogram_tester.ExpectTotalCount(
      "BookmarkAccountStorageMoveDialog.Download.Accepted", 1);
  histogram_tester.ExpectTotalCount(
      "BookmarkAccountStorageMoveDialog.Download.Declined", 0);
  histogram_tester.ExpectTotalCount(
      "BookmarkAccountStorageMoveDialog.Download.ExplicitlyClosed", 0);
}

IN_PROC_BROWSER_TEST_F(BookmarkAccountStorageMoveDialogInteractiveTest,
                       FullFlowCancelMoveFromAccountToLocalStorage) {
  base::HistogramTester histogram_tester;

  bookmarks::BookmarkModel* bookmark_model =
      BookmarkModelFactory::GetForBrowserContext(browser()->profile());
  const bookmarks::BookmarkNode* source_folder =
      bookmark_model->account_bookmark_bar_node();
  const bookmarks::BookmarkNode* node =
      bookmark_model->AddFolder(source_folder, 0, u"Account");
  const bookmarks::BookmarkNode* target_folder = bookmark_model->AddFolder(
      bookmark_model->bookmark_bar_node(), 0, u"Local");
  const bookmarks::BookmarkNode* first_target_folder_node =
      bookmark_model->AddFolder(target_folder, 0, u"First");
  const bookmarks::BookmarkNode* last_target_folder_node =
      bookmark_model->AddFolder(target_folder, 1, u"Last");

  BookmarkParentFolder destination =
      BookmarkParentFolder::FromFolderNode(target_folder);
  service()->Move(node, destination, 1, browser());
  RunTestSequence(PressButton(kBookmarkAccountStorageMoveDialogCancelButton));

  ASSERT_EQ(target_folder->children().size(), 2u);
  EXPECT_EQ(target_folder->children()[0].get(), first_target_folder_node);
  EXPECT_EQ(target_folder->children()[1].get(), last_target_folder_node);
  ASSERT_EQ(source_folder->children().size(), 1u);
  EXPECT_EQ(source_folder->children()[0].get(), node);

  histogram_tester.ExpectTotalCount(
      "BookmarkAccountStorageMoveDialog.Upload.Shown", 0);
  histogram_tester.ExpectTotalCount(
      "BookmarkAccountStorageMoveDialog.Upload.Accepted", 0);
  histogram_tester.ExpectTotalCount(
      "BookmarkAccountStorageMoveDialog.Upload.Declined", 0);
  histogram_tester.ExpectTotalCount(
      "BookmarkAccountStorageMoveDialog.Upload.ExplicitlyClosed", 0);

  histogram_tester.ExpectTotalCount(
      "BookmarkAccountStorageMoveDialog.Download.Shown", 1);
  histogram_tester.ExpectTotalCount(
      "BookmarkAccountStorageMoveDialog.Download.Accepted", 0);
  histogram_tester.ExpectTotalCount(
      "BookmarkAccountStorageMoveDialog.Download.Declined", 1);
  histogram_tester.ExpectTotalCount(
      "BookmarkAccountStorageMoveDialog.Download.ExplicitlyClosed", 0);
}

IN_PROC_BROWSER_TEST_F(BookmarkAccountStorageMoveDialogInteractiveTest,
                       FullFlowCancelMoveDialog) {
  base::HistogramTester histogram_tester;

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

  BookmarkParentFolder destination =
      BookmarkParentFolder::FromFolderNode(target_folder);
  service()->Move(node, destination, 1, browser());
  RunTestSequence(PressButton(kBookmarkAccountStorageMoveDialogCancelButton));

  ASSERT_EQ(target_folder->children().size(), 2u);
  EXPECT_EQ(target_folder->children()[0].get(), first_target_folder_node);
  EXPECT_EQ(target_folder->children()[1].get(), last_target_folder_node);
  ASSERT_EQ(source_folder->children().size(), 1u);
  EXPECT_EQ(source_folder->children()[0].get(), node);

  histogram_tester.ExpectTotalCount(
      "BookmarkAccountStorageMoveDialog.Upload.Shown", 1);
  histogram_tester.ExpectTotalCount(
      "BookmarkAccountStorageMoveDialog.Upload.Accepted", 0);
  histogram_tester.ExpectTotalCount(
      "BookmarkAccountStorageMoveDialog.Upload.Declined", 1);
  histogram_tester.ExpectTotalCount(
      "BookmarkAccountStorageMoveDialog.Upload.ExplicitlyClosed", 0);

  histogram_tester.ExpectTotalCount(
      "BookmarkAccountStorageMoveDialog.Download.Shown", 0);
  histogram_tester.ExpectTotalCount(
      "BookmarkAccountStorageMoveDialog.Download.Accepted", 0);
  histogram_tester.ExpectTotalCount(
      "BookmarkAccountStorageMoveDialog.Download.Declined", 0);
  histogram_tester.ExpectTotalCount(
      "BookmarkAccountStorageMoveDialog.Download.ExplicitlyClosed", 0);
}

IN_PROC_BROWSER_TEST_F(BookmarkAccountStorageMoveDialogInteractiveTest,
                       PressOKButtonUploadDialog) {
  base::HistogramTester histogram_tester;

  bookmarks::BookmarkModel* bookmark_model =
      BookmarkModelFactory::GetForBrowserContext(browser()->profile());
  const bookmarks::BookmarkNode* source_folder =
      bookmark_model->bookmark_bar_node();
  const bookmarks::BookmarkNode* node =
      bookmark_model->AddFolder(source_folder, 0, u"Local");
  const bookmarks::BookmarkNode* target_folder =
      bookmark_model->account_bookmark_bar_node();
  const bookmarks::BookmarkNode* first_target_folder_node =
      bookmark_model->AddFolder(target_folder, 0, u"First");
  const bookmarks::BookmarkNode* last_target_folder_node =
      bookmark_model->AddFolder(target_folder, 1, u"Last");
  base::test::TestFuture<void> closed_waiter;
  ShowBookmarkAccountStorageUploadDialog(browser(), node,
                                         closed_waiter.GetCallback());

  RunTestSequence(PressButton(kBookmarkAccountStorageMoveDialogOkButton));

  ASSERT_TRUE(closed_waiter.Wait());
  ASSERT_EQ(target_folder->children().size(), 3u);
  EXPECT_EQ(target_folder->children()[0].get(), first_target_folder_node);
  EXPECT_EQ(target_folder->children()[1].get(), last_target_folder_node);
  EXPECT_EQ(target_folder->children()[2].get(), node);
  EXPECT_EQ(source_folder->children().size(), 0u);

  histogram_tester.ExpectBucketCount(
      "BookmarkAccountStorageMoveDialog.Upload.Shown",
      BookmarkAccountStorageMoveDialogType::kUpload, 1);
  histogram_tester.ExpectBucketCount(
      "BookmarkAccountStorageMoveDialog.Upload.Accepted",
      BookmarkAccountStorageMoveDialogType::kUpload, 1);
  histogram_tester.ExpectBucketCount(
      "BookmarkAccountStorageMoveDialog.Upload.Declined",
      BookmarkAccountStorageMoveDialogType::kUpload, 0);
  histogram_tester.ExpectBucketCount(
      "BookmarkAccountStorageMoveDialog.Upload.ExplicitlyClosed",
      BookmarkAccountStorageMoveDialogType::kUpload, 0);

  histogram_tester.ExpectTotalCount(
      "BookmarkAccountStorageMoveDialog.Download.Shown", 0);
  histogram_tester.ExpectTotalCount(
      "BookmarkAccountStorageMoveDialog.Download.Accepted", 0);
  histogram_tester.ExpectTotalCount(
      "BookmarkAccountStorageMoveDialog.Download.Declined", 0);
  histogram_tester.ExpectTotalCount(
      "BookmarkAccountStorageMoveDialog.Download.ExplicitlyClosed", 0);
}

IN_PROC_BROWSER_TEST_F(BookmarkAccountStorageMoveDialogInteractiveTest,
                       PressCancelButtonUploadDialog) {
  base::HistogramTester histogram_tester;

  bookmarks::BookmarkModel* bookmark_model =
      BookmarkModelFactory::GetForBrowserContext(browser()->profile());
  const bookmarks::BookmarkNode* source_folder =
      bookmark_model->bookmark_bar_node();
  const bookmarks::BookmarkNode* node =
      bookmark_model->AddFolder(source_folder, 0, u"Local");
  const bookmarks::BookmarkNode* target_folder =
      bookmark_model->account_bookmark_bar_node();
  const bookmarks::BookmarkNode* first_target_folder_node =
      bookmark_model->AddFolder(target_folder, 0, u"First");
  const bookmarks::BookmarkNode* last_target_folder_node =
      bookmark_model->AddFolder(target_folder, 1, u"Last");
  base::test::TestFuture<void> closed_waiter;
  ShowBookmarkAccountStorageUploadDialog(browser(), node,
                                         closed_waiter.GetCallback());

  RunTestSequence(PressButton(kBookmarkAccountStorageMoveDialogCancelButton));

  ASSERT_TRUE(closed_waiter.Wait());
  ASSERT_EQ(target_folder->children().size(), 2u);
  EXPECT_EQ(target_folder->children()[0].get(), first_target_folder_node);
  EXPECT_EQ(target_folder->children()[1].get(), last_target_folder_node);
  ASSERT_EQ(source_folder->children().size(), 1u);
  EXPECT_EQ(source_folder->children()[0].get(), node);

  histogram_tester.ExpectBucketCount(
      "BookmarkAccountStorageMoveDialog.Upload.Shown",
      BookmarkAccountStorageMoveDialogType::kUpload, 1);
  histogram_tester.ExpectBucketCount(
      "BookmarkAccountStorageMoveDialog.Upload.Accepted",
      BookmarkAccountStorageMoveDialogType::kUpload, 0);
  histogram_tester.ExpectBucketCount(
      "BookmarkAccountStorageMoveDialog.Upload.Declined",
      BookmarkAccountStorageMoveDialogType::kUpload, 1);
  histogram_tester.ExpectBucketCount(
      "BookmarkAccountStorageMoveDialog.Upload.ExplicitlyClosed",
      BookmarkAccountStorageMoveDialogType::kUpload, 0);

  histogram_tester.ExpectTotalCount(
      "BookmarkAccountStorageMoveDialog.Download.Shown", 0);
  histogram_tester.ExpectTotalCount(
      "BookmarkAccountStorageMoveDialog.Download.Accepted", 0);
  histogram_tester.ExpectTotalCount(
      "BookmarkAccountStorageMoveDialog.Download.Declined", 0);
  histogram_tester.ExpectTotalCount(
      "BookmarkAccountStorageMoveDialog.Download.ExplicitlyClosed", 0);
}

IN_PROC_BROWSER_TEST_F(BookmarkAccountStorageMoveDialogInteractiveTest,
                       ExplicitlyCloseMoveDialog) {
  base::HistogramTester histogram_tester;
  const ui::Accelerator kEscapeKey(ui::VKEY_ESCAPE, ui::EF_NONE);

  bookmarks::BookmarkModel* bookmark_model =
      BookmarkModelFactory::GetForBrowserContext(browser()->profile());
  const bookmarks::BookmarkNode* local_folder = bookmark_model->AddFolder(
      bookmark_model->bookmark_bar_node(), 0, u"Local");
  const bookmarks::BookmarkNode* account_folder = bookmark_model->AddFolder(
      bookmark_model->account_bookmark_bar_node(), 0, u"Account");

  // Explicitly close upload dialog by clicking escape.
  BookmarkParentFolder destination =
      BookmarkParentFolder::FromFolderNode(account_folder);
  service()->Move(local_folder, destination, 1, browser());
  // We cannot add an element identifier to the dialog when it's built using
  // DialogModel::Builder. Thus, we check for its existence by checking the
  // visibility of its cancel button, even though we use the escape key to close
  // it.
  RunTestSequence(
      WaitForShow(kBookmarkAccountStorageMoveDialogCancelButton),
      SendAccelerator(kBookmarkAccountStorageMoveDialogCancelButton, kEscapeKey)
          .SetMustRemainVisible(false),
      WaitForHide(kBookmarkAccountStorageMoveDialogCancelButton));

  histogram_tester.ExpectTotalCount(
      "BookmarkAccountStorageMoveDialog.Upload.Shown", 1);
  histogram_tester.ExpectTotalCount(
      "BookmarkAccountStorageMoveDialog.Upload.Accepted", 0);
  histogram_tester.ExpectTotalCount(
      "BookmarkAccountStorageMoveDialog.Upload.Declined", 0);
  histogram_tester.ExpectTotalCount(
      "BookmarkAccountStorageMoveDialog.Upload.ExplicitlyClosed", 1);

  // Explicitly close download dialog by clicking escape.
  destination = BookmarkParentFolder::FromFolderNode(local_folder);
  service()->Move(account_folder, destination, 0, browser());
  RunTestSequence(
      WaitForShow(kBookmarkAccountStorageMoveDialogCancelButton),
      SendAccelerator(kBookmarkAccountStorageMoveDialogCancelButton, kEscapeKey)
          .SetMustRemainVisible(false),
      WaitForHide(kBookmarkAccountStorageMoveDialogCancelButton));

  histogram_tester.ExpectTotalCount(
      "BookmarkAccountStorageMoveDialog.Download.Shown", 1);
  histogram_tester.ExpectTotalCount(
      "BookmarkAccountStorageMoveDialog.Download.Accepted", 0);
  histogram_tester.ExpectTotalCount(
      "BookmarkAccountStorageMoveDialog.Download.Declined", 0);
  histogram_tester.ExpectTotalCount(
      "BookmarkAccountStorageMoveDialog.Download.ExplicitlyClosed", 1);
}

// Tests that removing the source node or destination folder (e.g., in another
// window) closes the upload dialog.
IN_PROC_BROWSER_TEST_F(BookmarkAccountStorageMoveDialogInteractiveTest,
                       UploadDialogClosesWhenSourceOrDestinationRemoved) {
  base::HistogramTester histogram_tester;

  bookmarks::BookmarkModel* bookmark_model = service()->bookmark_model();
  const bookmarks::BookmarkNode* local_folder = bookmark_model->AddFolder(
      bookmark_model->bookmark_bar_node(), 0, u"Local folder");
  const bookmarks::BookmarkNode* local_folder_bookmark =
      bookmark_model->AddURL(local_folder, 0, u"Local folder bookmark",
                             GURL("http://local-folder-bookmark/"));
  const bookmarks::BookmarkNode* local_bookmark =
      bookmark_model->AddURL(bookmark_model->bookmark_bar_node(), 1,
                             u"Local bookmark", GURL("http://local-bookmark/"));
  const bookmarks::BookmarkNode* account_folder = bookmark_model->AddFolder(
      bookmark_model->account_bookmark_bar_node(), 0, u"Account folder");

  BookmarkParentFolder destination =
      BookmarkParentFolder::FromFolderNode(account_folder);

  service()->Move(local_folder_bookmark, destination, 1, browser());
  RunTestSequence(WaitForShow(kBookmarkAccountStorageMoveDialogCancelButton),
                  Do([bookmark_model, local_folder] {
                    // Remove the folder that contains the bookmark being moved.
                    bookmark_model->Remove(
                        local_folder,
                        bookmarks::metrics::BookmarkEditSource::kOther,
                        FROM_HERE);
                  }),
                  WaitForHide(kBookmarkAccountStorageMoveDialogCancelButton));

  service()->Move(local_bookmark, destination, 1, browser());
  RunTestSequence(WaitForShow(kBookmarkAccountStorageMoveDialogCancelButton),
                  Do([bookmark_model, account_folder] {
                    // Remove the destination folder.
                    bookmark_model->Remove(
                        account_folder,
                        bookmarks::metrics::BookmarkEditSource::kOther,
                        FROM_HERE);
                  }),
                  WaitForHide(kBookmarkAccountStorageMoveDialogCancelButton));

  histogram_tester.ExpectTotalCount(
      "BookmarkAccountStorageMoveDialog.Upload.Shown", 2);
  histogram_tester.ExpectTotalCount(
      "BookmarkAccountStorageMoveDialog.Upload.Accepted", 0);
  histogram_tester.ExpectTotalCount(
      "BookmarkAccountStorageMoveDialog.Upload.Declined", 0);
  histogram_tester.ExpectTotalCount(
      "BookmarkAccountStorageMoveDialog.Upload.ExplicitlyClosed", 2);
}

// Tests that removing the source node or destination folder (e.g., in another
// window) closes the download dialog.
IN_PROC_BROWSER_TEST_F(BookmarkAccountStorageMoveDialogInteractiveTest,
                       DownloadDialogClosesWhenSourceOrDestinationRemoved) {
  base::HistogramTester histogram_tester;

  bookmarks::BookmarkModel* bookmark_model = service()->bookmark_model();
  const bookmarks::BookmarkNode* local_folder = bookmark_model->AddFolder(
      bookmark_model->bookmark_bar_node(), 0, u"Local folder");
  const bookmarks::BookmarkNode* account_bookmark = bookmark_model->AddURL(
      bookmark_model->account_bookmark_bar_node(), 0, u"Account bookmark",
      GURL("http://account-bookmark/"));
  const bookmarks::BookmarkNode* account_folder = bookmark_model->AddFolder(
      bookmark_model->account_bookmark_bar_node(), 1, u"Account folder");
  const bookmarks::BookmarkNode* account_folder_bookmark =
      bookmark_model->AddURL(account_folder, 0, u"Account folder bookmark",
                             GURL("http://account-folder-bookmark/"));

  BookmarkParentFolder destination =
      BookmarkParentFolder::FromFolderNode(local_folder);

  service()->Move(account_folder_bookmark, destination, 1, browser());
  RunTestSequence(WaitForShow(kBookmarkAccountStorageMoveDialogCancelButton),
                  Do([bookmark_model, account_folder] {
                    // Remove the folder that contains the bookmark being moved.
                    bookmark_model->Remove(
                        account_folder,
                        bookmarks::metrics::BookmarkEditSource::kOther,
                        FROM_HERE);
                  }),
                  WaitForHide(kBookmarkAccountStorageMoveDialogCancelButton));

  service()->Move(account_bookmark, destination, 1, browser());
  RunTestSequence(WaitForShow(kBookmarkAccountStorageMoveDialogCancelButton),
                  Do([bookmark_model, local_folder] {
                    // Remove the destination folder.
                    bookmark_model->Remove(
                        local_folder,
                        bookmarks::metrics::BookmarkEditSource::kOther,
                        FROM_HERE);
                  }),
                  WaitForHide(kBookmarkAccountStorageMoveDialogCancelButton));

  histogram_tester.ExpectTotalCount(
      "BookmarkAccountStorageMoveDialog.Download.Shown", 2);
  histogram_tester.ExpectTotalCount(
      "BookmarkAccountStorageMoveDialog.Download.Accepted", 0);
  histogram_tester.ExpectTotalCount(
      "BookmarkAccountStorageMoveDialog.Download.Declined", 0);
  histogram_tester.ExpectTotalCount(
      "BookmarkAccountStorageMoveDialog.Download.ExplicitlyClosed", 2);
}

// Tests that removing an unrelated bookmark does not close the upload dialog.
IN_PROC_BROWSER_TEST_F(BookmarkAccountStorageMoveDialogInteractiveTest,
                       UploadDialogStaysOpenWhenUnrelatedBookmarkRemoved) {
  base::HistogramTester histogram_tester;

  bookmarks::BookmarkModel* bookmark_model = service()->bookmark_model();
  const bookmarks::BookmarkNode* local_bookmark =
      bookmark_model->AddURL(bookmark_model->bookmark_bar_node(), 0,
                             u"Local bookmark", GURL("http://local-bookmark/"));
  const bookmarks::BookmarkNode* second_local_bookmark = bookmark_model->AddURL(
      bookmark_model->bookmark_bar_node(), 1, u"Local bookmark 2",
      GURL("http://local-bookmark-2/"));
  const bookmarks::BookmarkNode* account_folder =
      bookmark_model->account_bookmark_bar_node();

  base::test::TestFuture<void> closed_waiter;
  ShowBookmarkAccountStorageUploadDialog(browser(), local_bookmark,
                                         closed_waiter.GetCallback());

  RunTestSequence(PressButton(kBookmarkAccountStorageMoveDialogOkButton),
                  Do([bookmark_model, second_local_bookmark] {
                    // Remove the bookmark that is not being moved.
                    bookmark_model->Remove(
                        second_local_bookmark,
                        bookmarks::metrics::BookmarkEditSource::kOther,
                        FROM_HERE);
                  }));

  ASSERT_TRUE(closed_waiter.Wait());
  ASSERT_EQ(bookmark_model->bookmark_bar_node()->children().size(), 0u);
  ASSERT_EQ(account_folder->children().size(), 1u);
  EXPECT_EQ(account_folder->children()[0].get(), local_bookmark);

  histogram_tester.ExpectTotalCount(
      "BookmarkAccountStorageMoveDialog.Upload.Shown", 1);
  histogram_tester.ExpectTotalCount(
      "BookmarkAccountStorageMoveDialog.Upload.Accepted", 1);
  histogram_tester.ExpectTotalCount(
      "BookmarkAccountStorageMoveDialog.Upload.Declined", 0);
  histogram_tester.ExpectTotalCount(
      "BookmarkAccountStorageMoveDialog.Upload.ExplicitlyClosed", 0);
}

class BookmarkAccountStorageMoveDialogWithoutContextWidgetInteractiveTest
    : public BookmarkAccountStorageMoveDialogInteractiveTest {
 public:
  BookmarkAccountStorageMoveDialogWithoutContextWidgetInteractiveTest() =
      default;
  BookmarkAccountStorageMoveDialogWithoutContextWidgetInteractiveTest(
      const BookmarkAccountStorageMoveDialogWithoutContextWidgetInteractiveTest&) =
      delete;
  BookmarkAccountStorageMoveDialogWithoutContextWidgetInteractiveTest&
  operator=(
      const BookmarkAccountStorageMoveDialogWithoutContextWidgetInteractiveTest&) =
      delete;
  ~BookmarkAccountStorageMoveDialogWithoutContextWidgetInteractiveTest()
      override = default;

  void SetUpOnMainThread() override {
    // Do not call the parent `SetUpOnMainThread()` in order to make sure that
    // the context widget is not set to the current browser. The test itself
    // should override it with `SetContextWidget()`.
    SetUpTest();
  }
};

IN_PROC_BROWSER_TEST_F(
    BookmarkAccountStorageMoveDialogWithoutContextWidgetInteractiveTest,
    PressOKButtonInIncogntoMode) {
  base::HistogramTester histogram_tester;

  Profile* original_profile = browser()->profile();
  ASSERT_FALSE(original_profile->IsOffTheRecord());
  bookmarks::BookmarkModel* bookmark_model =
      BookmarkModelFactory::GetForBrowserContext(original_profile);
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

  // Create Incognito Mode browser.
  Profile* otr_profile =
      original_profile->GetPrimaryOTRProfile(/*create_if_needed*/ true);
  Browser* otr_browser = CreateBrowser(otr_profile);
  CloseBrowserSynchronously(browser());
  ASSERT_EQ(chrome::GetTotalBrowserCount(), 1u);

  // A new browser should open on the Original Profile with the bookmarks
  // manager tab opened.
  base::test::TestFuture<Browser*> browser_waiter;
  // Deletes itself.
  new profiles::BrowserAddedForProfileObserver(original_profile,
                                               browser_waiter.GetCallback());
  content::TestNavigationObserver bookmarks_manager_observer{
      GURL(chrome::kChromeUIBookmarksURL)};
  bookmarks_manager_observer.StartWatchingNewWebContents();

  base::test::TestFuture<void> closed_waiter;
  ShowBookmarkAccountStorageMoveDialog(otr_browser, node, target_folder,
                                       /*index=*/1,
                                       closed_waiter.GetCallback());

  Browser* new_browser = browser_waiter.Get();
  ASSERT_TRUE(new_browser);
  EXPECT_EQ(new_browser->profile(), original_profile);
  bookmarks_manager_observer.WaitForNavigationFinished();
  // No other browser was opened.
  ASSERT_EQ(chrome::GetTotalBrowserCount(), 2u);

  // Makes sure the context is the new browser. Otherwise the button cannot be
  // found. No prior context should be set in this test.
  SetContextWidget(
      BrowserView::GetBrowserViewForBrowser(new_browser)->GetWidget());
  RunTestSequence(PressButton(kBookmarkAccountStorageMoveDialogOkButton));

  ASSERT_TRUE(closed_waiter.Wait());
  ASSERT_EQ(target_folder->children().size(), 3u);
  EXPECT_EQ(target_folder->children()[0].get(), first_target_folder_node);
  EXPECT_EQ(target_folder->children()[1].get(), node);
  EXPECT_EQ(target_folder->children()[2].get(), last_target_folder_node);
  EXPECT_EQ(source_folder->children().size(), 0u);

  histogram_tester.ExpectBucketCount(
      "BookmarkAccountStorageMoveDialog.Upload.Shown",
      BookmarkAccountStorageMoveDialogType::kDownloadOrUpload, 1);
  histogram_tester.ExpectBucketCount(
      "BookmarkAccountStorageMoveDialog.Upload.Accepted",
      BookmarkAccountStorageMoveDialogType::kDownloadOrUpload, 1);
  histogram_tester.ExpectBucketCount(
      "BookmarkAccountStorageMoveDialog.Upload.Declined",
      BookmarkAccountStorageMoveDialogType::kDownloadOrUpload, 0);
  histogram_tester.ExpectBucketCount(
      "BookmarkAccountStorageMoveDialog.Upload.ExplicitlyClosed",
      BookmarkAccountStorageMoveDialogType::kDownloadOrUpload, 0);

  histogram_tester.ExpectTotalCount(
      "BookmarkAccountStorageMoveDialog.Download.Shown", 0);
  histogram_tester.ExpectTotalCount(
      "BookmarkAccountStorageMoveDialog.Download.Accepted", 0);
  histogram_tester.ExpectTotalCount(
      "BookmarkAccountStorageMoveDialog.Download.Declined", 0);
  histogram_tester.ExpectTotalCount(
      "BookmarkAccountStorageMoveDialog.Download.ExplicitlyClosed", 0);
}

}  // namespace
