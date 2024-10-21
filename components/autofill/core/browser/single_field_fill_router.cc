// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/single_field_fill_router.h"

#include <string>
#include <vector>

#include "base/check_deref.h"
#include "components/autofill/core/browser/autocomplete_history_manager.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/merchant_promo_code_manager.h"
#include "components/autofill/core/browser/payments/iban_manager.h"

namespace autofill {

SingleFieldFillRouter::SingleFieldFillRouter(
    AutocompleteHistoryManager* autocomplete_history_manager,
    IbanManager* iban_manager,
    MerchantPromoCodeManager* merchant_promo_code_manager)
    : autocomplete_history_manager_(CHECK_DEREF(autocomplete_history_manager)),
      iban_manager_(iban_manager),
      merchant_promo_code_manager_(merchant_promo_code_manager) {}

SingleFieldFillRouter::~SingleFieldFillRouter() = default;

void SingleFieldFillRouter::OnWillSubmitForm(
    const FormData& form,
    const FormStructure* form_structure,
    bool is_autocomplete_enabled) {
  CHECK(!form_structure ||
        form.fields().size() == form_structure->field_count());
  std::vector<FormFieldData> autocomplete_fields;
  for (size_t i = 0; i < form.fields().size(); i++) {
    // If |form_structure| is present, then the fields in |form_structure| and
    // the fields in |form| are 1:1. |form_structure| not being present
    // indicates we may have fields that were not able to be parsed, so we route
    // them to autocomplete functionality by default.
    bool skip_because_promo_code =
        merchant_promo_code_manager_ && form_structure &&
        form_structure->field(i)->Type().GetStorableType() ==
            MERCHANT_PROMO_CODE;
    bool skip_because_iban =
        iban_manager_ && form_structure &&
        form_structure->field(i)->Type().GetStorableType() == IBAN_VALUE;
    if (!skip_because_iban && !skip_because_promo_code) {
      autocomplete_fields.push_back(form.fields()[i]);
    }
  }
  autocomplete_history_manager_->OnWillSubmitFormWithFields(
      autocomplete_fields, is_autocomplete_enabled);
}

bool SingleFieldFillRouter::OnGetSingleFieldSuggestions(
    const FormStructure* form_structure,
    const FormFieldData& field,
    const AutofillField* autofill_field,
    const AutofillClient& client,
    SingleFieldFillRouter::OnSuggestionsReturnedCallback
        on_suggestions_returned) {
  // Retrieving suggestions for a new field; select the appropriate filler.
  if (merchant_promo_code_manager_ && form_structure && autofill_field &&
      merchant_promo_code_manager_->OnGetSingleFieldSuggestions(
          *form_structure, field, *autofill_field, client,
          on_suggestions_returned)) {
    return true;
  }
  if (iban_manager_ && autofill_field &&
      iban_manager_->OnGetSingleFieldSuggestions(field, *autofill_field, client,
                                                 on_suggestions_returned)) {
    return true;
  }
  return autocomplete_history_manager_->OnGetSingleFieldSuggestions(
      field, client, on_suggestions_returned);
}

void SingleFieldFillRouter::CancelPendingQueries() {
  autocomplete_history_manager_->CancelPendingQueries();
}

void SingleFieldFillRouter::OnRemoveCurrentSingleFieldSuggestion(
    const std::u16string& field_name,
    const std::u16string& value,
    SuggestionType type) {
  if (type == SuggestionType::kAutocompleteEntry) {
    autocomplete_history_manager_->OnRemoveCurrentSingleFieldSuggestion(
        field_name, value, type);
  }
}

void SingleFieldFillRouter::OnSingleFieldSuggestionSelected(
    const Suggestion& suggestion) {
  SuggestionType type = suggestion.type;
  if (merchant_promo_code_manager_ &&
      (type == SuggestionType::kMerchantPromoCodeEntry ||
       type == SuggestionType::kSeePromoCodeDetails)) {
    merchant_promo_code_manager_->OnSingleFieldSuggestionSelected(suggestion);
  } else if (iban_manager_ && type == SuggestionType::kIbanEntry) {
    iban_manager_->OnSingleFieldSuggestionSelected(suggestion);
  } else if (type == SuggestionType::kAutocompleteEntry) {
    autocomplete_history_manager_->OnSingleFieldSuggestionSelected(suggestion);
  }
}

}  // namespace autofill
