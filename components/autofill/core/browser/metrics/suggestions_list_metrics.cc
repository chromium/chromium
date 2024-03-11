// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/metrics/suggestions_list_metrics.h"

#include "base/metrics/histogram_functions.h"
#include "base/metrics/user_metrics.h"
#include "base/notreached.h"
#include "components/autofill/core/browser/filling_product.h"
#include "components/autofill/core/browser/metrics/autofill_metrics.h"

namespace autofill::autofill_metrics {
namespace {

ManageSuggestionType ToManageSuggestionType(FillingProduct popup_type) {
  switch (popup_type) {
    case FillingProduct::kAddress:
      return ManageSuggestionType::kAddresses;
    case FillingProduct::kCreditCard:
      return ManageSuggestionType::kPaymentMethodsCreditCards;
    case FillingProduct::kIban:
      return ManageSuggestionType::kPaymentMethodsIbans;
    case FillingProduct::kAutocomplete:
    case FillingProduct::kCompose:
    case FillingProduct::kMerchantPromoCode:
    case FillingProduct::kPassword:
    case FillingProduct::kPlusAddresses:
    case FillingProduct::kNone:
      return ManageSuggestionType::kOther;
  }
}

}  // anonymous namespace

void LogSuggestionsCount(size_t num_suggestions,
                         FillingProduct filling_product) {
  switch (filling_product) {
    case FillingProduct::kAddress:
      // TODO(b/324029575): Remove when the metric below gets to Stable.
      base::UmaHistogramCounts100("Autofill.AddressSuggestionsCount",
                                  num_suggestions);
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
      NOTREACHED_NORETURN();
  }
}

void LogSuggestionAcceptedIndex(int index,
                                FillingProduct filling_product,
                                bool off_the_record) {
  const int uma_index = std::min(index, kMaxBucketsCount);
  base::UmaHistogramSparse("Autofill.SuggestionAcceptedIndex", uma_index);

  switch (filling_product) {
    case FillingProduct::kCreditCard:
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
    case FillingProduct::kMerchantPromoCode:
      // It is NOTREACHED because all other types should be handled separately.
      NOTREACHED_NORETURN();
  }

  base::RecordAction(base::UserMetricsAction("Autofill_SelectedSuggestion"));

  base::UmaHistogramBoolean("Autofill.SuggestionAccepted.OffTheRecord",
                            off_the_record);
}

void LogAutofillSelectedManageEntry(FillingProduct filling_product) {
  const ManageSuggestionType uma_type = ToManageSuggestionType(filling_product);
  base::UmaHistogramEnumeration("Autofill.SuggestionsListManageClicked",
                                uma_type);
}

void LogAutofillShowCardsFromGoogleAccountButtonEventMetric(
    ShowCardsFromGoogleAccountButtonEvent event) {
  base::UmaHistogramEnumeration(
      "Autofill.ButterForPayments.ShowCardsFromGoogleAccountButtonEvents",
      event);
}

}  // namespace autofill::autofill_metrics
