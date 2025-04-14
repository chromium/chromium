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

// Enum for all supported BNPL issuers.
//
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// LINT.IfChange(SupportedBnplIssuer)
enum class SupportedBnplIssuer {
  kAffirm = 0,
  kAfterpay = 1,
  kZip = 2,
  kMaxValue = kZip,
};
// LINT.ThenChange(/tools/metrics/histograms/metadata/autofill/enums.xml:SupportedBnplIssuer)

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

// Returns the enum for logging corresponding to the given issuer_id.
SupportedBnplIssuer GetEnumForIssuerId(std::string_view issuer_id);

// Returns the histogram suffix corresponding to the given issuer_id.
std::string GetHistogramSuffixFromIssuerId(std::string_view issuer_id);

// Converts a BnplFlowResult enum to its string representation.
std::string ConvertBnplFlowResultToString(BnplFlowResult result);

// LINT.IfChange(BnplFormEvent)

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// TODO(crbug.com/409138442): Remove "Once" suffix from BNPL form event metrics
// and instead add comment that these are all "Once" metrics. For now, it is
// fine to leave as is to keep consistent with other in-progress CLs.
enum class BnplFormEvent {
  // Payments autofill suggestions were shown on a BNPL-eligible merchant.
  kSuggestionsShownOnce = 0,

  // The BNPL suggestion was added to the payments autofill dropdown and shown
  // to the user.
  kBnplSuggestionShownOnce = 1,

  // A BNPL suggestion was accepted on the current page.
  kBnplSuggestionAcceptedOnce = 2,

  // A form was filled with an Affirm VCN.
  kFormFilledWithAffirmOnce = 3,

  // A form was filled with a Zip VCN.
  kFormFilledWithZipOnce = 4,

  // A form was filled with an Afterpay VCN.
  kFormFilledWithAfterpayOnce = 5,

  // A form was submitted with an Affirm VCN.
  kFormSubmittedWithAffirmOnce = 6,

  // A form was submitted with a Zip VCN.
  kFormSubmittedWithZipOnce = 7,

  // A form was submitted with an Afterpay VCN.
  kFormSubmittedWithAfterpayOnce = 8,

  kMaxValue = kFormSubmittedWithAfterpayOnce,
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

// Logs the select BNPL issuer dialog result.
void LogSelectBnplIssuerDialogResult(SelectBnplIssuerDialogResult result);

// Logs the selection of BNPL issuer from the select BNPL issuer dialog.
void LogBnplIssuerSelection(std::string_view issuer_id);

// Logs that the BNPL suggestion was not shown and the reason why.
void LogBnplSuggestionNotShownReason(BnplSuggestionNotShownReason reason);

// Logs that the BNPL popup window was shown.
void LogBnplPopupWindowShown(std::string_view issuer_id);

// Logs the result of the BNPL popup window.
void LogBnplPopupWindowResult(std::string_view issuer_id,
                              BnplFlowResult result);

// Logs the duration a user took to go through the BNPL flow inside of the
// pop-up window. Broken down by issuer and result, because each issuer and
// each result should be looked at separately.
void LogBnplPopupWindowLatency(base::TimeDelta duration,
                               std::string_view issuer_id,
                               BnplFlowResult result);

// Logs BNPL form events. Please refer to `BnplFormEvent` for the possible
// enumerations that can be logged.
void LogBnplFormEvent(BnplFormEvent event);

// Logs that a form was filled with the BNPL issuer VCN.
void LogFormFilledWithBnplVcn(std::string_view issuer_id);

// Logs that a form was submitted with the BNPL issuer VCN.
void LogFormSubmittedWithBnplVcn(std::string_view issuer_id);

// Logs that the BNPL issuer selection dialog was shown.
void LogBnplSelectionDialogShown();

}  // namespace autofill::autofill_metrics

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_PAYMENTS_BNPL_METRICS_H_
