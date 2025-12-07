// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAYMENTS_CORE_SECURE_PAYMENT_CONFIRMATION_METRICS_H_
#define COMPONENTS_PAYMENTS_CORE_SECURE_PAYMENT_CONFIRMATION_METRICS_H_

namespace payments {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused. Keep in sync with
// tools/metrics/histograms/enums.xml.
enum class SecurePaymentConfirmationEnrollSystemPromptResult {
  kCanceled = 0,
  kAccepted = 1,
  kMaxValue = kAccepted,
};

// LINT.IfChange(BrowserBoundKeys)

enum class SecurePaymentConfirmationBrowserBoundKeyDeviceResult {
  kSuccessWithDeviceHardware = 0,
  kSuccessWithoutDeviceHardware = 1,
  kFailureWithDeviceHardware = 2,
  kFailureWithoutDeviceHardware = 3,
  kMaxValue = kFailureWithoutDeviceHardware,
};

enum class SecurePaymentConfirmationBrowserBoundKeyInclusionResult {
  kIncludedNew = 0,
  kIncludedExisting = 1,
  kNotIncludedWithDeviceHardware = 2,
  kNotIncludedWithoutDeviceHardware = 3,
  kMaxValue = kNotIncludedWithoutDeviceHardware,
};

// LINT.ThenChange(//tools/metrics/histograms/metadata/payment/enums.xml:BrowserBoundKeys)

void RecordEnrollSystemPromptResult(
    SecurePaymentConfirmationEnrollSystemPromptResult result);

void RecordBrowserBoundKeyInclusion(
    SecurePaymentConfirmationBrowserBoundKeyInclusionResult result);

void RecordBrowserBoundKeyCreation(
    SecurePaymentConfirmationBrowserBoundKeyDeviceResult result);

void RecordBrowserBoundKeyRetrieval(
    SecurePaymentConfirmationBrowserBoundKeyDeviceResult result);

void RecordBrowserBoundKeyMetadataUpdated(bool success);

// TODO(crbug.com/40171413): Move other SPC metrics into this common file.

}  // namespace payments

#endif  // COMPONENTS_PAYMENTS_CORE_SECURE_PAYMENT_CONFIRMATION_METRICS_H_
