// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_PAYMENTS_BNPL_METRICS_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_PAYMENTS_BNPL_METRICS_H_

#include <string_view>

#include "components/autofill/core/browser/payments/payments_window_manager.h"

namespace autofill::autofill_metrics {

using BnplFlowResult = payments::PaymentsWindowManager::BnplFlowResult;

// The reason why a BNPL suggestion was not shown on the page.
//
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class BnplSuggestionNotShownReason {
  // The checkout amount could not be extracted from the page. This value is
  // necessary to determine BNPL eligibility for the purchase.
  kAmountExtractionFailure = 0,

  // The checkout amount extracted from the page is not supported by any of the
  // available BNPL issuers.
  kCheckoutAmountNotSupported = 1,

  kMaxValue = kCheckoutAmountNotSupported,
};

// Enum to track the result of a corresponding BnplTosDialog that was shown.
//
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// LINT.IfChange(BnplTosDialogResult)
enum class BnplTosDialogResult {
  kCancelButtonClicked = 0,
  kAcceptButtonClicked = 1,
  kMaxValue = kAcceptButtonClicked,
};
// LINT.ThenChange(/tools/metrics/histograms/metadata/autofill/enums.xml:BnplTosDialogResult)

// Returns the histogram suffix corresponding to the given issuer_id.
std::string GetHistogramSuffixFromIssuerId(std::string_view issuer_id);

// LINT.IfChange(BnplFormEvent)

enum class BnplFormEvent {
  // Payments autofill suggestions were shown on a BNPL-eligible merchant.
  kSuggestionsShownOnce = 0,

  kMaxValue = kSuggestionsShownOnce,
};

// LINT.ThenChange(/tools/metrics/histograms/metadata/autofill/enums.xml:BnplFormEvent)

// Logs if the buy-now-pay-later preference is changed by the user through the
// pay-over-time toggle in the payment methods settings page. Records true when
// the user switches on buy-now-pay-later. Records false when the user switches
// off buy-now-pay-later.
void LogBnplPrefToggled(bool enabled);

// Logs the number of BNPL issuers synced at startup.
void LogBnplIssuersSyncedCountAtStartup(int count);

// Logs that the BNPL ToS dialog was shown.
void LogBnplTosDialogShown(std::string_view issuer_id);

// Logs that the BNPL ToS dialog closed reason.
void LogBnplTosDialogResult(BnplTosDialogResult result,
                            std::string_view issuer_id);

// Logs that the BNPL suggestion was not shown and the reason why.
void LogBnplSuggestionNotShownReason(BnplSuggestionNotShownReason reason);

// Logs that the BNPL popup window was shown.
void LogBnplPopupWindowShown(std::string_view issuer_id);

// Logs the result of the BNPL popup window.
void LogBnplPopupWindowResult(std::string_view issuer_id,
                              BnplFlowResult result);

// Logs BNPL form events. Please refer to `BnplFormEvent` for the possible
// enumerations that can be logged.
void LogBnplFormEvent(BnplFormEvent event);

}  // namespace autofill::autofill_metrics

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_PAYMENTS_BNPL_METRICS_H_
