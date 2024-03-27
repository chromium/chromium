// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HISTORY_CONTENT_BROWSER_VISITED_LINK_NAVIGATION_THROTTLE_H_
#define COMPONENTS_HISTORY_CONTENT_BROWSER_VISITED_LINK_NAVIGATION_THROTTLE_H_

#include "components/history/core/browser/history_service_observer.h"
#include "content/public/browser/navigation_throttle.h"

namespace content {
class NavigationHandle;
}

namespace history {
class HistoryService;
}

// Navigation throttle responsible for identifying the <origin, salt> pair
// associated with an incoming navigation. The resulting salt is sent in the
// navigation's `commit_params` to be used by the renderer when determining
// whether a link is :visited.
//
// Links can be styled as :visited in any Document, including Documents
// that result from non-tab navigations.
// HistoryTabHelper::ReadyToCommitNavigation() could be used to perform a
// similar function to this throttle, but the HistoryTabHelper does not listen
// for non-tab navigations. As a result, we chose to implement this throttle
// class which will listen for all navigations and attempt to assign a salt
// value.
class VisitedLinkNavigationThrottle : public content::NavigationThrottle,
                                      public history::HistoryServiceObserver {
 public:
  VisitedLinkNavigationThrottle(content::NavigationHandle* navigation_handle,
                                history::HistoryService* history_service);
  ~VisitedLinkNavigationThrottle() override;

  VisitedLinkNavigationThrottle(const VisitedLinkNavigationThrottle&) = delete;
  VisitedLinkNavigationThrottle& operator=(
      const VisitedLinkNavigationThrottle&) = delete;

  // NavigationThrottle overrides
  ThrottleCheckResult WillProcessResponse() override;
  const char* GetNameForLogging() override;

  // HistoryServiceObserver override
  void HistoryServiceBeingDeleted(
      history::HistoryService* history_service) override;

 private:
  raw_ptr<history::HistoryService> history_service_;
};

#endif  // COMPONENTS_HISTORY_CONTENT_BROWSER_VISITED_LINK_NAVIGATION_THROTTLE_H_
