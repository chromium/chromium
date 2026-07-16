// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MULTISTEP_FILTER_CONTENT_FILTER_INITIATED_NAVIGATION_MARKER_H_
#define COMPONENTS_MULTISTEP_FILTER_CONTENT_FILTER_INITIATED_NAVIGATION_MARKER_H_

#include <optional>

#include "base/time/time.h"
#include "components/multistep_filter/core/data_models/url_filter_suggestion.h"
#include "content/public/browser/navigation_handle_user_data.h"

namespace content {
class NavigationHandle;
}

namespace multistep_filter {

// Used to mark navigations that were triggered by the user accepting a
// Multistep Filter suggestion. This allows the observer to ignore these
// navigations and avoid generating new suggestions for them, and verify
// matching.
class FilterInitiatedNavigationMarker
    : public content::NavigationHandleUserData<
          FilterInitiatedNavigationMarker> {
 public:
  FilterInitiatedNavigationMarker(const FilterInitiatedNavigationMarker&) =
      delete;
  FilterInitiatedNavigationMarker& operator=(
      const FilterInitiatedNavigationMarker&) = delete;

  ~FilterInitiatedNavigationMarker() override;

  const std::optional<UrlFilterSuggestion>& suggestion() const {
    return suggestion_;
  }

 private:
  explicit FilterInitiatedNavigationMarker(
      content::NavigationHandle& navigation_handle,
      std::optional<UrlFilterSuggestion> suggestion = std::nullopt);

  friend class content::NavigationHandleUserData<
      FilterInitiatedNavigationMarker>;

  std::optional<UrlFilterSuggestion> suggestion_;

  NAVIGATION_HANDLE_USER_DATA_KEY_DECL();
};

}  // namespace multistep_filter

#endif  // COMPONENTS_MULTISTEP_FILTER_CONTENT_FILTER_INITIATED_NAVIGATION_MARKER_H_
