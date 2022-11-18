// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/metrics/payments/card_unmask_authentication_metrics.h"

#include <string>

#include "base/metrics/histogram_functions.h"

namespace autofill::autofill_metrics {

void LogCvcAuthAttempt(CreditCard::RecordType card_type) {
  std::string card_type_histogram_string;
  switch (card_type) {
    case CreditCard::FULL_SERVER_CARD:
    case CreditCard::MASKED_SERVER_CARD:
      card_type_histogram_string = ".ServerCard";
      break;
    case CreditCard::VIRTUAL_CARD:
      card_type_histogram_string = ".VirtualCard";
      break;
    case CreditCard::LOCAL_CARD:
      // We do not offer CVC auth for local cards.
      NOTREACHED();
      return;
  }
  base::UmaHistogramBoolean(
      "Autofill.CvcAuth" + card_type_histogram_string + ".Attempt", true);
}

}  // namespace autofill::autofill_metrics
