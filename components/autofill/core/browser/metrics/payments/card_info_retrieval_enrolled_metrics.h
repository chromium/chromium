// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_PAYMENTS_CARD_INFO_RETRIEVAL_ENROLLED_METRICS_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_PAYMENTS_CARD_INFO_RETRIEVAL_ENROLLED_METRICS_H_

namespace autofill::autofill_metrics {

// Enum for different types of form events. Used for metrics logging.
enum class CardInfoRetrievalEnrolledLoggingEvent {
  // A dropdown with suggestions was shown.
  kSuggestionShown = 0,
  // Same as above, but recoreded only once per page load.
  kSuggestionShownOnce = 1,
  // A suggestion was selected to fill the form.
  kSuggestionSelected = 2,
  // Same as above, but recoreded only once per page load.
  kSuggestionSelectedOnce = 3,
  // A suggestion was used to fill the form.
  kSuggestionFilled = 4,
  // Same as above, but recorded only once per page load.
  kSuggestionFilledOnce = 5,
  // Same as above, but recorded only once per page load.
  kSuggestionWillSubmitOnce = 6,
  // Same as above, but recorded only once per page load.
  kSuggestionSubmittedOnce = 7,
  kMaxValue = kSuggestionSubmittedOnce,
};

// Enum for different types of unmask results. Used for metrics logging.
// These values are used in enums.xml; do not reorder or renumber entries!
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class CardInfoRetrievalEnrolledUnmaskResult {
  // Default value, should never be used in logging.
  kUnknown = 0,
  // Card unmask completed successfully without further authentication steps.
  kRiskBasedUnmasked = 1,
  // Card unmask completed successfully via OTP authentication method.
  kAuthenticationUnmasked = 2,
  // Card unmask failed due to some generic authentication errors.
  kAuthenticationError = 3,
  // Card unmask was aborted due to user cancellation.
  kFlowCancelled = 4,
  // Card unmask failed due to unexpected errors.
  kUnexpectedError = 5,
  kMaxValue = kUnexpectedError,
};

// Log the suggestion event regarding card info retrieval enrolled.
void LogCardInfoRetrievalEnrolledFormEventMetric(
    CardInfoRetrievalEnrolledLoggingEvent event);

// Log the unmask result regarding card info retrieval enrolled.
void LogCardInfoRetrievalEnrolledUnmaskResult(
    CardInfoRetrievalEnrolledUnmaskResult unmask_result);

}  // namespace autofill::autofill_metrics

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_PAYMENTS_CARD_INFO_RETRIEVAL_ENROLLED_METRICS_H_
