// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/bookmarks/bookmark_stats.h"
#include "chrome/browser/profiles/profile.h"

#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "components/bookmarks/browser/bookmark_model.h"

using bookmarks::BookmarkNode;

namespace {

bool IsBookmarkBarLocation(BookmarkLaunchLocation location) {
  return location == BOOKMARK_LAUNCH_LOCATION_ATTACHED_BAR ||
         location == BOOKMARK_LAUNCH_LOCATION_BAR_SUBFOLDER;
}

auto GetMetricProfile(const Profile* profile) {
  DCHECK(profile);
  DCHECK(profile->IsRegularProfile() || profile->IsIncognitoProfile());
  return profile->IsRegularProfile()
             ? profile_metrics::BrowserProfileType::kRegular
             : profile_metrics::BrowserProfileType::kIncognito;
}

}  // namespace

void RecordBookmarkLaunch(BookmarkLaunchLocation location,
                          profile_metrics::BrowserProfileType profile_type) {
  if (IsBookmarkBarLocation(location)) {
    base::RecordAction(base::UserMetricsAction("ClickedBookmarkBarURLButton"));
  } else if (location == BOOKMARK_LAUNCH_LOCATION_APP_MENU) {
    base::RecordAction(
        base::UserMetricsAction("WrenchMenu_Bookmarks_LaunchURL"));
  } else if (location == BOOKMARK_LAUNCH_LOCATION_TOP_MENU) {
    base::RecordAction(base::UserMetricsAction("TopMenu_Bookmarks_LaunchURL"));
  }

  UMA_HISTOGRAM_ENUMERATION("Bookmarks.LaunchLocation", location,
                            BOOKMARK_LAUNCH_LOCATION_LIMIT);

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
