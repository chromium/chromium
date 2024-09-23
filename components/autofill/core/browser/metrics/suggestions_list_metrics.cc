// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/metrics/suggestions_list_metrics.h"

#include "base/metrics/histogram_functions.h"
#include "base/metrics/user_metrics.h"
#include "base/notreached.h"
#include "base/ranges/algorithm.h"
#include "base/strings/strcat.h"
#include "components/autofill/core/browser/filling_product.h"
#include "components/autofill/core/browser/metrics/autofill_metrics.h"

namespace autofill::autofill_metrics {

SuggestionRankingContext::SuggestionRankingContext() = default;
SuggestionRankingContext::SuggestionRankingContext(
    const SuggestionRankingContext&) = default;
SuggestionRankingContext& SuggestionRankingContext::operator=(
    const SuggestionRankingContext&) = default;
SuggestionRankingContext::~SuggestionRankingContext() = default;

// static
SuggestionRankingContext::RelativePosition
SuggestionRankingContext::GetRelativePositionEnum(size_t legacy_index,
                                                  size_t new_index) {
  // A lower index means that the suggestion was ranked higher.
  if (new_index < legacy_index) {
    return autofill_metrics::SuggestionRankingContext::RelativePosition::
        kRankedHigher;
  } else if (new_index > legacy_index) {
    return autofill_metrics::SuggestionRankingContext::RelativePosition::
        kRankedLower;
  }
  return autofill_metrics::SuggestionRankingContext::RelativePosition::
      kRankedSame;
}

bool SuggestionRankingContext::RankingsAreDifferent() const {
  return std::ranges::any_of(
      suggestion_rankings_difference_map, [](const auto& pair) {
        return pair.second != RelativePosition::kRankedSame;
      });
}

void LogSuggestionsCount(size_t num_suggestions,
                         FillingProduct filling_product) {
  switch (filling_product) {
    case FillingProduct::kAddress:
      base::UmaHistogramCounts100("Autofill.SuggestionsCount.Address",
                                  num_suggestions);
      break;
    case FillingProduct::kCreditCard:
      base::UmaHistogramCounts100("Autofill.SuggestionsCount.CreditCard",
                                  num_suggestions);
      break;
    case FillingProduct::kNone:
    case FillingProduct::kMerchantPromoCode:
    case FillingProduct::kIban:
    case FillingProduct::kAutocomplete:
    case FillingProduct::kPassword:
    case FillingProduct::kCompose:
    case FillingProduct::kPlusAddresses:
    case FillingProduct::kPredictionImprovements:
    case FillingProduct::kStandaloneCvc:
      NOTREACHED();
  }
}

void LogSuggestionAcceptedIndex(int index,
                                FillingProduct filling_product,
                                bool off_the_record) {
  const int uma_index = std::min(index, kMaxBucketsCount);
  base::UmaHistogramSparse("Autofill.SuggestionAcceptedIndex", uma_index);

  switch (filling_product) {
    case FillingProduct::kCreditCard:
    case FillingProduct::kStandaloneCvc:
      base::UmaHistogramSparse("Autofill.SuggestionAcceptedIndex.CreditCard",
                               uma_index);
      break;
    case FillingProduct::kAddress:
      base::UmaHistogramSparse("Autofill.SuggestionAcceptedIndex.Profile",
                               uma_index);
      break;
    case FillingProduct::kPassword:
    case FillingProduct::kNone:
      base::UmaHistogramSparse("Autofill.SuggestionAcceptedIndex.Other",
                               uma_index);
      break;
    case FillingProduct::kAutocomplete:
      base::UmaHistogramSparse("Autofill.SuggestionAcceptedIndex.Autocomplete",
                               uma_index);
      break;
    case FillingProduct::kIban:
    case FillingProduct::kCompose:
    case FillingProduct::kPlusAddresses:
    case FillingProduct::kPredictionImprovements:
    case FillingProduct::kMerchantPromoCode:
      // It is NOTREACHED because all other types should be handled separately.
      NOTREACHED();
  }

  base::RecordAction(base::UserMetricsAction("Autofill_SelectedSuggestion"));

  base::UmaHistogramBoolean("Autofill.SuggestionAccepted.OffTheRecord",
                            off_the_record);
}

void LogAutofillShowCardsFromGoogleAccountButtonEventMetric(
    ShowCardsFromGoogleAccountButtonEvent event) {
  base::UmaHistogramEnumeration(
      "Autofill.ButterForPayments.ShowCardsFromGoogleAccountButtonEvents",
      event);
}

void LogAutofillRankingSuggestionDifference(
    SuggestionRankingContext::RelativePosition ranking_difference) {
  base::UmaHistogramEnumeration(
      "Autofill.SuggestionAccepted.SuggestionRankingDifference.CreditCard",
      ranking_difference);
}

}  // namespace autofill::autofill_metrics
