// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/core/secure_payment_confirmation_metrics.h"

#include "base/metrics/histogram_functions.h"

namespace payments {

void RecordEnrollSystemPromptResult(
    SecurePaymentConfirmationEnrollSystemPromptResult result) {
  // The histogram name must be kept in sync with
  // tools/metrics/histograms/metadata/payment/histograms.xml
  base::UmaHistogramEnumeration(
      "PaymentRequest.SecurePaymentConfirmation.Funnel."
      "EnrollSystemPromptResult",
      result);
}

void RecordBrowserBoundKeyInclusion(
    SecurePaymentConfirmationBrowserBoundKeyInclusionResult result) {
  base::UmaHistogramEnumeration(
      "PaymentRequest.SecurePaymentConfirmation.BrowserBoundKeyInclusion",
      result);
}

void RecordBrowserBoundKeyCreation(
    SecurePaymentConfirmationBrowserBoundKeyDeviceResult result) {
  base::UmaHistogramEnumeration(
      "PaymentRequest.SecurePaymentConfirmation.BrowserBoundKeyStoreCreate",
      result);
}

void RecordBrowserBoundKeyRetrieval(
    SecurePaymentConfirmationBrowserBoundKeyDeviceResult result) {
  base::UmaHistogramEnumeration(
      "PaymentRequest.SecurePaymentConfirmation.BrowserBoundKeyStoreRetrieve",
      result);
}

void RecordBrowserBoundKeyMetadataUpdated(bool success) {
  base::UmaHistogramBoolean(
      "PaymentRequest.SecurePaymentConfirmation.BrowserBoundKeyMetdataUpdate",
      success);
}

}  // namespace payments
