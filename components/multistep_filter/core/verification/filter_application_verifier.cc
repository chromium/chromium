// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/multistep_filter/core/verification/filter_application_verifier.h"

#include <algorithm>
#include <string>
#include <vector>

#include "base/containers/to_vector.h"
#include "base/strings/utf_string_conversions.h"

namespace multistep_filter {

// static
FilterApplicationVerifier::Result FilterApplicationVerifier::Verify(
    const UrlFilterSuggestion& suggested_filters,
    const std::optional<FilterAnnotation>& extracted_annotation) {
  if (!extracted_annotation || extracted_annotation->attributes.empty()) {
    return {
        .outcome = SuggestionApplicationResult::kFailedNoExtractedAnnotations,
        .missing_keys = base::ToVector(suggested_filters.attribute_ui_labels,
                                       &FilterAttributeUiLabel::key)};
  }

  std::vector<std::string> missing_keys;
  for (const FilterAttributeUiLabel& suggested_label :
       suggested_filters.attribute_ui_labels) {
    const std::string expected_value =
        base::UTF16ToUTF8(suggested_label.attribute_value);
    const bool found_match = std::ranges::any_of(
        extracted_annotation->attributes, [&](const FilterAttribute& attr) {
          return attr.key == suggested_label.key &&
                 attr.value == expected_value;
        });
    if (!found_match) {
      missing_keys.push_back(suggested_label.key);
    }
  }

  if (missing_keys.empty()) {
    return {.outcome = SuggestionApplicationResult::kAllFiltersApplied};
  }

  std::sort(missing_keys.begin(), missing_keys.end());
  missing_keys.erase(std::unique(missing_keys.begin(), missing_keys.end()),
                     missing_keys.end());

  return {.outcome = SuggestionApplicationResult::kFailedAttributeMismatch,
          .missing_keys = std::move(missing_keys)};
}
}  // namespace multistep_filter
