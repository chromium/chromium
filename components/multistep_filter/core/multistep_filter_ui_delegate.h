// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MULTISTEP_FILTER_CORE_MULTISTEP_FILTER_UI_DELEGATE_H_
#define COMPONENTS_MULTISTEP_FILTER_CORE_MULTISTEP_FILTER_UI_DELEGATE_H_

#include <optional>

#include "base/memory/weak_ptr.h"
#include "components/multistep_filter/core/data_models/url_filter_suggestion.h"

class GURL;

namespace multistep_filter {

// Interface for interacting with the Multistep Filter UI.
// This delegate allows core services and observers to query the UI state
// (like suppression) and trigger UI updates (like clearing or showing
// suggestions).
class MultistepFilterUiDelegate {
 public:
  virtual ~MultistepFilterUiDelegate() = default;

  // Clears any currently displayed suggestions in the UI.
  virtual void ClearSuggestion() = 0;

  // Called when a suggestion is generated (or fails to generate).
  virtual void OnSuggestionGenerated(
      std::optional<UrlFilterSuggestion> suggestion) = 0;

  // Returns true if suggestions should be suppressed for the given `url`.
  virtual bool ShouldSuppressSuggestions(const GURL& url) const = 0;

  virtual base::WeakPtr<MultistepFilterUiDelegate> GetWeakPtr() = 0;
};

}  // namespace multistep_filter

#endif  // COMPONENTS_MULTISTEP_FILTER_CORE_MULTISTEP_FILTER_UI_DELEGATE_H_
