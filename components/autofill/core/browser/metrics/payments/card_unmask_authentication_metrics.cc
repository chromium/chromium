// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/metrics/payments/card_unmask_authentication_metrics.h"

#include <string>

#include "base/metrics/histogram_functions.h"
#include "components/autofill/core/browser/metrics/autofill_metrics.h"

namespace autofill::autofill_metrics {

void LogCvcAuthAttempt(CreditCard::RecordType card_type) {
  std::string card_type_histogram_string =
      AutofillMetrics::GetHistogramStringForCardType(card_type);
  base::UmaHistogramBoolean(
      "Autofill.CvcAuth" + card_type_histogram_string + ".Attempt", true);
}

void LogCvcAuthResult(CreditCard::RecordType card_type, CvcAuthEvent event) {
  std::string card_type_histogram_string =
      AutofillMetrics::GetHistogramStringForCardType(card_type);
  base::UmaHistogramEnumeration(
      "Autofill.CvcAuth" + card_type_histogram_string + ".Result", event);
}

void LogCvcAuthRetryableError(CreditCard::RecordType card_type,
                              CvcAuthEvent event) {
  std::string card_type_histogram_string =
      AutofillMetrics::GetHistogramStringForCardType(card_type);
  base::UmaHistogramEnumeration(
      "Autofill.CvcAuth" + card_type_histogram_string + ".RetryableError",
      event);
}

}  // namespace autofill::autofill_metrics
