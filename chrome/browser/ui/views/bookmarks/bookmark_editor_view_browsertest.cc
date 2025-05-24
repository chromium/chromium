// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/bookmarks/bookmark_editor_view.h"

#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/bookmarks/bookmark_utils_desktop.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
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
  // Shows the dialog for bookmarking all tabs. This shows a BookmarkEditorView
  // dialog, with a tree view, where a user can rename and select a parent
  // folder.
  void ShowUi(const std::string& name) override {
    chrome::ShowBookmarkAllTabsDialog(browser());
  }
};

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

IN_PROC_BROWSER_TEST_F(BookmarkEditorViewBrowserTestWithAccountBookmarks,
                       InvokeUi_Default) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(BookmarkEditorViewBrowserTestWithAccountBookmarks,
                       InvokeUi_AccountAndLocalNodes) {
  bookmarks::BookmarkModel* const bookmark_model =
      BookmarkModelFactory::GetForBrowserContext(browser()->profile());
  bookmark_model->AddFolder(bookmark_model->bookmark_bar_node(),
                            /*index=*/0, u"Local Folder");
  bookmark_model->AddFolder(bookmark_model->account_bookmark_bar_node(),
                            /*index=*/0, u"Account Folder");

  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(BookmarkEditorViewBrowserTestWithAccountBookmarks,
                       InvokeUi_OnlyAccountNodes) {
  bookmarks::BookmarkModel* const bookmark_model =
      BookmarkModelFactory::GetForBrowserContext(browser()->profile());
  bookmark_model->AddFolder(bookmark_model->account_bookmark_bar_node(),
                            /*index=*/0, u"Account Folder");
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(BookmarkEditorViewBrowserTestWithAccountBookmarks,
                       InvokeUi_OnlyLocalNodes) {
  bookmarks::BookmarkModel* const bookmark_model =
      BookmarkModelFactory::GetForBrowserContext(browser()->profile());
  bookmark_model->AddFolder(bookmark_model->other_node(),
                            /*index=*/0, u"Local Folder");

  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(BookmarkEditorViewBrowserTestWithAccountBookmarks,
                       InvokeUi_OnlyLocalChildren) {
  bookmarks::BookmarkModel* const bookmark_model =
      BookmarkModelFactory::GetForBrowserContext(browser()->profile());
  bookmark_model->AddURL(bookmark_model->other_node(), 0, u"bookmark 2",
                         GURL("http://www.google.com"));
  bookmark_model->AddURL(bookmark_model->bookmark_bar_node(), 0, u"bookmark",
                         GURL("http://www.google.com"));

  ShowAndVerifyUi();
}

class BookmarkEditorViewBrowserTestMoveDialog
    : public BookmarkEditorViewBrowserTest {
 public:
  // DialogBrowserTest:
  // Shows the dialog for moving one or multiple bookmarks. This shows a
  // BookmarkEditorView dialog with a tree view.
  void ShowUi(const std::string& name) override {
    BookmarkEditor::Show(
        browser()->window()->GetNativeWindow(), browser()->profile(),
        BookmarkEditor::EditDetails::MoveNodes(bookmark_model(), nodes_),
        BookmarkEditor::SHOW_TREE);
  }

  void set_nodes(const std::vector<raw_ptr<const bookmarks::BookmarkNode,
                                           VectorExperimental>>& nodes) {
    nodes_ = nodes;
  }

  bookmarks::BookmarkModel* bookmark_model() {
    return BookmarkModelFactory::GetForBrowserContext(browser()->profile());
  }

 private:
  std::vector<raw_ptr<const bookmarks::BookmarkNode, VectorExperimental>>
      nodes_;
};

IN_PROC_BROWSER_TEST_F(BookmarkEditorViewBrowserTestMoveDialog,
                       InvokeUi_MoveNodes) {
  const std::vector<raw_ptr<const bookmarks::BookmarkNode, VectorExperimental>>
      nodes({bookmark_model()->AddURL(bookmark_model()->other_node(), 0,
                                      u"bookmark 2",
                                      GURL("http://www.google.com")),
             bookmark_model()->AddURL(bookmark_model()->other_node(), 0,
                                      u"bookmark",
                                      GURL("http://www.google.com"))});

  set_nodes(nodes);
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(BookmarkEditorViewBrowserTestMoveDialog,
                       DeleteNodeDuringMoveDialog) {
  raw_ptr<BookmarkEditorView> editor_raw = nullptr;

  std::vector<raw_ptr<const bookmarks::BookmarkNode, VectorExperimental>> nodes(
      {bookmark_model()->AddURL(bookmark_model()->other_node(), 0,
                                u"bookmark 2", GURL("http://www.google.com")),
       bookmark_model()->AddURL(bookmark_model()->other_node(), 0, u"bookmark",
                                GURL("http://www.google.com"))});

  auto editor = std::make_unique<BookmarkEditorView>(
      browser()->profile(),
      BookmarkEditor::EditDetails::MoveNodes(bookmark_model(), nodes),
      BookmarkEditor::SHOW_TREE, base::DoNothing());
  editor_raw = editor.get();
  editor->Show(browser()->window()->GetNativeWindow());

  // `BookmarkEditorView` is self-deleting.
  editor.release();

  ASSERT_FALSE(editor_raw->GetWidget()->IsClosed());
  bookmark_model()->Remove(
      nodes[0], bookmarks::metrics::BookmarkEditSource::kOther, FROM_HERE);
  EXPECT_TRUE(editor_raw->GetWidget()->IsClosed());
}
