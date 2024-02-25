// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_SUGGESTIONS_LIST_METRICS_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_SUGGESTIONS_LIST_METRICS_H_

namespace autofill {
enum class FillingProduct;

namespace autofill_metrics {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// Keep enum up to date with AutofillSuggestionManageType in
// tools/metrics/histograms/enums.xml.
// Used by LogAutofillSelectedManageEntry().
enum class ManageSuggestionType {
  kOther = 0,
  // kPersonalInformation 1 is deprecated, see b/316345315.
  kAddresses = 2,
  kPaymentMethodsCreditCards = 3,
  kPaymentMethodsIbans = 4,
  kMaxValue = kPaymentMethodsIbans,
};

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

// Log the index of the selected Autofill suggestion in the popup.
void LogAutofillSuggestionAcceptedIndex(int index,
                                        FillingProduct filling_product,
                                        bool off_the_record);

// Logs that the user selected 'Manage...' settings entry in the popup.
void LogAutofillSelectedManageEntry(FillingProduct filling_product);

// Logs the 'Show cards from your Google Account" button events.
void LogAutofillShowCardsFromGoogleAccountButtonEventMetric(
    ShowCardsFromGoogleAccountButtonEvent event);

}  // namespace autofill_metrics
}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_SUGGESTIONS_LIST_METRICS_H_
