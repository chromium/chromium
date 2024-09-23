// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAGE_LOAD_METRICS_BROWSER_NAVIGATION_HANDLE_USER_DATA_H_
#define COMPONENTS_PAGE_LOAD_METRICS_BROWSER_NAVIGATION_HANDLE_USER_DATA_H_

#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/navigation_handle_user_data.h"

namespace page_load_metrics {

// Store the information about the from where source of a navigation is
// initiated.
class NavigationHandleUserData
    : public content::NavigationHandleUserData<NavigationHandleUserData> {
 public:
  // The enum is used for identifying the source of a navigation.
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class InitiatorLocation {
    kOther = 0,
    kBookmarkBar = 1,
    kNewTabPage = 2,
    kMaxValue = kNewTabPage
  };

  ~NavigationHandleUserData() override = default;

  InitiatorLocation navigation_type() const { return navigation_type_; }

  static void AttachNewTabPageNavigationHandleUserData(
      content::NavigationHandle& navigation_handle);

 private:
  NavigationHandleUserData(content::NavigationHandle& navigation,
                           InitiatorLocation navigation_type)
      : navigation_type_(navigation_type) {}

  // `navigation_type` is used to store where this navigation is initiated from.
  // This information is used to identify the source of the navigation, and this
  // kind of information is utilized by PageLoadMetricsObservers.
  const InitiatorLocation navigation_type_;

  friend content::NavigationHandleUserData<NavigationHandleUserData>;
  NAVIGATION_HANDLE_USER_DATA_KEY_DECL();
};

}  // namespace page_load_metrics

#endif  // COMPONENTS_PAGE_LOAD_METRICS_BROWSER_NAVIGATION_HANDLE_USER_DATA_H_
