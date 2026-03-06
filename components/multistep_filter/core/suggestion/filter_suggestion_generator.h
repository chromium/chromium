// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MULTISTEP_FILTER_CORE_SUGGESTION_FILTER_SUGGESTION_GENERATOR_H_
#define COMPONENTS_MULTISTEP_FILTER_CORE_SUGGESTION_FILTER_SUGGESTION_GENERATOR_H_

#include <optional>

#include "base/functional/callback.h"
#include "components/multistep_filter/core/data_models/url_filter_suggestion.h"
#include "url/gurl.h"

namespace multistep_filter {

// Generates filter suggestions for a given URL.
//
// Analyzes a URL to determine if a filter suggestion should be displayed.
//
// This class is owned by MultistepFilterService and shares its lifecycle.
class FilterSuggestionGenerator {
 public:
  FilterSuggestionGenerator();
  virtual ~FilterSuggestionGenerator();

  FilterSuggestionGenerator(const FilterSuggestionGenerator&) = delete;
  FilterSuggestionGenerator& operator=(const FilterSuggestionGenerator&) =
      delete;

  // Evaluates `url` to determine if a filter suggestion is applicable.
  //
  // Invokes `callback` with a suggestion if one is generated, or with
  // std::nullopt if no suggestion is applicable.
  virtual void GenerateSuggestion(
      const GURL& url,
      base::OnceCallback<void(std::optional<UrlFilterSuggestion>)> callback);
};

}  // namespace multistep_filter

#endif  // COMPONENTS_MULTISTEP_FILTER_CORE_SUGGESTION_FILTER_SUGGESTION_GENERATOR_H_
