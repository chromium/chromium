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

// Log the suggestion event regarding card info retrieval enrolled.
void LogCardInfoRetrievalEnrolledFormEventMetric(
    CardInfoRetrievalEnrolledLoggingEvent event);

}  // namespace autofill::autofill_metrics

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_PAYMENTS_CARD_INFO_RETRIEVAL_ENROLLED_METRICS_H_
