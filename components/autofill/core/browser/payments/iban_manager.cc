// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/payments/iban_manager.h"

#include "base/containers/contains.h"
#include "components/autofill/core/browser/autofill_optimization_guide.h"
#include "components/autofill/core/browser/browser_autofill_manager.h"
#include "components/autofill/core/browser/metrics/payments/iban_metrics.h"
#include "components/autofill/core/browser/payments_suggestion_generator.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/common/autofill_clock.h"

namespace autofill {

using autofill_metrics::IbanSuggestionsEvent;

namespace {

constexpr int kFieldLengthLimitOnServerIbanSuggestion = 6;

}  // namespace

IbanManager::IbanManager(PersonalDataManager* personal_data_manager)
    : personal_data_manager_(personal_data_manager) {}

IbanManager::~IbanManager() = default;

bool IbanManager::OnGetSingleFieldSuggestions(
    const FormStructure* form_structure,
    const FormFieldData& field,
    const AutofillField* autofill_field,
    const AutofillClient& client,
    OnSuggestionsReturnedCallback on_suggestions_returned) {
  // The field is eligible only if it's focused on an IBAN field.
  if (!autofill_field ||
      autofill_field->Type().GetStorableType() != IBAN_VALUE) {
    return false;
  }

  if (!personal_data_manager_ ||
      !personal_data_manager_->payments_data_manager()
           .IsAutofillPaymentMethodsEnabled()) {
    return false;
  }

  std::vector<Iban> ibans = personal_data_manager_->payments_data_manager()
                                .GetOrderedIbansToSuggest();
  if (ibans.empty()) {
    return false;
  }

  // AutofillOptimizationGuide will not be present on unsupported platforms.
  if (auto* autofill_optimization_guide =
          client.GetAutofillOptimizationGuide()) {
    if (autofill_optimization_guide->ShouldBlockSingleFieldSuggestions(
            client.GetLastCommittedPrimaryMainFrameOrigin().GetURL(),
            autofill_field)) {
      autofill_metrics::LogIbanSuggestionBlockListStatusMetric(
          autofill_metrics::IbanSuggestionBlockListStatus::kBlocked);
      return false;
    }
    autofill_metrics::LogIbanSuggestionBlockListStatusMetric(
        autofill_metrics::IbanSuggestionBlockListStatus::kAllowed);
  } else {
    autofill_metrics::LogIbanSuggestionBlockListStatusMetric(
        autofill_metrics::IbanSuggestionBlockListStatus::
            kBlocklistIsNotAvailable);
  }

  std::move(on_suggestions_returned)
      .Run(field.global_id(), GetIbanSuggestions(std::move(ibans), field));
  return true;
}

void IbanManager::OnSingleFieldSuggestionSelected(
    const Suggestion& suggestion) {
  uma_recorder_.OnIbanSuggestionSelected(suggestion);
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
  bool is_local_iban = absl::holds_alternative<Suggestion::Guid>(
      suggestion.GetPayload<Suggestion::BackendId>());
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

std::vector<Suggestion> IbanManager::GetIbanSuggestions(
    std::vector<Iban> ibans,
    const FormFieldData& field) {
  // If the input box content equals any of the available IBANs, then
  // assume the IBAN has been filled, and don't show any suggestions.
  if (!field.value().empty() &&
      base::Contains(ibans, field.value(), &Iban::value)) {
    return {};
  }

  FilterIbansToSuggest(field.value(), ibans);

  if (ibans.empty()) {
    return {};
  }

  uma_recorder_.OnIbanSuggestionsShown(field.global_id());
  return GetSuggestionsForIbans(ibans);
}

void IbanManager::FilterIbansToSuggest(const std::u16string& field_value,
                                       std::vector<Iban>& ibans) {
  std::erase_if(ibans, [&](const Iban& iban) {
    if (iban.record_type() == Iban::kLocalIban) {
      return !base::StartsWith(iban.value(), field_value);
    } else {
      CHECK_EQ(iban.record_type(), Iban::kServerIban);
      if (iban.prefix().empty()) {
        return field_value.length() >= kFieldLengthLimitOnServerIbanSuggestion;
      } else {
        return !(iban.prefix().starts_with(field_value) ||
                 field_value.starts_with(iban.prefix()));
      }
    }
  });
}

}  // namespace autofill
