// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/multistep_filter/core/data_models/url_filter_suggestion.h"

#include <utility>

#include "components/multistep_filter/core/data_models/filter_suggestion_candidate.h"

namespace multistep_filter {

UrlFilterSuggestion::UrlFilterSuggestion(std::string text, GURL url)
    : text_(std::move(text)), url_(std::move(url)) {}

UrlFilterSuggestion::UrlFilterSuggestion(FilterSuggestionCandidate candidate)
    : url_(std::move(candidate.navigation_url)) {
  // TODO(crbug.com/491202866): Change it with the UX approved text.
  text_ = "Recall info from previous tabs?";
}

}  // namespace multistep_filter
