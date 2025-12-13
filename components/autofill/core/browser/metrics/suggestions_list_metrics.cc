// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/metrics/suggestions_list_metrics.h"

#include <algorithm>

#include "base/metrics/histogram_functions.h"
#include "base/metrics/user_metrics.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/autofill_type.h"
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
    case FillingProduct::kPlusAddresses:
    case FillingProduct::kAutofillAi:
    case FillingProduct::kLoyaltyCard:
    case FillingProduct::kIdentityCredential:
    case FillingProduct::kDataList:
    case FillingProduct::kOneTimePassword:
    case FillingProduct::kPasskey:
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
      base::UmaHistogramSparse("Autofill.SuggestionAcceptedIndex.CreditCard",
                               uma_index);
      break;
    case FillingProduct::kAddress:
      base::UmaHistogramSparse("Autofill.SuggestionAcceptedIndex.Profile",
                               uma_index);
      break;
    case FillingProduct::kAutocomplete:
      base::UmaHistogramSparse("Autofill.SuggestionAcceptedIndex.Autocomplete",
                               uma_index);
      break;
    case FillingProduct::kIban:
    case FillingProduct::kLoyaltyCard:
    case FillingProduct::kCompose:
    case FillingProduct::kPlusAddresses:
    case FillingProduct::kAutofillAi:
    case FillingProduct::kMerchantPromoCode:
    case FillingProduct::kIdentityCredential:
    case FillingProduct::kPassword:
    case FillingProduct::kNone:
    case FillingProduct::kDataList:
    case FillingProduct::kOneTimePassword:
    case FillingProduct::kPasskey:
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
