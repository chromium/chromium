// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_STATS_H_
#define CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_STATS_H_

#include "base/time/time.h"
#include "components/profile_metrics/browser_profile_type.h"

class Profile;

namespace bookmarks {
class BookmarkNode;
struct BookmarkNodeData;
}  // namespace bookmarks

// This enum is used for the Bookmarks.EntryPoint histogram.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class BookmarkEntryPoint {
  kAccelerator = 0,
  kStarGesture = 1,
  kStarKey = 2,
  kStarMouse = 3,

  kMaxValue = kStarMouse
};

// This enum is used for the Bookmarks.LaunchLocation histogram.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class BookmarkLaunchLocation {
  kNone,
  kAttachedBar = 0,
  // kDetachedBar = 1, (deprecated)
  // These two are kind of sub-categories of the bookmark bar. Generally
  // a launch from a context menu or subfolder could be classified in one of
  // the other two bar buckets, but doing so is difficult because the menus
  // don't know of their greater place in Chrome.
  kSubfolder = 2,
  kContextMenu = 3,

  // Bookmarks menu within app menu.
  kAppMenu = 4,
  // Bookmark manager.
  kManager = 5,
  // Autocomplete suggestion.
  kOmnibox = 6,
  // System application menu (e.g. on Mac).
  kTopMenu = 7,

  // Bookmarks top level folder (i.e. bookmarks bar, other bookmarks) within the
  // side panel.
  kSidePanelFolder = 8,
  // Bookmarks subfolder within the side panel.
  kSidePanelSubfolder = 9,
  // Reading list tab within the side panel.
  kSidePanelPendingList = 10,
  // Reading list bubble in the bookmarks bar.
  kReadingListDialog = 11,
  // Context menu for a bookmark node in the side panel.
  kSidePanelContextMenu = 12,

  kMaxValue = kSidePanelContextMenu
};

// Captures information related to a bookmark's launch event. Used for metrics
// collection.
struct BookmarkLaunchAction {
  // The location of the open action.
  BookmarkLaunchLocation location = BookmarkLaunchLocation::kNone;

  // The time at which the launch action was initiated.
  base::TimeTicks action_time = base::TimeTicks::Now();
};
std::ostream& operator<<(std::ostream& out, const BookmarkLaunchAction& action);

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

// Records that the user edited or renamed a bookmark.
void RecordBookmarkEdited(BookmarkLaunchLocation location);

// Records that the user removed a bookmark.
void RecordBookmarkRemoved(BookmarkLaunchLocation location);

// Records the user adding a bookmark via star action, drag and drop, via
// Bookmark this tab... and Bookmark all tabs... buttons. For the Bookmark
// open tabs... the action is recorded only once and not as many times as
// count of tabs that were bookmarked.
void RecordBookmarksAdded(const Profile* profile);

// Records the user bookmarking all tabs, along with the open tabs count.
void RecordBookmarkAllTabsWithTabsCount(const Profile* profile, int count);

// Records that a bookmark or bookmarks were dropped. Determines the type of
// drop operation based on the data and parent node.
void RecordBookmarkDropped(const bookmarks::BookmarkNodeData& data,
                           const bookmarks::BookmarkNode* parent_node,
                           bool is_reorder);

#endif  // CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_STATS_H_
