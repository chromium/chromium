// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/single_field_form_fill_router.h"

namespace autofill {

SingleFieldFormFillRouter::SingleFieldFormFillRouter(
    AutocompleteHistoryManager* autocomplete_history_manager) {
  autocomplete_history_manager_ = autocomplete_history_manager->GetWeakPtr();

  // Defaults to the |autocomplete_history_manager_| as the current filler upon
  // construction. This is generally the case anyway, but doing so explicitly
  // keeps unit tests happy who don't first call |OnGetSingleFieldSuggestions|.
  // TODO(crbug.com/1245457): Is this the best approach? Or change the tests?
  current_single_field_form_filler_ = autocomplete_history_manager_;
}

SingleFieldFormFillRouter::~SingleFieldFormFillRouter() = default;

void SingleFieldFormFillRouter::OnGetSingleFieldSuggestions(
    int query_id,
    bool is_autocomplete_enabled,
    bool autoselect_first_suggestion,
    const std::u16string& name,
    const std::u16string& prefix,
    const std::string& form_control_type,
    base::WeakPtr<SingleFieldFormFiller::SuggestionsHandler> handler) {
  // Retrieving suggestions for a new field; select the appropriate filler.
  // NOTE: All single field form filling is currently handled by Autocomplete.
  //       This will soon be extended to merchant promo codes as well.
  current_single_field_form_filler_ = autocomplete_history_manager_;

  current_single_field_form_filler_->OnGetSingleFieldSuggestions(
      query_id, is_autocomplete_enabled, autoselect_first_suggestion, name,
      prefix, form_control_type, handler);
}

void SingleFieldFormFillRouter::OnWillSubmitForm(const FormData& form,
                                                 bool is_autocomplete_enabled) {
  current_single_field_form_filler_->OnWillSubmitForm(form,
                                                      is_autocomplete_enabled);
}

void SingleFieldFormFillRouter::CancelPendingQueries(
    const SingleFieldFormFiller::SuggestionsHandler* handler) {
  if (current_single_field_form_filler_)
    current_single_field_form_filler_->CancelPendingQueries(handler);
}

void SingleFieldFormFillRouter::OnRemoveCurrentSingleFieldSuggestion(
    const std::u16string& field_name,
    const std::u16string& value) {
  current_single_field_form_filler_->OnRemoveCurrentSingleFieldSuggestion(
      field_name, value);
}

void SingleFieldFormFillRouter::OnSingleFieldSuggestionSelected(
    const std::u16string& value) {
  current_single_field_form_filler_->OnSingleFieldSuggestionSelected(value);
}

}  // namespace autofill
