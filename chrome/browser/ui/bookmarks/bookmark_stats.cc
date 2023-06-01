// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/bookmarks/bookmark_stats.h"
#include "chrome/browser/profiles/profile.h"

#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_node_data.h"

using bookmarks::BookmarkNode;

namespace {

bool IsBookmarkBarLocation(BookmarkLaunchLocation location) {
  return location == BookmarkLaunchLocation::kAttachedBar ||
         location == BookmarkLaunchLocation::kSubfolder;
}

auto GetMetricProfile(const Profile* profile) {
  DCHECK(profile);
  DCHECK(profile->IsRegularProfile() || profile->IsIncognitoProfile());
  return profile->IsRegularProfile()
             ? profile_metrics::BrowserProfileType::kRegular
             : profile_metrics::BrowserProfileType::kIncognito;
}

}  // namespace

std::ostream& operator<<(std::ostream& out,
                         const BookmarkLaunchAction& launch_action) {
  return out << "BookmarkLaunchAction(location = "
             << static_cast<int>(launch_action.location)
             << ", action_time = " << launch_action.action_time << ")";
}

void RecordBookmarkLaunch(BookmarkLaunchLocation location,
                          profile_metrics::BrowserProfileType profile_type) {
  if (IsBookmarkBarLocation(location)) {
    base::RecordAction(base::UserMetricsAction("ClickedBookmarkBarURLButton"));
  } else if (location == BookmarkLaunchLocation::kAppMenu) {
    base::RecordAction(
        base::UserMetricsAction("WrenchMenu_Bookmarks_LaunchURL"));
  } else if (location == BookmarkLaunchLocation::kTopMenu) {
    base::RecordAction(base::UserMetricsAction("TopMenu_Bookmarks_LaunchURL"));
  }

  UMA_HISTOGRAM_ENUMERATION("Bookmarks.LaunchLocation", location);

  UMA_HISTOGRAM_ENUMERATION("Bookmarks.UsageCountPerProfileType", profile_type);
}

void RecordBookmarkFolderLaunch(BookmarkLaunchLocation location) {
  if (IsBookmarkBarLocation(location)) {
    base::RecordAction(
        base::UserMetricsAction("MiddleClickedBookmarkBarFolder"));
  }
}

void RecordBookmarkFolderOpen(BookmarkLaunchLocation location) {
  if (IsBookmarkBarLocation(location))
    base::RecordAction(base::UserMetricsAction("ClickedBookmarkBarFolder"));
}

void RecordBookmarkAppsPageOpen(BookmarkLaunchLocation location) {
  if (IsBookmarkBarLocation(location)) {
    base::RecordAction(
        base::UserMetricsAction("ClickedBookmarkBarAppsShortcutButton"));
  }
}

void RecordBookmarkEdited(BookmarkLaunchLocation location) {
  UMA_HISTOGRAM_ENUMERATION("Bookmarks.EditLocation", location);
}

void RecordBookmarkRemoved(BookmarkLaunchLocation location) {
  UMA_HISTOGRAM_ENUMERATION("Bookmarks.RemovedLocation", location);
}

void RecordBookmarksAdded(const Profile* profile) {
  profile_metrics::BrowserProfileType profile_type = GetMetricProfile(profile);
  UMA_HISTOGRAM_ENUMERATION("Bookmarks.AddedPerProfileType", profile_type);
}

void RecordBookmarkAllTabsWithTabsCount(const Profile* profile, int count) {
  profile_metrics::BrowserProfileType profile_type = GetMetricProfile(profile);
  if (profile_type == profile_metrics::BrowserProfileType::kRegular) {
    UMA_HISTOGRAM_COUNTS_100("Bookmarks.BookmarkAllTabsWithTabsCount.Regular",
                             count);
  } else {
    UMA_HISTOGRAM_COUNTS_100("Bookmarks.BookmarkAllTabsWithTabsCount.Incognito",
                             count);
  }
}

void RecordBookmarkDropped(const bookmarks::BookmarkNodeData& data,
                           const bookmarks::BookmarkNode* parent_node,
                           bool is_reorder) {
  enum class DropType : int {
    kDropURLOntoBar = 0,
    kDropURLIntoFolder = 1,
    kDropBookmarkOntoBar = 2,
    kDropBookmarkIntoFolder = 3,
    kDropFolderOntoBar = 4,
    kDropFolderIntoFolder = 5,
    kReorderBookmarkOnBar = 6,
    kReorderBookmarkInFolder = 7,
    kReorderFolderOnBar = 8,
    kReorderSubfolderInFolder = 9,
    kMaxValue = kReorderSubfolderInFolder
  };

  // Note that `has_single_url()` is true for individual existing bookmarks as
  // well as raw URLs, so we have to check the ID as well.
  DropType drop_type;
  if (data.has_single_url() && data.elements[0].id() == 0) {
    drop_type = parent_node->is_permanent_node() ? DropType::kDropURLOntoBar
                                                 : DropType::kDropURLIntoFolder;
  } else if (is_reorder) {
    if (data.has_single_url()) {
      drop_type = parent_node->is_permanent_node()
                      ? DropType::kReorderBookmarkOnBar
                      : DropType::kReorderBookmarkInFolder;
    } else {
      drop_type = parent_node->is_permanent_node()
                      ? DropType::kReorderFolderOnBar
                      : DropType::kReorderSubfolderInFolder;
    }
  } else {
    if (data.has_single_url()) {
      drop_type = parent_node->is_permanent_node()
                      ? DropType::kDropBookmarkOntoBar
                      : DropType::kDropBookmarkIntoFolder;
    } else {
      drop_type = parent_node->is_permanent_node()
                      ? DropType::kDropFolderOntoBar
                      : DropType::kDropFolderIntoFolder;
    }
  }
  UMA_HISTOGRAM_ENUMERATION("Bookmarks.BookmarksBar.DragDropType", drop_type);
}
