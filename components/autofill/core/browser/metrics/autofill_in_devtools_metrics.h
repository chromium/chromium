// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_AUTOFILL_IN_DEVTOOLS_METRICS_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_AUTOFILL_IN_DEVTOOLS_METRICS_H_

#include <string>

namespace autofill::autofill_metrics {

// These values are persisted to UMA logs and specify which test address
// suggestion an user has selected from the subpopup. Entries should not be
// renumbered and numeric values should never be reused. This list matches
// countries for each available test address provided by devtools frontend. When
// adding more values to this enum, please also update its equivalent in
// tools/metrics/histograms/metadata/autofill/enums.xml
enum class AutofillInDevtoolsAvailableTestAddressesCountries {
  kUnitedStated = 0,
  kBrazil = 1,
  kMexico = 2,
  kJapan = 3,
  kIndia = 4,
  kGermany = 5,
  kMaxValue = kGermany
};

// Devtools test addresses events.
// These events are not based on address form submissions, but on whether a
// devtools test address suggestion was shown (which is conditioned on devtools
// being open) or selected.
// When adding more values to this enum, please also update its equivalent in
// tools/metrics/histograms/metadata/autofill/enums.xml
enum class AutofillInDevtoolsTestAddressesEvents {
  // Emitted when the top level test addresses suggestion is shown.
  kTestAddressesSuggestionShown = 0,
  // Emitted when a specific test address is chosen to fill the form.
  kTestAddressesSuggestionSelected = 1,
  kMaxValue = kTestAddressesSuggestionSelected
};

void OnDevtoolsTestAddressesShown();

// Logged when a test address suggestion is accepted for a given `country`,
void OnDevtoolsTestAddressesAccepted(std::u16string_view country);

}  // namespace autofill::autofill_metrics

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_AUTOFILL_IN_DEVTOOLS_METRICS_H_
