// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MULTISTEP_FILTER_CORE_MULTISTEP_FILTER_UI_DELEGATE_H_
#define COMPONENTS_MULTISTEP_FILTER_CORE_MULTISTEP_FILTER_UI_DELEGATE_H_

#include <optional>

#include "base/functional/callback.h"
#include "components/multistep_filter/core/data_models/suggestion_user_decision.h"
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
  struct SuggestionUiCallbacks {
    // Called the first time a suggestion UI is shown after page load.
    base::OnceClosure on_suggestion_shown;
    // Called the first time a suggestion UI is shown after being dismissed or
    // timing out before.
    // Note: This callback is only executed if the on_suggestion_shown callback
    // was executed before.
    base::OnceClosure on_suggestion_reopened;
    // Called with the final interaction by the user (e.g., accepting,
    // dismissing, or opening settings) or when the suggestion is ignored.
    // A suggestion is considered ignored if it was shown but subsequently
    // cleared due to a new navigation, tab closure, or being replaced by a
    // new suggestion.
    // Note: This callback is only executed if the on_suggestion_shown callback
    // was executed before.
    base::OnceCallback<void(SuggestionUserDecision)> on_user_interaction;
  };

  virtual ~MultistepFilterUiDelegate() = default;

  // Clears any currently displayed suggestions in the UI.
  virtual void ClearSuggestion() = 0;

  // Called when a suggestion is generated (or fails to generate).
  // The `callbacks` will be used by the UI to report user interactions.
  // TODO (crbug.com/532968622): Rename this method to ShowSuggestions.
  virtual void OnSuggestionGenerated(
      std::optional<UrlFilterSuggestion> suggestion,
      SuggestionUiCallbacks callbacks) = 0;
};

}  // namespace multistep_filter

#endif  // COMPONENTS_MULTISTEP_FILTER_CORE_MULTISTEP_FILTER_UI_DELEGATE_H_
