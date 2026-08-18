// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_PAYMENTS_CHURNED_USERS_METRICS_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_PAYMENTS_CHURNED_USERS_METRICS_H_

#include "components/autofill/core/browser/ui/payments/payments_ui_closed_reasons.h"

namespace autofill::autofill_metrics {

// Logs the result of the payments churned users bubble.
void LogPaymentsChurnedUsersBubbleResult(PaymentsUiClosedReason closed_reason);

// Represents the reason why the payments churned users bubble was not shown,
// or if it was shown successfully.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class PaymentsChurnedUsersBubbleShowResult {
  kShown = 0,
  kOffTheRecord = 1,
  kNoCachedForm = 2,
  kStrikeDatabaseBlocked = 3,
  kNoVisibleCreditCardForm = 4,
  kPrefAlreadyTurnedOn = 5,
  kPrefNotUserControlled = 6,
  kNoAccountInfoPresent = 7,
  kMaxValue = kNoAccountInfoPresent,
};

// Logs the result of attempting to show the payments churned users bubble.
void LogPaymentsChurnedUsersBubbleShowResult(
    PaymentsChurnedUsersBubbleShowResult result);

}  // namespace autofill::autofill_metrics

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_PAYMENTS_CHURNED_USERS_METRICS_H_
