// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/metrics/suggestions_list_metrics.h"

#include <stddef.h>

#include <algorithm>

#include "base/containers/span.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/autofill_type.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/filling/filling_product.h"
#include "components/autofill/core/browser/metrics/autofill_metrics.h"

namespace autofill::autofill_metrics {

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
    case FillingProduct::kAutofillAi:
    case FillingProduct::kLoyaltyCard:
    case FillingProduct::kIdentityCredential:
    case FillingProduct::kDataList:
    case FillingProduct::kOneTimePassword:
    case FillingProduct::kPasskey:
    case FillingProduct::kAtMemory:
      NOTREACHED();
  }
}

void LogSuggestionAcceptedIndex(
    int index,
    FillingProduct filling_product,
    bool off_the_record,
    base::span<const SuggestionType> shown_suggestion_types) {
  const int uma_index = std::min(index, kMaxBucketsCount);
  base::UmaHistogramSparse("Autofill.SuggestionAcceptedIndex", uma_index);

  const int num_of_suggestions =
      std::ranges::count(shown_suggestion_types, filling_product,
                         &GetFillingProductFromSuggestionType);
  // Records the metric
  // "Autofill.SuggestionAcceptedIndex.DisplayedAtLeast{Min}.{Product}"
  // where "{Min}" is the minimum number of shown suggestions necessary for
  // the interaction to be considered for this metric. The purpose of the
  // metric is to filter out the majority of users that have only very few
  // suggestions which leads to a shift towards lower number in
  // "Autofill.SuggestionAcceptedIndex.{Product}".
  auto log_accepted_index_displayed_at_least = [&](int min_suggestions) {
    if (num_of_suggestions >= min_suggestions) {
      base::UmaHistogramSparse(
          base::StrCat({"Autofill.SuggestionAcceptedIndex.DisplayedAtLeast",
                        base::NumberToString(min_suggestions), ".",
                        FillingProductToString(filling_product)}),
          uma_index);
    }
  };
  switch (filling_product) {
    case FillingProduct::kCreditCard:
    case FillingProduct::kAddress:
    case FillingProduct::kAutocomplete:
    case FillingProduct::kAutofillAi:
      base::UmaHistogramSparse(
          base::StrCat({"Autofill.SuggestionAcceptedIndex.",
                        FillingProductToString(filling_product)}),
          uma_index);
      log_accepted_index_displayed_at_least(5);
      log_accepted_index_displayed_at_least(10);
      log_accepted_index_displayed_at_least(20);
      break;
    case FillingProduct::kIban:
    case FillingProduct::kLoyaltyCard:
    case FillingProduct::kCompose:
    case FillingProduct::kMerchantPromoCode:
    case FillingProduct::kIdentityCredential:
    case FillingProduct::kPassword:
    case FillingProduct::kNone:
    case FillingProduct::kDataList:
    case FillingProduct::kOneTimePassword:
    case FillingProduct::kPasskey:
    case FillingProduct::kAtMemory:
      // It is NOTREACHED because all other types should be handled separately.
      NOTREACHED();
  }

  base::RecordAction(base::UserMetricsAction("Autofill_SelectedSuggestion"));

  base::UmaHistogramBoolean("Autofill.SuggestionAccepted.OffTheRecord",
                            off_the_record);
}

void LogAddressAutofillOnTypingSuggestionAccepted(
    FieldType field_type_used,
    const AutofillField* autofill_trigger_field) {
  // TODO(crbug.com/381994105): Consider deleting this metric in favor or
  // Autofill.AddressSuggestionOnTypingAcceptance.PerFieldType.
  base::UmaHistogramEnumeration(
      "Autofill.AddressSuggestionOnTyping.AddressFieldTypeUsed",
      field_type_used, FieldType::MAX_VALID_FIELD_TYPE);
  FieldTypeSet field_types = autofill_trigger_field
                                 ? autofill_trigger_field->Type().GetTypes()
                                 : FieldTypeSet{};
  base::UmaHistogramBoolean(
      "Autofill.AddressSuggestionOnTypingAcceptance.FieldClassication",
      !FieldTypeSet{NO_SERVER_DATA, UNKNOWN_TYPE, EMPTY_TYPE}.contains_all(
          field_types));
  if (autofill_trigger_field) {
    base::UmaHistogramCounts100(
        "Autofill.AddressSuggestionOnTypingAcceptance.NumberOfCharactersTyped",
        autofill_trigger_field->value().length());
  }
}

}  // namespace autofill::autofill_metrics
