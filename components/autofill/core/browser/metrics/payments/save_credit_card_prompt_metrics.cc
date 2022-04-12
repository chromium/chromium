// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/metrics/payments/save_credit_card_prompt_metrics.h"

#include "base/metrics/histogram_functions.h"

namespace autofill {

void LogSaveCreditCardPromptResult(
    SaveCreditCardPromptResult event,
    bool is_upload,
    AutofillClient::SaveCreditCardOptions options) {
  if (!is_upload) {
    base::UmaHistogramEnumeration("Autofill.CreditCardSaveFlowResult.Local",
                                  event);
    return;
  }
  base::UmaHistogramEnumeration("Autofill.CreditCardSaveFlowResult.Server",
                                event);
  if (options.should_request_expiration_date_from_user) {
    base::UmaHistogramEnumeration(
        "Autofill.CreditCardSaveFlowResult.Server.RequestingExpirationDate",
        event);
  }
  if (options.should_request_name_from_user) {
    base::UmaHistogramEnumeration(
        "Autofill.CreditCardSaveFlowResult.Server.RequestingCardholderName",
        event);
  }
}
}  // namespace autofill
