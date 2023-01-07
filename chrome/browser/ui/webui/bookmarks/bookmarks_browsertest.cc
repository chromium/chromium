// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/bookmarks/bookmarks_browsertest.h"

#include <memory>
#include <utility>

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/bookmarks/managed_bookmark_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/pref_names.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/common/bookmark_pref_names.h"
#include "components/bookmarks/managed/managed_bookmark_service.h"
#include "components/bookmarks/test/bookmark_test_helpers.h"
#include "components/prefs/pref_service.h"
#include "components/user_prefs/user_prefs.h"

BookmarksBrowserTest::BookmarksBrowserTest() {}

BookmarksBrowserTest::~BookmarksBrowserTest() {}

void BookmarksBrowserTest::SetupExtensionAPITest() {
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

void BookmarksBrowserTest::SetupExtensionAPIEditDisabledTest() {
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
