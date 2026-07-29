// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MULTISTEP_FILTER_CORE_VERIFICATION_SUGGESTION_APPLICATION_RESULT_H_
#define COMPONENTS_MULTISTEP_FILTER_CORE_VERIFICATION_SUGGESTION_APPLICATION_RESULT_H_

#include <string_view>

namespace multistep_filter {

// Represents the outcome of attempting to apply a Multistep Filter suggestion
// to a page after navigation.
//
// These values are persisted to log metrics. Entries should not be renumbered
// and numeric values should never be reused.
//
// LINT.IfChange(SuggestionApplicationResult)
enum class SuggestionApplicationResult {
  // All suggested filters were successfully verified on the landing page.
  kAllFiltersApplied = 0,
  // Kept for backward compatibility but replaced the by more fine-grained
  // failure states below. Represents any failure case that wasn't
  // abandoned.
  kNotAllFiltersApplied = 1,
  // The user navigated away, closed the tab, or accepted a new suggestion
  // before the suggestion application could be verified.
  kAbandonedBeforeVerification = 2,
  // The navigation post suggestion application failed or was to an unsupported
  // scheme.
  kFailedErrorPage = 3,
  // Failed to extract any annotations from the landing page, or the extracted
  // annotation contained no attributes. This takes precedence over count
  // mismatch (i.e., if 0 annotations are found, this is returned even if we
  // expected > 0).
  kFailedNoExtractedAnnotations = 4,
  // The number of applied filters did not match the suggested count. Only
  // reported if at least one annotation was found (otherwise
  // `kFailedNoExtractedAnnotations` is returned).
  kFailedCountMismatch = 5,
  // The applied filters did not match the suggested filter keys/values.
  kFailedAttributeMismatch = 6,
  kMaxValue = kFailedAttributeMismatch,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/multistep_filter/enums.xml:SuggestionApplicationResult)

std::string_view SuggestionApplicationResultToString(
    SuggestionApplicationResult result);

}  // namespace multistep_filter

#endif  // COMPONENTS_MULTISTEP_FILTER_CORE_VERIFICATION_SUGGESTION_APPLICATION_RESULT_H_
