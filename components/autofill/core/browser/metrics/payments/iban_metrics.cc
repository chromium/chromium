// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/metrics/payments/iban_metrics.h"

#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"

namespace autofill::autofill_metrics {

void LogStrikesPresentWhenIBANSaved(const int num_strikes) {
  base::UmaHistogramCounts100(
      "Autofill.StrikeDatabase.StrikesPresentWhenIbanSaved.Local", num_strikes);
}

void LogIBANSaveNotOfferedDueToMaxStrikesMetric(
    AutofillMetrics::SaveTypeMetric metric) {
  base::UmaHistogramEnumeration(
      "Autofill.StrikeDatabase.IbanSaveNotOfferedDueToMaxStrikes", metric);
}

void LogSaveIbanBubbleOfferMetric(SaveIbanPromptOffer metric, bool is_reshow) {
  std::string base_histogram_name = "Autofill.SaveIbanPromptOffer.Local";
  std::string show = is_reshow ? ".Reshows" : ".FirstShow";
  base::UmaHistogramEnumeration(base_histogram_name + show, metric);
}

void LogSaveIbanBubbleResultMetric(SaveIbanBubbleResult metric,
                                   bool is_reshow) {
  std::string base_histogram_name = "Autofill.SaveIbanPromptResult.Local";
  std::string show = is_reshow ? ".Reshows" : ".FirstShow";
  base::UmaHistogramEnumeration(base_histogram_name + show, metric);
}

void LogSaveIbanBubbleResultSavedWithNicknameMetric(bool save_with_nickname) {
  base::UmaHistogramBoolean(
      "Autofill.SaveIbanPromptResult.Local.SavedWithNickname",
      save_with_nickname);
}

}  // namespace autofill::autofill_metrics
