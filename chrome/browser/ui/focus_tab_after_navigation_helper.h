// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_FOCUS_TAB_AFTER_NAVIGATION_HELPER_H_
#define CHROME_BROWSER_UI_FOCUS_TAB_AFTER_NAVIGATION_HELPER_H_

#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

// FocusTabAfterNavigationHelper focuses the tab contents (potentially taking
// focus away from other browser elements like the omnibox) after
// 1) browser-initiated navigations (e.g. after omnibox- or bookmark-initiated
//    navigations)
// 2) navigations that leave NTP (e.g. after an NTP-replacement extension or
//    third-party NTP executes window.location = ...).
class FocusTabAfterNavigationHelper
    : public content::WebContentsObserver,
      public content::WebContentsUserData<FocusTabAfterNavigationHelper> {
 public:
  ~FocusTabAfterNavigationHelper() override;

  // Not copyable nor movable.
  FocusTabAfterNavigationHelper(const FocusTabAfterNavigationHelper&) = delete;
  FocusTabAfterNavigationHelper& operator=(
      const FocusTabAfterNavigationHelper&) = delete;

  // content::WebContentsObserver
  void ReadyToCommitNavigation(content::NavigationHandle* navigation) override;

 private:
  friend class content::WebContentsUserData<FocusTabAfterNavigationHelper>;

  explicit FocusTabAfterNavigationHelper(content::WebContents* contents);

  bool ShouldFocusTabContents(content::NavigationHandle* navigation);
  bool IsNtpURL(const GURL& url);

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

#endif  // CHROME_BROWSER_UI_FOCUS_TAB_AFTER_NAVIGATION_HELPER_H_
