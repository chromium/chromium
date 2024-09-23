// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/metrics/payments/iban_metrics.h"

#include <string>
#include <vector>

#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/strcat.h"
#include "components/autofill/core/browser/data_model/iban.h"
#include "components/autofill/core/common/autofill_clock.h"

namespace autofill::autofill_metrics {

void LogStoredIbanMetrics(
    const std::vector<std::unique_ptr<Iban>>& local_ibans,
    const std::vector<std::unique_ptr<Iban>>& server_ibans,
    base::TimeDelta disused_data_threshold) {
  auto LogStoredIban = [disused_data_threshold](
                           const std::vector<std::unique_ptr<Iban>>& ibans) {
    if (ibans.empty()) {
      return;
    }
    // Iterate over all of the IBANs and gather metrics.
    size_t num_ibans_with_nickname = 0;
    size_t num_disused_ibans = 0;
    const base::Time now = AutofillClock::Now();
    const std::string histogram_suffix =
        ibans[0]->record_type() == Iban::kLocalIban ? "Local" : "Server";
    for (const std::unique_ptr<Iban>& iban : ibans) {
      const base::TimeDelta time_since_last_use = now - iban->use_date();
      if (time_since_last_use > disused_data_threshold) {
        num_disused_ibans++;
      }
      base::UmaHistogramCounts1000(
          base::StrCat(
              {"Autofill.DaysSinceLastUse.StoredIban.", histogram_suffix}),
          time_since_last_use.InDays());
      if (!iban->nickname().empty()) {
        num_ibans_with_nickname++;
      }
    }

    base::UmaHistogramCounts100(
        base::StrCat({"Autofill.StoredIbanCount.", histogram_suffix}),
        ibans.size());
    base::UmaHistogramCounts100(
        base::StrCat(
            {"Autofill.StoredIbanCount.", histogram_suffix, ".WithNickname"}),
        num_ibans_with_nickname);

    base::UmaHistogramCounts100(
        base::StrCat({"Autofill.StoredIbanDisusedCount.", histogram_suffix}),
        num_disused_ibans);
  };

  LogStoredIban(local_ibans);
  LogStoredIban(server_ibans);
  base::UmaHistogramCounts100("Autofill.StoredIbanCount",
                              local_ibans.size() + server_ibans.size());
}

void LogDaysSinceLastIbanUse(const Iban& iban) {
  CHECK(iban.record_type() == Iban::RecordType::kLocalIban ||
        iban.record_type() == Iban::RecordType::kServerIban);
  base::UmaHistogramCounts1000(
      base::StrCat({"Autofill.DaysSinceLastUse.StoredIban.",
                    (iban.record_type() == Iban::RecordType::kServerIban)
                        ? "Server"
                        : "Local"}),
      (AutofillClock::Now() - iban.use_date()).InDays());
}

void LogStrikesPresentWhenIbanSaved(const int num_strikes,
                                    bool is_upload_save) {
  base::UmaHistogramCounts100(
      base::StrCat({"Autofill.StrikeDatabase.StrikesPresentWhenIbanSaved.",
                    is_upload_save ? "Upload" : "Local"}),
      num_strikes);
}

void LogIbanSaveNotOfferedDueToMaxStrikesMetric(
    AutofillMetrics::SaveTypeMetric metric) {
  base::UmaHistogramEnumeration(
      "Autofill.StrikeDatabase.IbanSaveNotOfferedDueToMaxStrikes", metric);
}

void LogUploadIbanMetric(UploadIbanOriginMetric origin_metric,
                         UploadIbanActionMetric action_metric) {
  std::string histogram_name = "Autofill.UploadIban.";
  switch (action_metric) {
    case UploadIbanActionMetric::kOffered:
      histogram_name += "Offered";
      break;
    case UploadIbanActionMetric::kAccepted:
      histogram_name += "Accepted";
      break;
    case UploadIbanActionMetric::kDeclined:
      histogram_name += "Declined";
      break;
    case UploadIbanActionMetric::kIgnored:
      histogram_name += "Ignored";
      break;
  }
  base::UmaHistogramEnumeration(histogram_name, origin_metric);
}

void LogSaveIbanBubbleOfferMetric(SaveIbanPromptOffer metric,
                                  bool is_reshow,
                                  bool is_upload_save) {
  std::string base_histogram_name = base::StrCat(
      {"Autofill.SaveIbanPromptOffer.", is_upload_save ? "Upload" : "Local",
       is_reshow ? ".Reshows" : ".FirstShow"});
  base::UmaHistogramEnumeration(base_histogram_name, metric);
}

void LogSaveIbanBubbleResultMetric(SaveIbanBubbleResult metric,
                                   bool is_reshow,
                                   bool is_upload_save) {
  std::string base_histogram_name = base::StrCat(
      {"Autofill.SaveIbanPromptResult.", is_upload_save ? "Upload" : "Local",
       is_reshow ? ".Reshows" : ".FirstShow"});
  base::UmaHistogramEnumeration(base_histogram_name, metric);
}

void LogSaveIbanBubbleResultSavedWithNicknameMetric(bool save_with_nickname,
                                                    bool is_upload_save) {
  base::UmaHistogramBoolean(
      base::StrCat({"Autofill.SaveIbanPromptResult.",
                    is_upload_save ? "Upload" : "Local", ".SavedWithNickname"}),
      save_with_nickname);
}

void LogIndividualIbanSuggestionsEvent(IbanSuggestionsEvent event) {
  base::UmaHistogramEnumeration("Autofill.Iban.Suggestions", event);
}

void LogIbanSuggestionBlockListStatusMetric(
    IbanSuggestionBlockListStatus event) {
  base::UmaHistogramEnumeration(
      "Autofill.Iban.ShowSuggestionsBlocklistDecision", event);
}

void LogServerIbanLinkClicked(AutofillMetrics::PaymentsSigninState sync_state) {
  base::UmaHistogramEnumeration("Autofill.ServerIbanLinkClicked", sync_state);
}

void LogIbanUploadEnabledMetric(
    IbanUploadEnabledStatus metric,
    AutofillMetrics::PaymentsSigninState sync_state) {
  const std::string base_metric = std::string("Autofill.IbanUploadEnabled");
  base::UmaHistogramEnumeration(base_metric, metric);

  const std::string sync_subhistogram_metric =
      base_metric + AutofillMetrics::GetMetricsSyncStateSuffix(sync_state);
  base::UmaHistogramEnumeration(sync_subhistogram_metric, metric);
}

void LogServerIbanUnmaskLatency(base::TimeDelta latency, bool is_successful) {
  base::UmaHistogramTimes(base::StrCat({"Autofill.Iban.UnmaskIbanDuration.",
                                        is_successful ? "Success" : "Failure"}),
                          latency);
  base::UmaHistogramTimes("Autofill.Iban.UnmaskIbanDuration", latency);
}

void LogServerIbanUnmaskStatus(bool is_successful) {
  base::UmaHistogramBoolean("Autofill.Iban.UnmaskIbanResult", is_successful);
}

void LogIbanSaveOfferedCountry(std::string_view country_code) {
  base::UmaHistogramEnumeration("Autofill.Iban.CountryOfSaveOfferedIban",
                                Iban::GetIbanSupportedCountry(country_code));
}

void LogIbanSaveAcceptedCountry(std::string_view country_code) {
  base::UmaHistogramEnumeration("Autofill.Iban.CountryOfSaveAcceptedIban",
                                Iban::GetIbanSupportedCountry(country_code));
}

void LogIbanSelectedCountry(std::string_view country_code) {
  base::UmaHistogramEnumeration("Autofill.Iban.CountryOfSelectedIban",
                                Iban::GetIbanSupportedCountry(country_code));
}

void LogIbanUploadSaveFailed(bool iban_saved_locally) {
  base::UmaHistogramBoolean("Autofill.IbanUpload.SaveFailed",
                            iban_saved_locally);
}

}  // namespace autofill::autofill_metrics
