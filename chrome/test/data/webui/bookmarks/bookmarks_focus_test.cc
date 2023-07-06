// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/web_ui_mocha_browser_test.h"
#include "content/public/test/browser_test.h"

class BookmarksFocusTest : public WebUIMochaFocusTest {
 protected:
  BookmarksFocusTest() { set_test_loader_host(chrome::kChromeUIBookmarksHost); }
};

typedef BookmarksFocusTest BookmarksFolderNodeFocusTest;
IN_PROC_BROWSER_TEST_F(BookmarksFolderNodeFocusTest, All) {
  RunTest("bookmarks/folder_node_focus_test.js", "mocha.run()");
}

typedef BookmarksFocusTest BookmarksListFocusTest;

// http://crbug.com/1000950 : Flaky.
IN_PROC_BROWSER_TEST_F(BookmarksListFocusTest, DISABLED_All) {
  RunTest("bookmarks/list_focus_test.js", "mocha.run()");
}

typedef BookmarksFocusTest BookmarksDialogFocusManagerTest;

// http://crbug.com/1000950 : Flaky.
IN_PROC_BROWSER_TEST_F(BookmarksDialogFocusManagerTest, DISABLED_All) {
  RunTest("bookmarks/dialog_focus_manager_test.js", "mocha.run()");
}

typedef BookmarksFocusTest BookmarksDNDManagerTest;

// TODO(https://crbug.com/1409439): Test is flaky.
IN_PROC_BROWSER_TEST_F(BookmarksDNDManagerTest, All) {
  RunTest("bookmarks/dnd_manager_test.js", "mocha.run()");
}
