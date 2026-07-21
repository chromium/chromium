// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/single_field_fillers/single_field_fill_router.h"

#include <stddef.h>

#include <string>
#include <vector>

#include "base/check.h"
#include "base/check_deref.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/payments/iban_manager.h"
#include "components/autofill/core/browser/single_field_fillers/autocomplete/autocomplete_history_manager.h"
#include "components/autofill/core/browser/single_field_fillers/payments/merchant_promo_code_manager.h"
#include "components/autofill/core/browser/suggestions/suggestion.h"
#include "components/autofill/core/browser/suggestions/suggestion_type.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/form_field_data.h"

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
  autocomplete_history_manager_->OnWillSubmitFormWithFields(
      form.fields(), form_structure, is_autocomplete_enabled);
  if (iban_manager_) {
    iban_manager_->OnWillSubmitFormWithFields();
  }
}

void SingleFieldFillRouter::CancelPendingQueries() {
  autocomplete_history_manager_->CancelPendingQuery();
}

void SingleFieldFillRouter::OnRemoveCurrentSingleFieldSuggestion(
    const std::u16string& field_name,
    const std::u16string& field_label,
    const std::u16string& value,
    SuggestionType type) {
  if (type == SuggestionType::kAutocompleteEntry) {
    autocomplete_history_manager_->OnRemoveCurrentSingleFieldSuggestion(
        field_name, field_label, value, type);
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
    // TODO(crbug.com/507313423): Reimplement this metric.
    if (!base::FeatureList::IsEnabled(
            features::kAutofillLabelSensitiveAutocomplete)) {
      autocomplete_history_manager_->OnSingleFieldSuggestionSelected(
          suggestion);
    }
  }
}

}  // namespace autofill
