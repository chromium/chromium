// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_STATS_TAB_HELPER_H_
#define CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_STATS_TAB_HELPER_H_

#include "chrome/browser/ui/bookmarks/bookmark_stats.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "ui/base/window_open_disposition.h"

// A tab helper responsible for reporting metrics on various bookmarks user
// journeys.
class BookmarkStatsTabHelper
    : public content::WebContentsObserver,
      public content::WebContentsUserData<BookmarkStatsTabHelper> {
 public:
  BookmarkStatsTabHelper();
  BookmarkStatsTabHelper(const BookmarkStatsTabHelper&) = delete;
  BookmarkStatsTabHelper& operator=(const BookmarkStatsTabHelper&) = delete;
  ~BookmarkStatsTabHelper() override;

  // The launch action should be set immediately after a given tab has been
  // navigated following a bookmark launch action. The launch action will only
  // be set if there is a pending navigation for the current tab.
  // `tab_disposition` is the disposition requested for by this tab for the
  // navigation.
  bool SetLaunchAction(const BookmarkLaunchAction& launch_action,
                       WindowOpenDisposition tab_disposition);

  // content::WebContentsObserver:
  void DidStartNavigation(
      content::NavigationHandle* navigation_handle) override;
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;
  void DidFirstVisuallyNonEmptyPaint() override;

  const std::optional<BookmarkLaunchAction>& launch_action_for_testing() const {
    return launch_action_;
  }
  const std::optional<WindowOpenDisposition>& tab_disposition_for_testing()
      const {
    return tab_disposition_;
  }

 private:
  friend class content::WebContentsUserData<BookmarkStatsTabHelper>;

  explicit BookmarkStatsTabHelper(content::WebContents* web_contents);

  // The launch action and requested tab disposition for the current navigation.
  // These will be reset after the following navigation begins.
  std::optional<BookmarkLaunchAction> launch_action_;
  std::optional<WindowOpenDisposition> tab_disposition_;

  // Tracks whether launch data should be reset as a new primary frame
  // navigation begins.
  bool should_reset_launch_data_ = false;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

#endif  // CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_STATS_TAB_HELPER_H_
