// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_PAYMENTS_CARD_UNMASK_AUTHENTICATION_METRICS_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_PAYMENTS_CARD_UNMASK_AUTHENTICATION_METRICS_H_

#include "components/autofill/core/browser/data_model/credit_card.h"

namespace autofill::autofill_metrics {

// Card unmasking CVC authentication-related metrics.
// CVC authentication-related events.
enum class CvcAuthEvent {
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.

  // Unknown result. Should not happen.
  kUnknown = 0,
  // The CVC auth succeeded.
  kSuccess = 1,
  // The CVC auth failed because the flow was cancelled.
  kFlowCancelled = 2,
  // The CVC auth failed because the UnmaskCard request failed due to
  // authentication errors.
  kUnmaskCardAuthError = 3,
  // The CVC auth failed because the UnmaskCard request failed due to virtual
  // card retrieval errors.
  kUnmaskCardVirtualCardRetrievalError = 4,
  // The flow failed for technical reasons, such as closing the page or lack of
  // network connection.
  kGenericError = 5,
  // The CVC auth failed temporarily because the CVC didn't match the
  // expected value. This is a retryable error.
  kTemporaryErrorCvcMismatch = 6,
  // The CVC auth failed temporarily because the card used was expired. This is
  // a retryable error.
  kTemporaryErrorExpiredCard = 7,
  kMaxValue = kTemporaryErrorExpiredCard
};

// Logs when a CVC authentication starts.
void LogCvcAuthAttempt(CreditCard::RecordType card_type);

// Logs when a CVC authentication finishes.
void LogCvcAuthResult(CreditCard::RecordType card_type, CvcAuthEvent event);

// Logs when a retryable error occurs in the CVC authentication flow.
void LogCvcAuthRetryableError(CreditCard::RecordType card_type,
                              CvcAuthEvent event);

}  // namespace autofill::autofill_metrics

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_PAYMENTS_OFFERS_METRICS_H_
