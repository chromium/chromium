// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_PAYMENTS_CARD_UNMASK_AUTHENTICATION_METRICS_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_PAYMENTS_CARD_UNMASK_AUTHENTICATION_METRICS_H_

#include "components/autofill/core/browser/data_model/credit_card.h"

namespace autofill::autofill_metrics {

// Card unmasking CVC authentication-related metrics.
// Logs when a CVC authentication starts.
void LogCvcAuthAttempt(CreditCard::RecordType card_type);

}  // namespace autofill::autofill_metrics

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_PAYMENTS_OFFERS_METRICS_H_
