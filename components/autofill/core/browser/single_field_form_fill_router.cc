// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/single_field_form_fill_router.h"

#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/suggestions_context.h"

namespace autofill {

SingleFieldFormFillRouter::SingleFieldFormFillRouter(
    AutocompleteHistoryManager* autocomplete_history_manager,
    IbanManager* iban_manager,
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
    AutofillSuggestionTriggerSource trigger_source,
    const FormFieldData& field,
    const AutofillClient& client,
    OnSuggestionsReturnedCallback on_suggestions_returned,
    const SuggestionsContext& context) {
  // Retrieving suggestions for a new field; select the appropriate filler.
  if (merchant_promo_code_manager_ &&
      merchant_promo_code_manager_->OnGetSingleFieldSuggestions(
          trigger_source, field, client, on_suggestions_returned, context)) {
    return true;
  }
  if (iban_manager_ &&
      iban_manager_->OnGetSingleFieldSuggestions(
          trigger_source, field, client, on_suggestions_returned, context)) {
    return true;
  }
  return autocomplete_history_manager_->OnGetSingleFieldSuggestions(
      trigger_source, field, client, std::move(on_suggestions_returned),
      context);
}

void SingleFieldFormFillRouter::OnWillSubmitFormWithFields(
    const std::vector<FormFieldData>& fields,
    bool is_autocomplete_enabled) {}

void SingleFieldFormFillRouter::CancelPendingQueries() {
  if (autocomplete_history_manager_) {
    autocomplete_history_manager_->CancelPendingQueries();
  }
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
    PopupItemId popup_item_id) {
  if (merchant_promo_code_manager_ &&
      popup_item_id == PopupItemId::kMerchantPromoCodeEntry) {
    merchant_promo_code_manager_->OnRemoveCurrentSingleFieldSuggestion(
        field_name, value, popup_item_id);
  } else if (iban_manager_ && popup_item_id == PopupItemId::kIbanEntry) {
    iban_manager_->OnRemoveCurrentSingleFieldSuggestion(field_name, value,
                                                        popup_item_id);
  } else {
    autocomplete_history_manager_->OnRemoveCurrentSingleFieldSuggestion(
        field_name, value, popup_item_id);
  }
}

void SingleFieldFormFillRouter::OnSingleFieldSuggestionSelected(
    const std::u16string& value,
    PopupItemId popup_item_id) {
  if (merchant_promo_code_manager_ &&
      (popup_item_id == PopupItemId::kMerchantPromoCodeEntry ||
       popup_item_id == PopupItemId::kSeePromoCodeDetails)) {
    merchant_promo_code_manager_->OnSingleFieldSuggestionSelected(
        value, popup_item_id);
  } else if (iban_manager_ && popup_item_id == PopupItemId::kIbanEntry) {
    iban_manager_->OnSingleFieldSuggestionSelected(value, popup_item_id);
  } else {
    autocomplete_history_manager_->OnSingleFieldSuggestionSelected(
        value, popup_item_id);
  }
}

}  // namespace autofill
