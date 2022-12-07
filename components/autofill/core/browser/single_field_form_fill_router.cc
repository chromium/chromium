// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/single_field_form_fill_router.h"

#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/suggestions_context.h"

namespace autofill {

SingleFieldFormFillRouter::SingleFieldFormFillRouter(
    AutocompleteHistoryManager* autocomplete_history_manager,
    IBANManager* iban_manager,
    MerchantPromoCodeManager* merchant_promo_code_manager)
    : autocomplete_history_manager_(autocomplete_history_manager->GetWeakPtr()),
      iban_manager_(iban_manager ? iban_manager->GetWeakPtr() : nullptr),
      merchant_promo_code_manager_(
          merchant_promo_code_manager
              ? merchant_promo_code_manager->GetWeakPtr()
              : nullptr) {}

SingleFieldFormFillRouter::~SingleFieldFormFillRouter() = default;

void SingleFieldFormFillRouter::OnWillSubmitForm(
    const FormData& form,
    const FormStructure* form_structure,
    bool is_autocomplete_enabled) {
  if (form_structure)
    DCHECK(form.fields.size() == form_structure->field_count());
  std::vector<FormFieldData> autocomplete_fields;
  std::vector<FormFieldData> iban_fields;
  std::vector<FormFieldData> merchant_promo_code_fields;
  for (size_t i = 0; i < form.fields.size(); i++) {
    // If |form_structure| is present, then the fields in |form_structure| and
    // the fields in |form| should be 1:1. |form_structure| not being present
    // indicates we may have fields that were not able to be parsed, so we route
    // them to autocomplete functionality by default.
    if (merchant_promo_code_manager_ && form_structure &&
        form_structure->field(i)->Type().GetStorableType() ==
            MERCHANT_PROMO_CODE) {
      merchant_promo_code_fields.push_back(form.fields[i]);
    } else if (iban_manager_ && form_structure &&
               form_structure->field(i)->Type().GetStorableType() ==
                   IBAN_VALUE) {
      iban_fields.push_back(form.fields[i]);
    } else {
      autocomplete_fields.push_back(form.fields[i]);
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
    AutoselectFirstSuggestion autoselect_first_suggestion,
    const FormFieldData& field,
    const AutofillClient& client,
    base::WeakPtr<SingleFieldFormFiller::SuggestionsHandler> handler,
    const SuggestionsContext& context) {
  // Retrieving suggestions for a new field; select the appropriate filler.
  if (merchant_promo_code_manager_ &&
      merchant_promo_code_manager_->OnGetSingleFieldSuggestions(
          autoselect_first_suggestion, field, client, handler, context)) {
    return true;
  }
  if (iban_manager_ &&
      iban_manager_->OnGetSingleFieldSuggestions(
          autoselect_first_suggestion, field, client, handler, context)) {
    return true;
  }
  if (autocomplete_history_manager_->OnGetSingleFieldSuggestions(
          autoselect_first_suggestion, field, client, handler, context)) {
    return true;
  }
  return false;
}

void SingleFieldFormFillRouter::OnWillSubmitFormWithFields(
    const std::vector<FormFieldData>& fields,
    bool is_autocomplete_enabled) {}

void SingleFieldFormFillRouter::CancelPendingQueries(
    const SingleFieldFormFiller::SuggestionsHandler* handler) {
  if (autocomplete_history_manager_)
    autocomplete_history_manager_->CancelPendingQueries(handler);
  if (merchant_promo_code_manager_)
    merchant_promo_code_manager_->CancelPendingQueries(handler);
  if (iban_manager_)
    iban_manager_->CancelPendingQueries(handler);
}

void SingleFieldFormFillRouter::OnRemoveCurrentSingleFieldSuggestion(
    const std::u16string& field_name,
    const std::u16string& value,
    int frontend_id) {
  if (merchant_promo_code_manager_ &&
      frontend_id == POPUP_ITEM_ID_MERCHANT_PROMO_CODE_ENTRY) {
    merchant_promo_code_manager_->OnRemoveCurrentSingleFieldSuggestion(
        field_name, value, frontend_id);
  } else if (iban_manager_ && frontend_id == POPUP_ITEM_ID_IBAN_ENTRY) {
    iban_manager_->OnRemoveCurrentSingleFieldSuggestion(field_name, value,
                                                        frontend_id);
  } else {
    autocomplete_history_manager_->OnRemoveCurrentSingleFieldSuggestion(
        field_name, value, frontend_id);
  }
}

void SingleFieldFormFillRouter::OnSingleFieldSuggestionSelected(
    const std::u16string& value,
    int frontend_id) {
  if (merchant_promo_code_manager_ &&
      (frontend_id == POPUP_ITEM_ID_MERCHANT_PROMO_CODE_ENTRY ||
       frontend_id == POPUP_ITEM_ID_SEE_PROMO_CODE_DETAILS)) {
    merchant_promo_code_manager_->OnSingleFieldSuggestionSelected(value,
                                                                  frontend_id);
  } else if (iban_manager_ && frontend_id == POPUP_ITEM_ID_IBAN_ENTRY) {
    iban_manager_->OnSingleFieldSuggestionSelected(value, frontend_id);
  } else {
    autocomplete_history_manager_->OnSingleFieldSuggestionSelected(value,
                                                                   frontend_id);
  }
}

}  // namespace autofill
