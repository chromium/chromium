// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/metrics/payments/bnpl_metrics.h"

#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "components/autofill/core/browser/data_model/payments/bnpl_issuer.h"
#include "components/autofill/core/browser/payments/constants.h"

namespace autofill::autofill_metrics {

using IssuerId = autofill::BnplIssuer::IssuerId;

std::string GetHistogramSuffixFromIssuerId(IssuerId issuer_id) {
  switch (issuer_id) {
    case IssuerId::kBnplAffirm:
      return "Affirm";
    case IssuerId::kBnplZip:
      return "Zip";
    case IssuerId::kBnplAfterpay:
      return "Afterpay";
  }
  NOTREACHED();
}

std::string ConvertBnplFlowResultToString(BnplFlowResult result) {
  switch (result) {
    case BnplFlowResult::kSuccess:
      return "Success";
    case BnplFlowResult::kFailure:
      return "Failure";
    case BnplFlowResult::kUserClosed:
      return "UserClosed";
  }
  NOTREACHED();
}

void LogBnplPrefToggled(bool enabled) {
  base::UmaHistogramBoolean("Autofill.SettingsPage.BnplToggled", enabled);
}

void LogBnplIssuersSyncedCountAtStartup(int count) {
  base::UmaHistogramCounts100("Autofill.Bnpl.IssuersSyncedCount.Startup",
                              count);
}

void LogBnplTosDialogShown(IssuerId issuer_id) {
  std::string histogram_name =
      base::StrCat({"Autofill.Bnpl.TosDialogShown.",
                    GetHistogramSuffixFromIssuerId(issuer_id)});
  base::UmaHistogramBoolean(histogram_name, /*sample=*/true);
}

void LogBnplTosDialogResult(BnplTosDialogResult result, IssuerId issuer_id) {
  std::string histogram_name =
      base::StrCat({"Autofill.Bnpl.TosDialogResult.",
                    GetHistogramSuffixFromIssuerId(issuer_id)});
  base::UmaHistogramEnumeration(histogram_name, result);
}

void LogSelectBnplIssuerDialogResult(SelectBnplIssuerDialogResult result) {
  base::UmaHistogramEnumeration("Autofill.Bnpl.SelectionDialogResult", result);
}

void LogBnplIssuerSelection(IssuerId issuer_id) {
  base::UmaHistogramEnumeration("Autofill.Bnpl.SelectionDialogIssuerSelected",
                                issuer_id);
}

void LogBnplSuggestionNotShownReason(BnplSuggestionNotShownReason reason) {
  base::UmaHistogramEnumeration("Autofill.Bnpl.SuggestionNotShownReason",
                                reason);
}

void LogBnplPopupWindowShown(IssuerId issuer_id) {
  std::string histogram_name =
      base::StrCat({"Autofill.Bnpl.PopupWindowShown.",
                    GetHistogramSuffixFromIssuerId(issuer_id)});
  base::UmaHistogramBoolean(histogram_name, /*sample=*/true);
}

void LogBnplPopupWindowResult(IssuerId issuer_id, BnplFlowResult result) {
  std::string histogram_name =
      base::StrCat({"Autofill.Bnpl.PopupWindowResult.",
                    GetHistogramSuffixFromIssuerId(issuer_id)});
  base::UmaHistogramEnumeration(histogram_name, result);
}

void LogBnplPopupWindowLatency(base::TimeDelta duration,
                               IssuerId issuer_id,
                               BnplFlowResult result) {
  std::string histogram_name =
      base::StrCat({"Autofill.Bnpl.PopupWindowLatency.",
                    GetHistogramSuffixFromIssuerId(issuer_id), ".",
                    ConvertBnplFlowResultToString(result)});

  base::UmaHistogramLongTimes(histogram_name, duration);
}

void LogBnplFormEvent(BnplFormEvent event) {
  base::UmaHistogramEnumeration("Autofill.FormEvents.CreditCard.Bnpl", event);
}

void LogFormFilledWithBnplVcn(IssuerId issuer_id) {
  switch (issuer_id) {
    case IssuerId::kBnplAffirm:
      LogBnplFormEvent(BnplFormEvent::kFormFilledWithAffirm);
      return;
    case IssuerId::kBnplZip:
      LogBnplFormEvent(BnplFormEvent::kFormFilledWithZip);
      return;
    case IssuerId::kBnplAfterpay:
      LogBnplFormEvent(BnplFormEvent::kFormFilledWithAfterpay);
      return;
  }
  NOTREACHED();
}

void LogFormSubmittedWithBnplVcn(IssuerId issuer_id) {
  switch (issuer_id) {
    case IssuerId::kBnplAffirm:
      LogBnplFormEvent(BnplFormEvent::kFormSubmittedWithAffirm);
      return;
    case IssuerId::kBnplZip:
      LogBnplFormEvent(BnplFormEvent::kFormSubmittedWithZip);
      return;
    case IssuerId::kBnplAfterpay:
      LogBnplFormEvent(BnplFormEvent::kFormSubmittedWithAfterpay);
      return;
  }
  NOTREACHED();
}

void LogBnplSelectionDialogShown() {
  base::UmaHistogramBoolean("Autofill.Bnpl.SelectionDialogShown",
                            /*sample=*/true);
}

}  // namespace autofill::autofill_metrics
