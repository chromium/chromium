// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAGE_LOAD_METRICS_BROWSER_NAVIGATION_HANDLE_USER_DATA_H_
#define COMPONENTS_PAGE_LOAD_METRICS_BROWSER_NAVIGATION_HANDLE_USER_DATA_H_

#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/navigation_handle_user_data.h"

namespace page_load_metrics {

// Stores information about the location where a navigation was initiated from.
//
// Timing of availability:
// This user data is attached to a `content::NavigationHandle`. While some
// navigations attach it during the handle's creation (e.g., via a callback),
// other navigations (such as those initiated from the Omnibox) attach it
// immediately after the navigation is initiated. Consequently, this user data
// is NOT guaranteed to be present during `PageLoadMetricsObserver::OnStart()`.
//
// Instead, `PageLoadMetricsObserver::OnCommit()` (or
// `DidActivatePrerenderedPage()` for prerender activation) is a reliable time
// to retrieve this data, because:
// 1. By the time of commit, any post-initiation attachment code has already
//    run.
// 2. The `NavigationHandle` (and therefore this user data) is still alive.
// Note that once the navigation has finished committing, the
// `NavigationHandle` is destroyed, making the user data no longer accessible.
class NavigationHandleUserData
    : public content::NavigationHandleUserData<NavigationHandleUserData> {
 public:
  // The enum is used for identifying the source of a navigation.
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  // LINT.IfChange(NavigationInitiatorType)
  enum class InitiatorLocation {
    kOther = 0,
    kBookmarkBar = 1,
    kNewTabPage = 2,
    kOmniboxDirectUrlInput = 3,
    kOmniboxDefaultSearchEngine = 4,
    kMaxValue = kOmniboxDefaultSearchEngine
  };
  // LINT.ThenChange(//tools/metrics/histograms/metadata/navigation/enums.xml:NavigationInitiatorType)

  ~NavigationHandleUserData() override = default;

  InitiatorLocation navigation_type() const { return navigation_type_; }

  static void AttachNewTabPageNavigationHandleUserData(
      content::NavigationHandle& navigation_handle);

  static void AttachOmniboxDirectUrlInputNavigationHandleUserData(
      content::NavigationHandle& navigation_handle);

  static void AttachOmniboxDefaultSearchEngineNavigationHandleUserData(
      content::NavigationHandle& navigation_handle);

 private:
  NavigationHandleUserData(content::NavigationHandle& navigation,
                           InitiatorLocation navigation_type);

  // `navigation_type` is used to store where this navigation is initiated from.
  // This information is used to identify the source of the navigation, and this
  // kind of information is utilized by PageLoadMetricsObservers.
  const InitiatorLocation navigation_type_;

  friend content::NavigationHandleUserData<NavigationHandleUserData>;
  NAVIGATION_HANDLE_USER_DATA_KEY_DECL();
};

}  // namespace page_load_metrics

#endif  // COMPONENTS_PAGE_LOAD_METRICS_BROWSER_NAVIGATION_HANDLE_USER_DATA_H_
