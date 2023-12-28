// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/metrics/suggestions_list_metrics.h"

#include "base/metrics/histogram_functions.h"
#include "base/metrics/user_metrics.h"
#include "components/autofill/core/browser/filling_product.h"
#include "components/autofill/core/browser/metrics/autofill_metrics.h"
#include "components/autofill/core/browser/ui/popup_types.h"

namespace autofill::autofill_metrics {
namespace {

ManageSuggestionType ToManageSuggestionType(PopupType popup_type) {
  switch (popup_type) {
    case PopupType::kAddresses:
      return ManageSuggestionType::kAddresses;
    case PopupType::kCreditCards:
      return ManageSuggestionType::kPaymentMethodsCreditCards;
    case PopupType::kIbans:
      return ManageSuggestionType::kPaymentMethodsIbans;
    case PopupType::kPasswords:
      ABSL_FALLTHROUGH_INTENDED;
    case PopupType::kAutocomplete:
    case PopupType::kUnspecified:
      return ManageSuggestionType::kOther;
  }
}

}  // anonymous namespace

void LogAutofillSuggestionAcceptedIndex(int index,
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

void LogAutofillSelectedManageEntry(PopupType popup_type) {
  const ManageSuggestionType uma_type = ToManageSuggestionType(popup_type);
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
