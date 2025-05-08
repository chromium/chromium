// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_PAYMENTS_BNPL_METRICS_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_PAYMENTS_BNPL_METRICS_H_

#include <string_view>

#include "components/autofill/core/browser/data_model/payments/bnpl_issuer.h"
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

// The dialog close reason of select BNPL issuer dialog.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// LINT.IfChange(SelectBnplIssuerDialogResult)
enum class SelectBnplIssuerDialogResult {
  kCancelButtonClicked = 0,
  kIssuerSelected = 1,
  kMaxValue = kIssuerSelected,
};
// LINT.ThenChange(/tools/metrics/histograms/metadata/autofill/enums.xml:SelectBnplIssuerDialogResult)

// Returns the histogram suffix corresponding to the given issuer_id.
std::string GetHistogramSuffixFromIssuerId(
    autofill::BnplIssuer::IssuerId issuer_id);

// Converts a BnplFlowResult enum to its string representation.
std::string ConvertBnplFlowResultToString(BnplFlowResult result);

// LINT.IfChange(BnplFormEvent)

// All BNPL Form Events are logged once per page load.
//
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class BnplFormEvent {
  // Payments autofill suggestions were shown on a BNPL-eligible merchant.
  kSuggestionsShown = 0,

  // The BNPL suggestion was added to the payments autofill dropdown and shown
  // to the user.
  kBnplSuggestionShown = 1,

  // A BNPL suggestion was accepted on the current page.
  kBnplSuggestionAccepted = 2,

  // A form was filled with an Affirm VCN.
  kFormFilledWithAffirm = 3,

  // A form was filled with a Zip VCN.
  kFormFilledWithZip = 4,

  // A form was filled with an Afterpay VCN.
  kFormFilledWithAfterpay = 5,

  // A form was submitted with an Affirm VCN.
  kFormSubmittedWithAffirm = 6,

  // A form was submitted with a Zip VCN.
  kFormSubmittedWithZip = 7,

  // A form was submitted with an Afterpay VCN.
  kFormSubmittedWithAfterpay = 8,

  kMaxValue = kFormSubmittedWithAfterpay,
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
void LogBnplTosDialogShown(autofill::BnplIssuer::IssuerId issuer_id);

// Logs that the BNPL ToS dialog closed reason.
void LogBnplTosDialogResult(BnplTosDialogResult result,
                            autofill::BnplIssuer::IssuerId issuer_id);

// Logs the select BNPL issuer dialog result.
void LogSelectBnplIssuerDialogResult(SelectBnplIssuerDialogResult result);

// Logs the selection of BNPL issuer from the select BNPL issuer dialog.
void LogBnplIssuerSelection(autofill::BnplIssuer::IssuerId issuer_id);

// Logs that the BNPL suggestion was not shown and the reason why.
void LogBnplSuggestionNotShownReason(BnplSuggestionNotShownReason reason);

// Logs that the BNPL popup window was shown.
void LogBnplPopupWindowShown(autofill::BnplIssuer::IssuerId issuer_id);

// Logs the result of the BNPL popup window.
void LogBnplPopupWindowResult(autofill::BnplIssuer::IssuerId issuer_id,
                              BnplFlowResult result);

// Logs the duration a user took to go through the BNPL flow inside of the
// pop-up window. Broken down by issuer and result, because each issuer and
// each result should be looked at separately.
void LogBnplPopupWindowLatency(base::TimeDelta duration,
                               autofill::BnplIssuer::IssuerId issuer_id,
                               BnplFlowResult result);

// Logs BNPL form events. Please refer to `BnplFormEvent` for the possible
// enumerations that can be logged.
void LogBnplFormEvent(BnplFormEvent event);

// Logs that a form was filled with the BNPL issuer VCN.
void LogFormFilledWithBnplVcn(autofill::BnplIssuer::IssuerId issuer_id);

// Logs that a form was submitted with the BNPL issuer VCN.
void LogFormSubmittedWithBnplVcn(autofill::BnplIssuer::IssuerId issuer_id);

// Logs that the BNPL issuer selection dialog was shown.
void LogBnplSelectionDialogShown();

}  // namespace autofill::autofill_metrics

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_PAYMENTS_BNPL_METRICS_H_
