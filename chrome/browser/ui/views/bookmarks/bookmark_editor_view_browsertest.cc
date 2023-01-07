// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/bookmarks/bookmark_editor_view.h"

#include "chrome/browser/ui/bookmarks/bookmark_utils_desktop.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "content/public/test/browser_test.h"

// Test harness for integration tests using BookmarkEditorView.
class BookmarkEditorViewBrowserTest : public DialogBrowserTest {
 public:
  BookmarkEditorViewBrowserTest() {}

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
