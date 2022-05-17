// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/single_field_form_fill_router.h"

#include "components/autofill/core/browser/suggestions_context.h"
#include "components/autofill/core/common/autofill_payments_features.h"

namespace autofill {

SingleFieldFormFillRouter::SingleFieldFormFillRouter(
    AutocompleteHistoryManager* autocomplete_history_manager) {
  autocomplete_history_manager_ = autocomplete_history_manager->GetWeakPtr();
}

SingleFieldFormFillRouter::~SingleFieldFormFillRouter() = default;

void SingleFieldFormFillRouter::OnWillSubmitForm(const FormData& form,
                                                 bool is_autocomplete_enabled) {
  autocomplete_history_manager_->OnWillSubmitFormWithFields(
      form.fields, is_autocomplete_enabled);
}

void SingleFieldFormFillRouter::OnGetSingleFieldSuggestions(
    int query_id,
    bool is_autocomplete_enabled,
    bool autoselect_first_suggestion,
    const std::u16string& name,
    const std::u16string& prefix,
    const std::string& form_control_type,
    base::WeakPtr<SingleFieldFormFiller::SuggestionsHandler> handler,
    const SuggestionsContext& context) {
  autocomplete_history_manager_->OnGetSingleFieldSuggestions(
      query_id, is_autocomplete_enabled, autoselect_first_suggestion, name,
      prefix, form_control_type, handler, context);
}

void SingleFieldFormFillRouter::OnWillSubmitFormWithFields(
    const std::vector<FormFieldData>& fields,
    bool is_autocomplete_enabled) {}

void SingleFieldFormFillRouter::CancelPendingQueries(
    const SingleFieldFormFiller::SuggestionsHandler* handler) {
  if (autocomplete_history_manager_)
    autocomplete_history_manager_->CancelPendingQueries(handler);
}

void SingleFieldFormFillRouter::OnRemoveCurrentSingleFieldSuggestion(
    const std::u16string& field_name,
    const std::u16string& value,
    int frontend_id) {
  autocomplete_history_manager_->OnRemoveCurrentSingleFieldSuggestion(
      field_name, value, frontend_id);
}

void SingleFieldFormFillRouter::OnSingleFieldSuggestionSelected(
    const std::u16string& value,
    int frontend_id) {
  autocomplete_history_manager_->OnSingleFieldSuggestionSelected(value,
                                                                 frontend_id);
}

}  // namespace autofill
