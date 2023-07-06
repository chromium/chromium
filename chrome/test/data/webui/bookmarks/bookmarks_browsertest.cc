// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/bookmarks/managed_bookmark_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/web_ui_mocha_browser_test.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/common/bookmark_pref_names.h"
#include "components/bookmarks/managed/managed_bookmark_service.h"
#include "components/bookmarks/test/bookmark_test_helpers.h"
#include "components/prefs/pref_service.h"
#include "components/user_prefs/user_prefs.h"
#include "content/public/test/browser_test.h"

class BookmarksBrowserTest : public WebUIMochaBrowserTest {
 protected:
  BookmarksBrowserTest() {
    set_test_loader_host(chrome::kChromeUIBookmarksHost);
  }
};

typedef BookmarksBrowserTest BookmarksActionsTest;
IN_PROC_BROWSER_TEST_F(BookmarksActionsTest, All) {
  RunTest("bookmarks/actions_test.js", "mocha.run()");
}

typedef BookmarksBrowserTest BookmarksAppTest;
IN_PROC_BROWSER_TEST_F(BookmarksAppTest, All) {
  RunTest("bookmarks/app_test.js", "mocha.run()");
}

typedef BookmarksBrowserTest BookmarksCommandManagerTest;

// https://crbug.com/1010381: Flaky.
IN_PROC_BROWSER_TEST_F(BookmarksCommandManagerTest, DISABLED_All) {
  RunTest("bookmarks/command_manager_test.js", "mocha.run()");
}

typedef BookmarksBrowserTest BookmarksEditDialogTest;
IN_PROC_BROWSER_TEST_F(BookmarksEditDialogTest, All) {
  RunTest("bookmarks/edit_dialog_test.js", "mocha.run()");
}

typedef BookmarksBrowserTest BookmarksItemTest;
IN_PROC_BROWSER_TEST_F(BookmarksItemTest, All) {
  RunTest("bookmarks/item_test.js", "mocha.run()");
}

typedef BookmarksBrowserTest BookmarksListTest;
IN_PROC_BROWSER_TEST_F(BookmarksListTest, All) {
  RunTest("bookmarks/list_test.js", "mocha.run()");
}

typedef BookmarksBrowserTest BookmarksReducersTest;
IN_PROC_BROWSER_TEST_F(BookmarksReducersTest, All) {
  RunTest("bookmarks/reducers_test.js", "mocha.run()");
}

typedef BookmarksBrowserTest BookmarksRouterTest;
IN_PROC_BROWSER_TEST_F(BookmarksRouterTest, All) {
  RunTest("bookmarks/router_test.js", "mocha.run()");
}

typedef BookmarksBrowserTest BookmarksFolderNodeTest;
IN_PROC_BROWSER_TEST_F(BookmarksFolderNodeTest, All) {
  RunTest("bookmarks/folder_node_test.js", "mocha.run()");
}

typedef BookmarksBrowserTest BookmarksPolicyTest;
IN_PROC_BROWSER_TEST_F(BookmarksPolicyTest, All) {
  RunTest("bookmarks/policy_test.js", "mocha.run()");
}

typedef BookmarksBrowserTest BookmarksStoreTest;
IN_PROC_BROWSER_TEST_F(BookmarksStoreTest, All) {
  RunTest("bookmarks/store_test.js", "mocha.run()");
}

typedef BookmarksBrowserTest BookmarksToolbarTest;
IN_PROC_BROWSER_TEST_F(BookmarksToolbarTest, All) {
  RunTest("bookmarks/toolbar_test.js", "mocha.run()");
}

typedef BookmarksBrowserTest BookmarksUtilTest;
IN_PROC_BROWSER_TEST_F(BookmarksUtilTest, All) {
  RunTest("bookmarks/util_test.js", "mocha.run()");
}

class BookmarksExtensionAPITest : public BookmarksBrowserTest {
 protected:
  void SetupExtensionAPITest() {
    // Add managed bookmarks.
    Profile* profile = browser()->profile();
    bookmarks::BookmarkModel* model =
        BookmarkModelFactory::GetForBrowserContext(profile);
    bookmarks::ManagedBookmarkService* managed =
        ManagedBookmarkServiceFactory::GetForProfile(profile);
    bookmarks::test::WaitForBookmarkModelToLoad(model);

    base::Value::List list;
    base::Value::Dict node;
    node.Set("name", "Managed Bookmark");
    node.Set("url", "http://www.chromium.org");
    list.Append(node.Clone());
    node.clear();
    node.Set("name", "Managed Folder");
    node.Set("children", base::Value::List());
    list.Append(std::move(node));
    profile->GetPrefs()->Set(bookmarks::prefs::kManagedBookmarks,
                             base::Value(std::move(list)));
    ASSERT_EQ(2u, managed->managed_node()->children().size());
  }
};

IN_PROC_BROWSER_TEST_F(BookmarksExtensionAPITest, All) {
  SetupExtensionAPITest();
  RunTest("bookmarks/extension_api_test.js", "mocha.run()");
}

class BookmarksExtensionAPIEditDisabledTest : public BookmarksBrowserTest {
 protected:
  void SetupExtensionAPIEditDisabledTest() {
    Profile* profile = browser()->profile();

    // Provide some testing data here, since bookmark editing will be disabled
    // within the extension.
    bookmarks::BookmarkModel* model =
        BookmarkModelFactory::GetForBrowserContext(profile);
    bookmarks::test::WaitForBookmarkModelToLoad(model);
    const bookmarks::BookmarkNode* bar = model->bookmark_bar_node();
    const bookmarks::BookmarkNode* folder = model->AddFolder(bar, 0, u"Folder");
    model->AddURL(bar, 1, u"AAA", GURL("http://aaa.example.com"));
    model->AddURL(folder, 0, u"BBB", GURL("http://bbb.example.com"));

    PrefService* prefs = user_prefs::UserPrefs::Get(profile);
    prefs->SetBoolean(bookmarks::prefs::kEditBookmarksEnabled, false);
  }
};

IN_PROC_BROWSER_TEST_F(BookmarksExtensionAPIEditDisabledTest, All) {
  SetupExtensionAPIEditDisabledTest();
  RunTest("bookmarks/extension_api_test_edit_disabled.js", "mocha.run()");
}
