// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_OMNIBOX_LOOKALIKE_URL_NAVIGATION_OBSERVER_H_
#define CHROME_BROWSER_UI_OMNIBOX_LOOKALIKE_URL_NAVIGATION_OBSERVER_H_

#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

namespace content {
class NavigationHandle;
}

class SiteEngagementService;

// Observes navigations and shows an infobar if the navigated domain name
// is visually similar to a top domain or a domain with a site engagement score.
class LookalikeUrlNavigationObserver
    : public content::WebContentsObserver,
      public content::WebContentsUserData<LookalikeUrlNavigationObserver> {
 public:
  // Used for metrics. Multiple events can occur per navigation.
  enum class NavigationSuggestionEvent {
    kNone = 0,
    kInfobarShown = 1,
    kLinkClicked = 2,
    kMatchTopSite = 3,
    kMatchSiteEngagement = 4,

    // Append new items to the end of the list above; do not modify or
    // replace existing values. Comment out obsolete items.
    kMaxValue = kMatchSiteEngagement,
  };

  // Used for UKM. There is only a single MatchType per navigation.
  enum class MatchType {
    kNone = 0,
    kTopSite = 1,
    kSiteEngagement = 2,

    // Append new items to the end of the list above; do not modify or replace
    // existing values. Comment out obsolete items.
    kMaxValue = kSiteEngagement,
  };

  static const char kHistogramName[];

  static void CreateForWebContents(content::WebContents* web_contents);

  explicit LookalikeUrlNavigationObserver(content::WebContents* web_contents);
  ~LookalikeUrlNavigationObserver() override;

  // content::WebContentsObserver:
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;

 private:
  // Returns a site that the user has used before that |url| may be attempting
  // to spoof, based on skeleton comparison.
  std::string GetMatchingSiteEngagementDomain(SiteEngagementService* service,
                                              const GURL& url);
};

#endif  // CHROME_BROWSER_UI_OMNIBOX_LOOKALIKE_URL_NAVIGATION_OBSERVER_H_
