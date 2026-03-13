// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MULTISTEP_FILTER_CONTENT_FILTER_INITIATED_NAVIGATION_MARKER_H_
#define COMPONENTS_MULTISTEP_FILTER_CONTENT_FILTER_INITIATED_NAVIGATION_MARKER_H_

#include "content/public/browser/navigation_handle_user_data.h"

namespace content {
class NavigationHandle;
}

namespace multistep_filter {

// Used to mark navigations that were triggered by the user accepting a
// Multistep Filter suggestion. This allows the observer to ignore these
// navigations and avoid generating new suggestions for them.
class FilterInitiatedNavigationMarker
    : public content::NavigationHandleUserData<
          FilterInitiatedNavigationMarker> {
 public:
  FilterInitiatedNavigationMarker(const FilterInitiatedNavigationMarker&) =
      delete;
  FilterInitiatedNavigationMarker& operator=(
      const FilterInitiatedNavigationMarker&) = delete;

  ~FilterInitiatedNavigationMarker() override = default;

 private:
  explicit FilterInitiatedNavigationMarker(
      content::NavigationHandle& navigation_handle) {}

  friend class content::NavigationHandleUserData<
      FilterInitiatedNavigationMarker>;

  NAVIGATION_HANDLE_USER_DATA_KEY_DECL();
};

}  // namespace multistep_filter

#endif  // COMPONENTS_MULTISTEP_FILTER_CONTENT_FILTER_INITIATED_NAVIGATION_MARKER_H_
