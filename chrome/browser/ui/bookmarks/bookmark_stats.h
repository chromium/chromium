// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_STATS_H_
#define CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_STATS_H_

#include "components/profile_metrics/browser_profile_type.h"

class Profile;

// This enum is used for the Bookmarks.EntryPoint histogram.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum BookmarkEntryPoint {
  BOOKMARK_ENTRY_POINT_ACCELERATOR,
  BOOKMARK_ENTRY_POINT_STAR_GESTURE,
  BOOKMARK_ENTRY_POINT_STAR_KEY,
  BOOKMARK_ENTRY_POINT_STAR_MOUSE,

  BOOKMARK_ENTRY_POINT_LIMIT  // Keep this last.
};

// This enum is used for the Bookmarks.LaunchLocation histogram.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum BookmarkLaunchLocation {
  BOOKMARK_LAUNCH_LOCATION_NONE,
  BOOKMARK_LAUNCH_LOCATION_ATTACHED_BAR = 0,
  // BOOKMARK_LAUNCH_LOCATION_DETACHED_BAR = 1, (deprecated)
  // These two are kind of sub-categories of the bookmark bar. Generally
  // a launch from a context menu or subfolder could be classified in one of
  // the other two bar buckets, but doing so is difficult because the menus
  // don't know of their greater place in Chrome.
  BOOKMARK_LAUNCH_LOCATION_BAR_SUBFOLDER = 2,
  BOOKMARK_LAUNCH_LOCATION_CONTEXT_MENU = 3,

  // Bookmarks menu within app menu.
  BOOKMARK_LAUNCH_LOCATION_APP_MENU = 4,
  // Bookmark manager.
  BOOKMARK_LAUNCH_LOCATION_MANAGER = 5,
  // Autocomplete suggestion.
  BOOKMARK_LAUNCH_LOCATION_OMNIBOX = 6,
  // System application menu (e.g. on Mac).
  BOOKMARK_LAUNCH_LOCATION_TOP_MENU = 7,

  BOOKMARK_LAUNCH_LOCATION_LIMIT  // Keep this last.
};

// Records the launch of a bookmark for UMA purposes.
void RecordBookmarkLaunch(BookmarkLaunchLocation location,
                          profile_metrics::BrowserProfileType profile_type);

// Records the user launching all bookmarks in a folder (via middle-click, etc.)
// for UMA purposes.
void RecordBookmarkFolderLaunch(BookmarkLaunchLocation location);

// Records the user opening a folder of bookmarks for UMA purposes.
void RecordBookmarkFolderOpen(BookmarkLaunchLocation location);

// Records the user opening the apps page for UMA purposes.
void RecordBookmarkAppsPageOpen(BookmarkLaunchLocation location);

// Records the user adding a bookmark via star action, drag and drop, via
// Bookmark this tab... and Bookmark all tabs... buttons. For the Bookmark
// open tabs... the action is recorded only once and not as many times as
// count of tabs that were bookmarked.
void RecordBookmarksAdded(const Profile* profile);

// Records the user bookmarking all tabs, along with the open tabs count.
void RecordBookmarkAllTabsWithTabsCount(const Profile* profile, int count);

#endif  // CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_STATS_H_
