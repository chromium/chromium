// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_SUGGESTIONS_LIST_METRICS_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_SUGGESTIONS_LIST_METRICS_H_

#include <cstddef>

namespace autofill {
enum class FillingProduct;

namespace autofill_metrics {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// Used by LogAutofillShowCardsFromGoogleAccountButtonEventMetric().
enum class ShowCardsFromGoogleAccountButtonEvent {
  // 'Show Cards from Google Account' button appeared.
  kButtonAppeared = 0,
  // 'Show Cards from Google Account' button appeared. Logged once per page
  // load.
  kButtonAppearedOnce = 1,
  // 'Show Cards from Google Account' button clicked.
  kButtonClicked = 2,
  kMaxValue = kButtonClicked,
};

// Log the number of Autofill suggestions for the given
// `filling_product`presented to the user when displaying the autofill popup.
void LogSuggestionsCount(size_t num_suggestions,
                         FillingProduct filling_product);

// Log the index of the selected Autofill suggestion in the popup.
void LogSuggestionAcceptedIndex(int index,
                                FillingProduct filling_product,
                                bool off_the_record);

// Logs the 'Show cards from your Google Account" button events.
void LogAutofillShowCardsFromGoogleAccountButtonEventMetric(
    ShowCardsFromGoogleAccountButtonEvent event);

}  // namespace autofill_metrics
}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_SUGGESTIONS_LIST_METRICS_H_
