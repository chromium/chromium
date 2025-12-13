// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/metrics/payments/credit_card_save_metrics_desktop.h"

#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "components/autofill/core/browser/metrics/payments/credit_card_save_metrics.h"
#include "components/autofill/core/browser/payments/payments_autofill_client.h"

namespace autofill::autofill_metrics {
namespace {

// Helper function to log metrics with sub histograms based on
// SaveCreditCardOptions. SaveCardPromptMetricType will be either
// SaveCardPromptOffer or SaveCardPromptResultDesktop.
// Note: If you add a new sub-histogram here, it must be added to histograms.xml
// for each SaveCardPromptMetricType supported by this function.
template <typename SaveCardPromptMetricType>
void LogSubHistograms(
    const std::string& base_histogram_name,
    SaveCardPromptMetricType metric,
    bool is_upload_save,
    const payments::PaymentsAutofillClient::SaveCreditCardOptions&
        save_credit_card_options) {
  if (save_credit_card_options.should_request_name_from_user) {
    base::UmaHistogramEnumeration(
        base::StrCat({base_histogram_name, ".RequestingCardholderName"}),
        metric);
  }
  if (save_credit_card_options.should_request_expiration_date_from_user) {
    base::UmaHistogramEnumeration(
        base::StrCat({base_histogram_name, ".RequestingExpirationDate"}),
        metric);
  }
  if (save_credit_card_options.card_save_type ==
      payments::PaymentsAutofillClient::CardSaveType::kCardSaveWithCvc) {
    base::UmaHistogramEnumeration(
        base::StrCat({base_histogram_name, ".SavingWithCvc"}), metric);
  }
  if (save_credit_card_options.has_multiple_legal_lines) {
    CHECK(is_upload_save);
    base::UmaHistogramEnumeration(
        base::StrCat({base_histogram_name, ".WithMultipleLegalLines"}), metric);
  }
  if (save_credit_card_options
          .has_same_last_four_as_server_card_but_different_expiration_date) {
    CHECK(is_upload_save);
    base::UmaHistogramEnumeration(
        base::StrCat(
            {base_histogram_name, ".WithSameLastFourButDifferentExpiration"}),
        metric);
  }
}

}  // namespace

void LogSaveCreditCardPromptOfferMetricDesktop(
    SaveCardPromptOffer metric,
    bool is_upload_save,
    const payments::PaymentsAutofillClient::SaveCreditCardOptions&
        save_credit_card_options) {
  std::string_view destination = is_upload_save ? ".Server" : ".Local";
  std::string base_histogram_name =
      base::StrCat({"Autofill.SaveCreditCardPromptOffer.Desktop", destination});
  base::UmaHistogramEnumeration(base_histogram_name, metric);

  LogSubHistograms(base_histogram_name, metric, is_upload_save,
                   save_credit_card_options);
}

void LogSaveCreditCardPromptResultMetricDesktop(
    SaveCardPromptResultDesktop metric,
    bool is_upload_save,
    const payments::PaymentsAutofillClient::SaveCreditCardOptions&
        save_credit_card_options,
    bool has_saved_cards) {
  std::string_view destination = is_upload_save ? ".Server" : ".Local";
  std::string base_histogram_name = base::StrCat(
      {"Autofill.SaveCreditCardPromptResult.Desktop", destination});
  base::UmaHistogramEnumeration(base_histogram_name, metric);

  LogSubHistograms(base_histogram_name, metric, is_upload_save,
                   save_credit_card_options);

  base::UmaHistogramEnumeration(
      base::StrCat({base_histogram_name, has_saved_cards ? ".UserHasSavedCards"
                                                         : ".UserHasNoCards"}),
      metric);
}

}  // namespace autofill::autofill_metrics
