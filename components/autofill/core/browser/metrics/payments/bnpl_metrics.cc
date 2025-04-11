// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/metrics/payments/bnpl_metrics.h"

#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "components/autofill/core/browser/payments/constants.h"

namespace autofill::autofill_metrics {

SupportedBnplIssuer GetEnumForIssuerId(std::string_view issuer_id) {
  if (issuer_id == kBnplAffirmIssuerId) {
    return SupportedBnplIssuer::kAffirm;
  } else if (issuer_id == kBnplZipIssuerId) {
    return SupportedBnplIssuer::kZip;
  } else if (issuer_id == kBnplAfterpayIssuerId) {
    return SupportedBnplIssuer::kAfterpay;
  }
  NOTREACHED();
}

std::string GetHistogramSuffixFromIssuerId(std::string_view issuer_id) {
  if (issuer_id == kBnplAffirmIssuerId) {
    return "Affirm";
  } else if (issuer_id == kBnplZipIssuerId) {
    return "Zip";
  } else if (issuer_id == kBnplAfterpayIssuerId) {
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

void LogBnplTosDialogShown(std::string_view issuer_id) {
  std::string histogram_name =
      base::StrCat({"Autofill.Bnpl.TosDialogShown.",
                    GetHistogramSuffixFromIssuerId(issuer_id)});
  base::UmaHistogramBoolean(histogram_name, /*sample=*/true);
}

void LogBnplTosDialogResult(BnplTosDialogResult result,
                            std::string_view issuer_id) {
  std::string histogram_name =
      base::StrCat({"Autofill.Bnpl.TosDialogResult.",
                    GetHistogramSuffixFromIssuerId(issuer_id)});
  base::UmaHistogramEnumeration(histogram_name, result);
}

void LogSelectBnplIssuerDialogResult(SelectBnplIssuerDialogResult result) {
  base::UmaHistogramEnumeration("Autofill.Bnpl.SelectionDialogResult", result);
}

void LogBnplIssuerSelection(std::string_view issuer_id) {
  base::UmaHistogramEnumeration("Autofill.Bnpl.SelectionDialogIssuerSelected",
                                GetEnumForIssuerId(issuer_id));
}

void LogBnplSuggestionNotShownReason(BnplSuggestionNotShownReason reason) {
  base::UmaHistogramEnumeration("Autofill.Bnpl.SuggestionNotShownReason",
                                reason);
}

void LogBnplPopupWindowShown(std::string_view issuer_id) {
  std::string histogram_name =
      base::StrCat({"Autofill.Bnpl.PopupWindowShown.",
                    GetHistogramSuffixFromIssuerId(issuer_id)});
  base::UmaHistogramBoolean(histogram_name, /*sample=*/true);
}

void LogBnplPopupWindowResult(std::string_view issuer_id,
                              BnplFlowResult result) {
  std::string histogram_name =
      base::StrCat({"Autofill.Bnpl.PopupWindowResult.",
                    GetHistogramSuffixFromIssuerId(issuer_id)});
  base::UmaHistogramEnumeration(histogram_name, result);
}

void LogBnplPopupWindowLatency(base::TimeDelta duration,
                               std::string_view issuer_id,
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

void LogFormFilledWithBnplVcn(std::string_view issuer_id) {
  if (issuer_id == kBnplAffirmIssuerId) {
    LogBnplFormEvent(BnplFormEvent::kFormFilledWithAffirmOnce);
  } else if (issuer_id == kBnplZipIssuerId) {
    LogBnplFormEvent(BnplFormEvent::kFormFilledWithZipOnce);
  } else if (issuer_id == kBnplAfterpayIssuerId) {
    LogBnplFormEvent(BnplFormEvent::kFormFilledWithAfterpayOnce);
  } else {
    NOTREACHED();
  }
}

void LogFormSubmittedWithBnplVcn(std::string_view issuer_id) {
  if (issuer_id == kBnplAffirmIssuerId) {
    LogBnplFormEvent(BnplFormEvent::kFormSubmittedWithAffirmOnce);
  } else if (issuer_id == kBnplZipIssuerId) {
    LogBnplFormEvent(BnplFormEvent::kFormSubmittedWithZipOnce);
  } else if (issuer_id == kBnplAfterpayIssuerId) {
    LogBnplFormEvent(BnplFormEvent::kFormSubmittedWithAfterpayOnce);
  } else {
    NOTREACHED();
  }
}

void LogBnplSelectionDialogShown() {
  base::UmaHistogramBoolean("Autofill.Bnpl.SelectionDialogShown",
                            /*sample=*/true);
}

}  // namespace autofill::autofill_metrics
