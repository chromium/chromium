// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Constants for the names of various bookmarks preferences.

#ifndef COMPONENTS_BOOKMARKS_COMMON_BOOKMARK_PREF_NAMES_H_
#define COMPONENTS_BOOKMARKS_COMMON_BOOKMARK_PREF_NAMES_H_

namespace bookmarks::prefs {

// Boolean which specifies whether the user has added any new bookmarks
// following the launch of the power bookmarks feature.
inline constexpr char kAddedBookmarkSincePowerBookmarksLaunch[] =
    "bookmarks.added_since_power_bookmarks_launch";
// Boolean which specifies the ids of the bookmark nodes that are expanded in
// the bookmark editor.
inline constexpr char kBookmarkEditorExpandedNodes[] =
    "bookmark_editor.expanded_nodes";
// Modifying bookmarks is completely disabled when this is set to false.
inline constexpr char kEditBookmarksEnabled[] = "bookmarks.editing_enabled";
// A list of bookmarks to include in a Managed Bookmarks root node. Each
// list item is a dictionary containing a "name" and an "url" entry, detailing
// the bookmark name and target URL respectively.
inline constexpr char kManagedBookmarks[] = "bookmarks.managed_bookmarks";
// String which specifies the Managed Bookmarks folder name
inline constexpr char kManagedBookmarksFolderName[] =
    "bookmarks.managed_bookmarks_folder_name";
// Boolean which specifies whether the apps shortcut is visible on the bookmark
// bar.
inline constexpr char kShowAppsShortcutInBookmarkBar[] =
    "bookmark_bar.show_apps_shortcut";

// Boolean which specifies whether the saved tab groups are visible on the
// bookmark bar.
inline constexpr char kShowTabGroupsInBookmarkBar[] =
    "bookmark_bar.show_tab_groups";

// Boolean which specifies whether the Managed Bookmarks folder is visible on
// the bookmark bar.
inline constexpr char kShowManagedBookmarksInBookmarkBar[] =
    "bookmark_bar.show_managed_bookmarks";
// Boolean which specifies whether the bookmark bar is visible on all tabs.
inline constexpr char kShowBookmarkBar[] = "bookmark_bar.show_on_all_tabs";

}  // namespace bookmarks::prefs

#endif  // COMPONENTS_BOOKMARKS_COMMON_BOOKMARK_PREF_NAMES_H_
