// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/metrics/payments/iban_metrics.h"

#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "components/autofill/core/common/autofill_clock.h"

namespace autofill::autofill_metrics {

void LogStoredIbanMetrics(const std::vector<std::unique_ptr<IBAN>>& local_ibans,
                          const base::TimeDelta& disused_data_threshold) {
  // Iterate over all of the IBANs and gather metrics.
  size_t num_local_ibans_with_nickname = 0;
  size_t num_disused_local_ibans = 0;
  const base::Time now = AutofillClock::Now();
  for (const auto& iban : local_ibans) {
    const base::TimeDelta time_since_last_use = now - iban->use_date();
    if (time_since_last_use > disused_data_threshold) {
      num_disused_local_ibans++;
    }
    base::UmaHistogramCounts1000("Autofill.DaysSinceLastUse.StoredIban.Local",
                                 time_since_last_use.InDays());
    if (!iban->nickname().empty()) {
      num_local_ibans_with_nickname++;
    }
  }

  base::UmaHistogramCounts100("Autofill.StoredIbanCount.Local",
                              local_ibans.size());
  base::UmaHistogramCounts100("Autofill.StoredIbanCount.Local.WithNickname",
                              num_local_ibans_with_nickname);

  base::UmaHistogramCounts100("Autofill.StoredIbanDisusedCount.Local",
                              num_disused_local_ibans);
}

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
