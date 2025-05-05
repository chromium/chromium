// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/suggestions/suggestion_generator.h"

namespace autofill {

std::vector<SuggestionGenerator::SuggestionData>
SuggestionGenerator::GetSuggestionDataForFillingProduct(
    const std::vector<std::pair<FillingProduct, std::vector<SuggestionData>>>&
        suggestion_data,
    FillingProduct filling_product) {
  auto it = std::ranges::find(
      suggestion_data, filling_product,
      &std::pair<FillingProduct, std::vector<SuggestionData>>::first);
  if (it != suggestion_data.end()) {
    return it->second;
  }
  return std::vector<SuggestionData>();
}

}  // namespace autofill
