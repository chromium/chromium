// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/bookmarks/bookmark_editor_view.h"

#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/bookmarks/bookmark_utils_desktop.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/browser/ui/views/bookmarks/bookmark_test_utils.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/signin/public/base/signin_switches.h"
#include "content/public/test/browser_test.h"

// Test harness for integration tests using BookmarkEditorView.
class BookmarkEditorViewBrowserTest : public DialogBrowserTest {
 public:
  BookmarkEditorViewBrowserTest() = default;

  BookmarkEditorViewBrowserTest(const BookmarkEditorViewBrowserTest&) = delete;
  BookmarkEditorViewBrowserTest& operator=(
      const BookmarkEditorViewBrowserTest&) = delete;

  // DialogBrowserTest:
  void ShowUi(const std::string& name) override {
    DCHECK_EQ("all_tabs", name);
    chrome::ShowBookmarkAllTabsDialog(browser());
  }
};

// Shows the dialog for bookmarking all tabs. This shows a BookmarkEditorView
// dialog, with a tree view, where a user can rename and select a parent folder.
IN_PROC_BROWSER_TEST_F(BookmarkEditorViewBrowserTest, InvokeUi_all_tabs) {
  ShowAndVerifyUi();
}

// Variant used for account bookmarks.
class BookmarkEditorViewBrowserTestWithAccountBookmarks
    : public BookmarkEditorViewBrowserTest {
 public:
  BookmarkEditorViewBrowserTestWithAccountBookmarks() = default;
  BookmarkEditorViewBrowserTestWithAccountBookmarks(
      const BookmarkEditorViewBrowserTestWithAccountBookmarks&) = delete;
  BookmarkEditorViewBrowserTestWithAccountBookmarks& operator=(
      const BookmarkEditorViewBrowserTestWithAccountBookmarks&) = delete;
  ~BookmarkEditorViewBrowserTestWithAccountBookmarks() override = default;

  void SetUpOnMainThread() override {
    DialogBrowserTest::SetUpOnMainThread();
    SignInAndEnableAccountBookmarkNodes(browser()->profile());
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_{
      switches::kSyncEnableBookmarksInTransportMode};
};

// TODO(crbug.com/354892429): Add test coverage for:
// * Only local/syncable nodes
// * Local nodes with no children
IN_PROC_BROWSER_TEST_F(BookmarkEditorViewBrowserTestWithAccountBookmarks,
                       InvokeUi_all_tabs) {
  bookmarks::BookmarkModel* const bookmark_model =
      BookmarkModelFactory::GetForBrowserContext(browser()->profile());
  bookmark_model->AddFolder(bookmark_model->bookmark_bar_node(),
                            /*index=*/0, u"Local Folder");
  bookmark_model->AddFolder(bookmark_model->account_bookmark_bar_node(),
                            /*index=*/0, u"Account Folder");

  ShowAndVerifyUi();
}
