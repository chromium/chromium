// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAYMENTS_CORE_PAYMENT_REQUEST_METRICS_H_
#define COMPONENTS_PAYMENTS_CORE_PAYMENT_REQUEST_METRICS_H_

#include <string>

class PrefService;

namespace payments {

// Sources that may set the payments.can_make_payment_enabled preference.
//
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class CanMakePaymentPreferenceSetter {
  // The pref was set from an unknown source, e.g. via the command line.
  kUnknown = 0,
  // The pref was set by the user.
  kUserSetting = 1,
  // ChromeOS only. The pref is set by a standalone browser (lacros).
  kStandaloneBrowser = 2,
  // The pref was set by an extension.
  kExtension = 3,
  // The pref was set by the custodian of the (supervised) user.
  kCustodian = 4,
  // The pref was set by an admin policy.
  kAdminPolicy = 5,
  kMaxValue = kAdminPolicy
};

// Outcomes of a payment request.
//
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// LINT.IfChange(PaymentRequestOutcome)
enum class PaymentRequestOutcome {
  kSuccess = 0,
  kAbortedByUser = 1,
  kAbortedByMerchant = 2,
  kAbortedInvalidDataFromRenderer = 3,
  kAbortedMojoConnectionError = 4,
  kAbortedMojoRendererClosing = 5,
  kAbortedInstrumentDetailsError = 6,
  kAbortedNoMatchingPaymentMethod = 7,
  kAbortedNoSupportedPaymentMethod = 8,
  kAbortedOther = 9,
  kAbortedUserNavigation = 10,
  kAbortedMerchantNavigation = 11,
  kAbortedUserOptedOut = 12,
  kAbortedInternalError = 13,
  kNotShownAlreadyShowing = 14,
  kNotShownUserActivationRequired = 15,
  kNotShownBackgroundTab = 16,
  kNotShownNoSupportedPaymentMethod = 17,
  kMaxValue = kNotShownNoSupportedPaymentMethod
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/payment/enums.xml:PaymentRequestOutcome)

// Records metrics for the 'payments.can_make_payment_enabled' user pref.
void RecordCanMakePaymentPrefMetrics(const PrefService& pref_service,
                                     const std::string& suffix);

}  // namespace payments

#endif  // COMPONENTS_PAYMENTS_CORE_PAYMENT_REQUEST_METRICS_H_
