// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/bookmarks/common/bookmark_pref_names.h"

namespace bookmarks {
namespace prefs {

// Boolean which specifies the ids of the bookmark nodes that are expanded in
// the bookmark editor.
const char kBookmarkEditorExpandedNodes[] = "bookmark_editor.expanded_nodes";

// Modifying bookmarks is completely disabled when this is set to false.
const char kEditBookmarksEnabled[] = "bookmarks.editing_enabled";

// A list of bookmarks to include in a Managed Bookmarks root node. Each
// list item is a dictionary containing a "name" and an "url" entry, detailing
// the bookmark name and target URL respectively.
const char kManagedBookmarks[] = "bookmarks.managed_bookmarks";

// String which specifies the Managed Bookmarks folder name
const char kManagedBookmarksFolderName[] =
    "bookmarks.managed_bookmarks_folder_name";

// Boolean which specifies whether the apps shortcut is visible on the bookmark
// bar.
const char kShowAppsShortcutInBookmarkBar[] = "bookmark_bar.show_apps_shortcut";

// Boolean which specifies whether the Managed Bookmarks folder is visible on
// the bookmark bar.
const char kShowManagedBookmarksInBookmarkBar[] =
    "bookmark_bar.show_managed_bookmarks";

// Boolean which specifies whether the bookmark bar is visible on all tabs.
const char kShowBookmarkBar[] = "bookmark_bar.show_on_all_tabs";

}  // namespace prefs
}  // namespace bookmarks
