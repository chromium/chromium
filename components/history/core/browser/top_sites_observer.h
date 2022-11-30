// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HISTORY_CORE_BROWSER_TOP_SITES_OBSERVER_H_
#define COMPONENTS_HISTORY_CORE_BROWSER_TOP_SITES_OBSERVER_H_

namespace history {

class TopSites;

// Interface for observing notifications from TopSites.
class TopSitesObserver {
 public:
  // An enum representing different for TopSitesChanged to happen.
  enum class ChangeReason {
    // TopSites was changed by most visited.
    MOST_VISITED,
    // The set of blocked urls has changed.
    BLOCKED_URLS,
    // TopSites was changed by AddForcedURLs.
    FORCED_URL,
  };

  TopSitesObserver() {}

  TopSitesObserver(const TopSitesObserver&) = delete;
  TopSitesObserver& operator=(const TopSitesObserver&) = delete;

  virtual ~TopSitesObserver() {}

  // Is called when TopSites finishes loading.
  virtual void TopSitesLoaded(TopSites* top_sites) = 0;

  // Is called when either one of the most visited urls
  // changed, or one of the images changes.
  virtual void TopSitesChanged(TopSites* top_sites,
                               ChangeReason change_reason) = 0;
};

}  // namespace history

#endif  // COMPONENTS_HISTORY_CORE_BROWSER_TOP_SITES_OBSERVER_H_
