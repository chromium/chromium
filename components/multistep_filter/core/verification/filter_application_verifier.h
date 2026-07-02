// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MULTISTEP_FILTER_CORE_VERIFICATION_FILTER_APPLICATION_VERIFIER_H_
#define COMPONENTS_MULTISTEP_FILTER_CORE_VERIFICATION_FILTER_APPLICATION_VERIFIER_H_

#include <string>
#include <vector>

#include "components/multistep_filter/core/data_models/filter_annotation.h"
#include "components/multistep_filter/core/data_models/url_filter_suggestion.h"

namespace multistep_filter {

// Centralized verifier that checks whether a Multistep Filter suggestion was
// applied successfully by verifying whether extracted attribute annotations
// match the suggested filters.
class FilterApplicationVerifier {
 public:
  // Holds the structured result of evaluating whether a suggestion was
  // applied successfully.
  struct Result {
    enum class Outcome {
      kSuccess,
      kNoExtractedAnnotations,
      kCountMismatch,
      kAttributeMismatch,
    };
    Outcome outcome;
    std::vector<std::string> missing_keys;

    bool is_success() const { return outcome == Outcome::kSuccess; }
  };

  FilterApplicationVerifier() = delete;

  // Pure domain evaluation: verifies whether the extracted annotation from the
  // new page matches the attributes that were suggested in the original
  // suggestion without side effects or logging.
  static Result Verify(const UrlFilterSuggestion& suggested_filters,
                       const FilterAnnotation& extracted_annotation);
};

}  // namespace multistep_filter

#endif  // COMPONENTS_MULTISTEP_FILTER_CORE_VERIFICATION_FILTER_APPLICATION_VERIFIER_H_
