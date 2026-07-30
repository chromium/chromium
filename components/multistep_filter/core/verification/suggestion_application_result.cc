// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/multistep_filter/core/verification/suggestion_application_result.h"

#include <string_view>

#include "base/notreached.h"

namespace multistep_filter {

std::string_view SuggestionApplicationResultToString(
    SuggestionApplicationResult result) {
  switch (result) {
    case SuggestionApplicationResult::kAllFiltersApplied:
      return "all_filters_applied";
    case SuggestionApplicationResult::kNotAllFiltersApplied:
      return "not_all_filters_applied";
    case SuggestionApplicationResult::kAbandonedBeforeVerification:
      return "abandoned_before_verification";
    case SuggestionApplicationResult::kFailedErrorPage:
      return "failed_error_page";
    case SuggestionApplicationResult::kFailedNoExtractedAnnotations:
      return "error_no_extracted_annotations";
    case SuggestionApplicationResult::kFailedCountMismatch:
      return "error_filter_count_mismatch";
    case SuggestionApplicationResult::kFailedAttributeMismatch:
      return "error_attribute_mismatch";
  }
}

}  // namespace multistep_filter
