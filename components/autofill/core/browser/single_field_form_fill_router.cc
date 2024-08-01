// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/single_field_form_fill_router.h"

#include "base/check_deref.h"
#include "components/autofill/core/browser/form_structure.h"

namespace autofill {

SingleFieldFormFillRouter::SingleFieldFormFillRouter(
    AutocompleteHistoryManager* autocomplete_history_manager,
    IbanManager* iban_manager,
    MerchantPromoCodeManager* merchant_promo_code_manager)
    : autocomplete_history_manager_(CHECK_DEREF(autocomplete_history_manager)),
      iban_manager_(iban_manager),
      merchant_promo_code_manager_(merchant_promo_code_manager) {}

SingleFieldFormFillRouter::~SingleFieldFormFillRouter() = default;

void SingleFieldFormFillRouter::OnWillSubmitForm(
    const FormData& form,
    const FormStructure* form_structure,
    bool is_autocomplete_enabled) {
  if (form_structure)
    DCHECK(form.fields().size() == form_structure->field_count());
  std::vector<FormFieldData> autocomplete_fields;
  std::vector<FormFieldData> iban_fields;
  std::vector<FormFieldData> merchant_promo_code_fields;
  for (size_t i = 0; i < form.fields().size(); i++) {
    // If |form_structure| is present, then the fields in |form_structure| and
    // the fields in |form| should be 1:1. |form_structure| not being present
    // indicates we may have fields that were not able to be parsed, so we route
    // them to autocomplete functionality by default.
    if (merchant_promo_code_manager_ && form_structure &&
        form_structure->field(i)->Type().GetStorableType() ==
            MERCHANT_PROMO_CODE) {
      merchant_promo_code_fields.push_back(form.fields()[i]);
    } else if (iban_manager_ && form_structure &&
               form_structure->field(i)->Type().GetStorableType() ==
                   IBAN_VALUE) {
      iban_fields.push_back(form.fields()[i]);
    } else {
      autocomplete_fields.push_back(form.fields()[i]);
    }
  }

  if (merchant_promo_code_manager_) {
    merchant_promo_code_manager_->OnWillSubmitFormWithFields(
        merchant_promo_code_fields, is_autocomplete_enabled);
  }
  if (iban_manager_) {
    iban_manager_->OnWillSubmitFormWithFields(iban_fields,
                                              is_autocomplete_enabled);
  }
  autocomplete_history_manager_->OnWillSubmitFormWithFields(
      autocomplete_fields, is_autocomplete_enabled);
}

bool SingleFieldFormFillRouter::OnGetSingleFieldSuggestions(
    const FormStructure* form_structure,
    const FormFieldData& field,
    const AutofillField* autofill_field,
    const AutofillClient& client,
    OnSuggestionsReturnedCallback on_suggestions_returned) {
  // Retrieving suggestions for a new field; select the appropriate filler.
  if (merchant_promo_code_manager_ &&
      merchant_promo_code_manager_->OnGetSingleFieldSuggestions(
          form_structure, field, autofill_field, client,
          on_suggestions_returned)) {
    return true;
  }
  if (iban_manager_ && iban_manager_->OnGetSingleFieldSuggestions(
                           form_structure, field, autofill_field, client,
                           on_suggestions_returned)) {
    return true;
  }
  return autocomplete_history_manager_->OnGetSingleFieldSuggestions(
      form_structure, field, autofill_field, client,
      std::move(on_suggestions_returned));
}

void SingleFieldFormFillRouter::OnWillSubmitFormWithFields(
    const std::vector<FormFieldData>& fields,
    bool is_autocomplete_enabled) {}

void SingleFieldFormFillRouter::CancelPendingQueries() {
  autocomplete_history_manager_->CancelPendingQueries();
  if (merchant_promo_code_manager_) {
    merchant_promo_code_manager_->CancelPendingQueries();
  }
  if (iban_manager_) {
    iban_manager_->CancelPendingQueries();
  }
}

void SingleFieldFormFillRouter::OnRemoveCurrentSingleFieldSuggestion(
    const std::u16string& field_name,
    const std::u16string& value,
    SuggestionType type) {
  if (merchant_promo_code_manager_ &&
      type == SuggestionType::kMerchantPromoCodeEntry) {
    merchant_promo_code_manager_->OnRemoveCurrentSingleFieldSuggestion(
        field_name, value, type);
  } else if (iban_manager_ && type == SuggestionType::kIbanEntry) {
    iban_manager_->OnRemoveCurrentSingleFieldSuggestion(field_name, value,
                                                        type);
  } else if (type == SuggestionType::kAutocompleteEntry) {
    autocomplete_history_manager_->OnRemoveCurrentSingleFieldSuggestion(
        field_name, value, type);
  }
}

void SingleFieldFormFillRouter::OnSingleFieldSuggestionSelected(
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
