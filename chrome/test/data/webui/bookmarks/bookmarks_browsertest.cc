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

using BookmarksTest = BookmarksBrowserTest;
IN_PROC_BROWSER_TEST_F(BookmarksTest, Actions) {
  RunTest("bookmarks/actions_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(BookmarksTest, App) {
  RunTest("bookmarks/app_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(BookmarksTest, CommandManager) {
  RunTest("bookmarks/command_manager_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(BookmarksTest, EditDialog) {
  RunTest("bookmarks/edit_dialog_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(BookmarksTest, Item) {
  RunTest("bookmarks/item_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(BookmarksTest, List) {
  RunTest("bookmarks/list_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(BookmarksTest, Reducers) {
  RunTest("bookmarks/reducers_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(BookmarksTest, Router) {
  RunTest("bookmarks/router_test.js", "mocha.run()");
}

// https://crbug.com/369045912: Flaky.
IN_PROC_BROWSER_TEST_F(BookmarksTest, DISABLED_FolderNode) {
  RunTest("bookmarks/folder_node_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(BookmarksTest, Policy) {
  RunTest("bookmarks/policy_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(BookmarksTest, Store) {
  RunTest("bookmarks/store_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(BookmarksTest, Toolbar) {
  RunTest("bookmarks/toolbar_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(BookmarksTest, Util) {
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

IN_PROC_BROWSER_TEST_F(BookmarksExtensionAPITest, All) {
  SetupExtensionAPITest();
  RunTest("bookmarks/extension_api_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(BookmarksExtensionAPITest, EditDisabled) {
  SetupExtensionAPIEditDisabledTest();
  RunTest("bookmarks/extension_api_test_edit_disabled.js", "mocha.run()");
}
