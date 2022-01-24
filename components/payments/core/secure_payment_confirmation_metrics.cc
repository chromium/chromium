// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/core/secure_payment_confirmation_metrics.h"

#include "base/metrics/histogram_functions.h"

namespace payments {

void RecordEnrollDialogShown(SecurePaymentConfirmationEnrollDialogShown shown) {
  // The histogram name must be kept in sync with
  // tools/metrics/histograms/metadata/payment/histograms.xml
  base::UmaHistogramEnumeration(
      "PaymentRequest.SecurePaymentConfirmation.Funnel."
      "EnrollDialogShown",
      shown);
}

void RecordEnrollDialogResult(
    SecurePaymentConfirmationEnrollDialogResult result) {
  // The histogram name must be kept in sync with
  // tools/metrics/histograms/metadata/payment/histograms.xml
  base::UmaHistogramEnumeration(
      "PaymentRequest.SecurePaymentConfirmation.Funnel."
      "EnrollDialogResult",
      result);
}

void RecordEnrollSystemPromptResult(
    SecurePaymentConfirmationEnrollSystemPromptResult result) {
  // The histogram name must be kept in sync with
  // tools/metrics/histograms/metadata/payment/histograms.xml
  base::UmaHistogramEnumeration(
      "PaymentRequest.SecurePaymentConfirmation.Funnel."
      "EnrollSystemPromptResult",
      result);
}

}  // namespace payments
