// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_SUGGESTIONS_LIST_METRICS_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_SUGGESTIONS_LIST_METRICS_H_

namespace autofill {
enum class PopupType;

namespace autofill_metrics {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// Keep enum up to date with AutofillSuggestionManageType in
// tools/metrics/histograms/enums.xml.
// Used by LogAutofillSelectedManageEntry().
enum class ManageSuggestionType {
  kOther = 0,
  kPersonalInformation = 1,
  kAddresses = 2,
  kPaymentMethods = 3,
  kMaxValue = kPaymentMethods,
};

// Log the index of the selected Autofill suggestion in the popup.
void LogAutofillSuggestionAcceptedIndex(int index,
                                        autofill::PopupType popup_type,
                                        bool off_the_record);

// Logs that the user selected 'Manage...' settings entry in the popup.
void LogAutofillSelectedManageEntry(autofill::PopupType popup_type);

}  // namespace autofill_metrics
}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_SUGGESTIONS_LIST_METRICS_H_
