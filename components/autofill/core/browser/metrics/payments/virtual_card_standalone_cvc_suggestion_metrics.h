// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_PAYMENTS_VIRTUAL_CARD_STANDALONE_CVC_SUGGESTION_METRICS_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_PAYMENTS_VIRTUAL_CARD_STANDALONE_CVC_SUGGESTION_METRICS_H_

#include "components/autofill/core/browser/metrics/autofill_metrics.h"

namespace autofill::autofill_metrics {

// Enum for different types of virtual card standalone CVC suggestion form
// events. Used for metrics logging.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class VirtualCardStandaloneCvcSuggestionFormEvent {
  // Standalone CVC suggestions were shown for virtual cards.
  kStandaloneCvcSuggestionShown = 0,
  // Standalone CVC suggestions were shown for a virtual card. Logged once
  // per page load.
  kStandaloneCvcSuggestionShownOnce = 1,
  // A standalone CVC suggestion was selected for a virtual card.
  kStandaloneCvcSuggestionSelected = 2,
  // A standalone CVC suggestion was selected for a virtual card. Logged once
  // per page load.
  kStandaloneCvcSuggestionSelectedOnce = 3,
  // A standalone CVC suggestion for a virtual card was filled into the form.
  kStandaloneCvcSuggestionFilled = 4,
  // A standalone CVC suggestion for a virtual card was filled into the form.
  // Logged once per page load.
  kStandaloneCvcSuggestionFilledOnce = 5,
  // Form was about to be submitted, after being filled with a standalone
  // CVC suggestion for a virtual card.
  kStandaloneCvcSuggestionWillSubmitOnce = 6,
  // Form was submitted, after being filled with a standalone CVC suggestion
  // for a virtual card.
  kStandaloneCvcSuggestionSubmittedOnce = 7,
  kMaxValue = kStandaloneCvcSuggestionSubmittedOnce,
};

// Log standalone CVC suggestion events for virtual cards.
void LogVirtualCardStandaloneCvcSuggestionFormEventMetric(
    VirtualCardStandaloneCvcSuggestionFormEvent event);

}  // namespace autofill::autofill_metrics

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_PAYMENTS_VIRTUAL_CARD_STANDALONE_CVC_SUGGESTION_METRICS_H_
