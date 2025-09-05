// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/suggestions/suggestion_generator.h"

#include "base/containers/span.h"

namespace autofill {

// static
std::vector<SuggestionGenerator::SuggestionData>
SuggestionGenerator::ExtractSuggestionDataForSource(
    base::span<
        const std::pair<SuggestionDataSource, std::vector<SuggestionData>>>
        all_suggestion_data,
    SuggestionDataSource suggestion_data_source) {
  auto it = std::ranges::find(
      all_suggestion_data, suggestion_data_source,
      &std::pair<SuggestionDataSource, std::vector<SuggestionData>>::first);
  if (it != all_suggestion_data.end()) {
    return it->second;
  }
  return std::vector<SuggestionData>();
}

}  // namespace autofill
