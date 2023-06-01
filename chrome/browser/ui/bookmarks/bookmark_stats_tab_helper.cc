// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/bookmarks/bookmark_stats_tab_helper.h"

#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"
#include "content/public/browser/navigation_handle.h"

BookmarkStatsTabHelper::~BookmarkStatsTabHelper() = default;

bool BookmarkStatsTabHelper::SetLaunchAction(
    const BookmarkLaunchAction& launch_action,
    WindowOpenDisposition tab_disposition) {
  if (web_contents()->GetController().GetPendingEntry()) {
    launch_action_ = launch_action;
    tab_disposition_ = tab_disposition;
    // The launch data should remain valid through to the end of the current
    // navigation and the begging of the next navigation.
    should_reset_launch_data_ = false;
    return true;
  }
  return false;
}

void BookmarkStatsTabHelper::DidStartNavigation(
    content::NavigationHandle* navigation_handle) {
  // Reset the launch action data at the beginning of the next navigation.
  if (navigation_handle->IsInPrimaryMainFrame() && should_reset_launch_data_) {
    tab_disposition_.reset();
    launch_action_.reset();
    should_reset_launch_data_ = false;
  }
}

void BookmarkStatsTabHelper::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (navigation_handle->IsInPrimaryMainFrame()) {
    should_reset_launch_data_ = true;
  }
}

void BookmarkStatsTabHelper::DidFirstVisuallyNonEmptyPaint() {
  if (launch_action_ &&
      tab_disposition_ == WindowOpenDisposition::CURRENT_TAB &&
      launch_action_->location == BookmarkLaunchLocation::kAttachedBar) {
    base::UmaHistogramTimes(
        "Bookmarks.AttachedBar.CurrentTab.TimeToFirstVisuallyNonEmptyPaint",
        base::TimeTicks::Now() - launch_action_->action_time);
  }
}

BookmarkStatsTabHelper::BookmarkStatsTabHelper(
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      content::WebContentsUserData<BookmarkStatsTabHelper>(*web_contents) {}

WEB_CONTENTS_USER_DATA_KEY_IMPL(BookmarkStatsTabHelper);
