// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/metrics/payments/save_and_fill_metrics.h"

#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"

namespace autofill::autofill_metrics {

void LogSaveAndFillFormEvent(SaveAndFillFormEvent event) {
  base::UmaHistogramEnumeration("Autofill.FormEvents.CreditCard.SaveAndFill",
                                event);
}

void LogSaveAndFillSuggestionNotShownReason(
    SaveAndFillSuggestionNotShownReason reason) {
  base::UmaHistogramEnumeration("Autofill.SaveAndFill.SuggestionNotShownReason",
                                reason);
}

void LogSaveAndFillGetDetailsForCreateCardResultAndLatency(
    bool succeeded,
    base::TimeDelta latency) {
  static constexpr std::string_view kHistogramName =
      "Autofill.SaveAndFill.GetDetailsForCreateCard.Latency";
  base::UmaHistogramMediumTimes(kHistogramName, latency);
  base::UmaHistogramMediumTimes(
      base::StrCat({kHistogramName, succeeded ? ".Success" : ".Failure"}),
      latency);
}

void LogSaveAndFillCreateCardResultAndLatency(bool succeeded,
                                              base::TimeDelta latency) {
  static constexpr std::string_view kHistogramName =
      "Autofill.SaveAndFill.CreateCard.Latency";
  base::UmaHistogramMediumTimes(kHistogramName, latency);
  base::UmaHistogramMediumTimes(
      base::StrCat({kHistogramName, succeeded ? ".Success" : ".Failure"}),
      latency);
}

void LogSaveAndFillStrikeDatabaseBlockReason(
    AutofillMetrics::AutofillStrikeDatabaseBlockReason reason) {
  base::UmaHistogramEnumeration(
      "Autofill.StrikeDatabase.SaveAndFillStrikeDatabaseBlockReason", reason);
}

void LogSaveAndFillNumOfStrikesPresentWhenDialogAccepted(int strike_count) {
  base::UmaHistogramCounts100(
      "Autofill.StrikeDatabase.NumOfStrikesPresentWhenSaveAndFillAccepted",
      strike_count);
}

void LogSaveAndFillDialogResult(SaveAndFillDialogResult result) {
  base::UmaHistogramEnumeration("Autofill.SaveAndFill.DialogResult", result);
}

void LogSaveAndFillDialogShown(bool is_upload) {
  base::UmaHistogramBoolean(base::StrCat({"Autofill.SaveAndFill.DialogShown.",
                                          is_upload ? "Upload" : "Local"}),
                            /*sample=*/true);
}

void LogSaveAndFillFunnelMetrics(bool succeeded,
                                 bool is_for_upload,
                                 SaveAndFillFormEvent event) {
  base::UmaHistogramEnumeration(
      base::StrCat({"Autofill.SaveAndFill.Funnel",
                    is_for_upload ? ".Upload" : ".Local",
                    succeeded ? ".Success" : ".Failure"}),
      event);
}

}  // namespace autofill::autofill_metrics
