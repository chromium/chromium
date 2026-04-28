// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/payments/iban_manager.h"

#include <variant>

#include "components/autofill/core/browser/data_manager/payments/payments_data_manager.h"
#include "components/autofill/core/browser/foundations/browser_autofill_manager.h"
#include "components/autofill/core/browser/metrics/payments/iban_metrics.h"
#include "components/autofill/core/browser/suggestions/payments/iban_suggestion_generator.h"
#include "components/autofill/core/browser/suggestions/payments/payments_suggestion_generator_util.h"
#include "components/autofill/core/browser/suggestions/suggestion_generator.h"
#include "components/autofill/core/common/autofill_clock.h"

namespace autofill {

namespace {

using autofill_metrics::IbanSuggestionsEvent;

}  // namespace

IbanManager::IbanManager(PaymentsDataManager* payments_data_manager)
    : payments_data_manager_(payments_data_manager) {}

void IbanManager::LogIbanFormFilled() {
  if (suggestion_status_ == SuggestionStatus::kSelected) {
    suggestion_status_ = SuggestionStatus::kFilled;
    autofill_metrics::LogIbanFormEvent(
        is_local_iban_suggestion_selected_
            ? autofill_metrics::IbanFormEvent::kLocalIbanFilled
            : autofill_metrics::IbanFormEvent::kServerIbanFilled);
  }
}

void IbanManager::OnWillSubmitFormWithFields() {
  if (suggestion_status_ == SuggestionStatus::kFilled) {
    suggestion_status_ = SuggestionStatus::kFormSubmitted;
    autofill_metrics::LogIbanFormEvent(
        autofill_metrics::IbanFormEvent::kFormSubmitted);
  }
}

bool IbanManager::OnGetSingleFieldSuggestions(
    const FormStructure& form,
    const FormFieldData& field,
    const AutofillField& autofill_field,
    const AutofillClient& client,
    SingleFieldFillRouter::OnSuggestionsReturnedCallback&
        on_suggestions_returned) {
  IbanSuggestionGenerator iban_suggestion_generator;
  bool suggestions_generated = false;

  auto on_suggestions_generated =
      [&on_suggestions_returned, &field, &suggestions_generated](
          SuggestionGenerator::ReturnedSuggestions returned_suggestions) {
        suggestions_generated = !returned_suggestions.second.empty();
        if (suggestions_generated) {
          std::move(on_suggestions_returned)
              .Run(field.global_id(), std::move(returned_suggestions.second));
        }
      };

  // Since the `on_suggestions_generated` callback is called synchronously,
  // we can assume that `suggestions_generated` will hold the correct value.
  iban_suggestion_generator.GenerateSuggestions(form.ToFormData(), field, &form,
                                                &autofill_field, client,
                                                on_suggestions_generated);
  return suggestions_generated;
}

void IbanManager::OnSingleFieldSuggestionSelected(
    const Suggestion& suggestion) {
  uma_recorder_.OnIbanSuggestionSelected(suggestion);
  if (suggestion_status_ == SuggestionStatus::kShown) {
    suggestion_status_ = SuggestionStatus::kSelected;
    is_local_iban_suggestion_selected_ =
        std::holds_alternative<Suggestion::Guid>(suggestion.payload);
    if (!is_local_iban_suggestion_selected_) {
      autofill_metrics::LogIbanFormEvent(
          autofill_metrics::IbanFormEvent::kServerIbanSelected);
    }
  }
}

void IbanManager::OnIbanSuggestionsShown(FieldGlobalId field_global_id) {
  if (suggestion_status_ == SuggestionStatus::kNotShown) {
    suggestion_status_ = SuggestionStatus::kShown;
    autofill_metrics::LogIbanFormEvent(
        autofill_metrics::IbanFormEvent::kSuggestionShown);
  }
  uma_recorder_.OnIbanSuggestionsShown(field_global_id);
}

void IbanManager::UmaRecorder::OnIbanSuggestionsShown(
    FieldGlobalId field_global_id) {
  // Log metrics related to the IBAN-related suggestions in the popup.
  autofill_metrics::LogIndividualIbanSuggestionsEvent(
      IbanSuggestionsEvent::kIbanSuggestionsShown);
  if (most_recent_suggestions_shown_field_global_id_ != field_global_id) {
    autofill_metrics::LogIndividualIbanSuggestionsEvent(
        IbanSuggestionsEvent::kIbanSuggestionsShownOnce);
  }

  most_recent_suggestions_shown_field_global_id_ = field_global_id;
}

void IbanManager::UmaRecorder::OnIbanSuggestionSelected(
    const Suggestion& suggestion) {
  autofill_metrics::LogIbanSelectedCountry(
      Iban::GetCountryCode(suggestion.main_text.value));
  bool is_local_iban =
      std::holds_alternative<Suggestion::Guid>(suggestion.payload);
  // We log every time the IBAN suggestion is selected.
  autofill_metrics::LogIndividualIbanSuggestionsEvent(
      is_local_iban ? IbanSuggestionsEvent::kLocalIbanSuggestionSelected
                    : IbanSuggestionsEvent::kServerIbanSuggestionSelected);
  if (most_recent_suggestion_selected_field_global_id_ !=
      most_recent_suggestions_shown_field_global_id_) {
    autofill_metrics::LogIndividualIbanSuggestionsEvent(
        is_local_iban
            ? IbanSuggestionsEvent::kLocalIbanSuggestionSelectedOnce
            : IbanSuggestionsEvent::kServerIbanSuggestionSelectedOnce);
  }

  most_recent_suggestion_selected_field_global_id_ =
      most_recent_suggestions_shown_field_global_id_;
}

}  // namespace autofill
