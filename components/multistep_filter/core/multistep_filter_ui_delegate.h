// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MULTISTEP_FILTER_CORE_MULTISTEP_FILTER_UI_DELEGATE_H_
#define COMPONENTS_MULTISTEP_FILTER_CORE_MULTISTEP_FILTER_UI_DELEGATE_H_

#include <optional>

#include "components/multistep_filter/core/data_models/url_filter_suggestion.h"

namespace multistep_filter {

// Interface for interacting with the Multistep Filter UI.
// This delegate allows core services and observers to query the UI state
// (like suppression) and trigger UI updates (like clearing or showing
// suggestions).
//
// Instances are owned indirectly by `ChromeFilterNavigationObserver` in
// chrome/browser; one instance exists per tab (via `tabs::TabFeatures`) but
// is replaced if the tab's WebContents is replaced in the tab.
class MultistepFilterUiDelegate {
 public:
  virtual ~MultistepFilterUiDelegate() = default;

  // Clears any currently displayed suggestions in the UI.
  virtual void ClearSuggestion() = 0;

  // Called when a suggestion is generated (or fails to generate).
  virtual void OnSuggestionGenerated(
      std::optional<UrlFilterSuggestion> suggestion) = 0;
};

}  // namespace multistep_filter

#endif  // COMPONENTS_MULTISTEP_FILTER_CORE_MULTISTEP_FILTER_UI_DELEGATE_H_
