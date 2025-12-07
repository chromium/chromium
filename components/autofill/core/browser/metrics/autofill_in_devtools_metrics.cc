// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/metrics/autofill_in_devtools_metrics.h"

#include <string>

#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"

namespace autofill::autofill_metrics {

void OnDevtoolsTestAddressesShown() {
  base::UmaHistogramEnumeration(
      "Autofill.TestAddressesEvent",
      AutofillInDevtoolsTestAddressesEvents::kTestAddressesSuggestionShown);
}

void OnDevtoolsTestAddressesAccepted(const std::u16string_view country) {
  base::UmaHistogramEnumeration(
      "Autofill.TestAddressesEvent",
      AutofillInDevtoolsTestAddressesEvents::kTestAddressesSuggestionSelected);
  if (country == u"United States") {
    base::UmaHistogramEnumeration(
        "Autofill.TestAddressSelected",
        AutofillInDevtoolsAvailableTestAddressesCountries::kUnitedStated);
    return;

  } else if (country == u"Brazil") {
    base::UmaHistogramEnumeration(
        "Autofill.TestAddressSelected",
        AutofillInDevtoolsAvailableTestAddressesCountries::kBrazil);
    return;
  } else if (country == u"Japan") {
    base::UmaHistogramEnumeration(
        "Autofill.TestAddressSelected",
        AutofillInDevtoolsAvailableTestAddressesCountries::kJapan);
    return;

  } else if (country == u"Mexico") {
    base::UmaHistogramEnumeration(
        "Autofill.TestAddressSelected",
        AutofillInDevtoolsAvailableTestAddressesCountries::kMexico);
    return;

  } else if (country == u"India") {
    base::UmaHistogramEnumeration(
        "Autofill.TestAddressSelected",
        AutofillInDevtoolsAvailableTestAddressesCountries::kIndia);
    return;
  } else if (country == u"Germany") {
    base::UmaHistogramEnumeration(
        "Autofill.TestAddressSelected",
        AutofillInDevtoolsAvailableTestAddressesCountries::kGermany);
    return;
  }
  NOTREACHED();
}

}  // namespace autofill::autofill_metrics
