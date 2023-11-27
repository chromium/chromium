// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/iban_manager.h"

#include "base/containers/contains.h"
#include "components/autofill/core/browser/autofill_optimization_guide.h"
#include "components/autofill/core/browser/autofill_suggestion_generator.h"
#include "components/autofill/core/browser/browser_autofill_manager.h"
#include "components/autofill/core/browser/metrics/payments/iban_metrics.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/browser/suggestions_context.h"
#include "components/autofill/core/common/autofill_clock.h"

namespace autofill {

namespace {

constexpr int kFieldLengthLimitOnServerIbanSuggestion = 6;

}  // namespace

IbanManager::IbanManager(PersonalDataManager* personal_data_manager)
    : personal_data_manager_(personal_data_manager) {}

IbanManager::~IbanManager() = default;

bool IbanManager::OnGetSingleFieldSuggestions(
    AutofillSuggestionTriggerSource trigger_source,
    const FormFieldData& field,
    const AutofillClient& client,
    OnSuggestionsReturnedCallback on_suggestions_returned,
    const SuggestionsContext& context) {
  // The field is eligible only if it's focused on an IBAN field.
  AutofillField* focused_field = context.focused_field;
  bool field_is_eligible =
      focused_field && focused_field->Type().GetStorableType() == IBAN_VALUE;
  if (!field_is_eligible) {
    return false;
  }

  if (!personal_data_manager_ ||
      !personal_data_manager_->IsAutofillPaymentMethodsEnabled()) {
    return false;
  }

  std::vector<const Iban*> ibans = personal_data_manager_->GetIbansToSuggest();
  if (ibans.empty()) {
    return false;
  }

  // AutofillOptimizationGuide will not be present on unsupported platforms.
  if (auto* autofill_optimization_guide =
          client.GetAutofillOptimizationGuide()) {
    if (autofill_optimization_guide->ShouldBlockSingleFieldSuggestions(
            client.GetLastCommittedPrimaryMainFrameOrigin().GetURL(),
            focused_field)) {
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

  // Rank the IBANs by ranking score (see AutoFillDataModel for details).
  base::ranges::sort(ibans, [comparison_time = AutofillClock::Now()](
                                const Iban* iban0, const Iban* iban1) {
    return iban0->HasGreaterRankingThan(iban1, comparison_time);
  });
  SendIbanSuggestions(std::move(ibans), field,
                      std::move(on_suggestions_returned), trigger_source);

  return true;
}

void IbanManager::OnSingleFieldSuggestionSelected(const std::u16string& value,
                                                  PopupItemId popup_item_id) {
  uma_recorder_.OnIbanSuggestionSelected();
}

base::WeakPtr<IbanManager> IbanManager::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void IbanManager::UmaRecorder::OnIbanSuggestionsShown(
    FieldGlobalId field_global_id) {
  // Log metrics related to the IBAN-related suggestions in the popup.
  autofill_metrics::LogIndividualIbanSuggestionsEvent(
      autofill_metrics::IbanSuggestionsEvent::kIbanSuggestionsShown);
  if (most_recent_suggestions_shown_field_global_id_ != field_global_id) {
    autofill_metrics::LogIndividualIbanSuggestionsEvent(
        autofill_metrics::IbanSuggestionsEvent::kIbanSuggestionsShownOnce);
  }

  most_recent_suggestions_shown_field_global_id_ = field_global_id;
}

void IbanManager::UmaRecorder::OnIbanSuggestionSelected() {
  // We log every time the IBAN suggestion is selected.
  autofill_metrics::LogIndividualIbanSuggestionsEvent(
      autofill_metrics::IbanSuggestionsEvent::kIbanSuggestionSelected);
  if (most_recent_suggestion_selected_field_global_id_ !=
      most_recent_suggestions_shown_field_global_id_) {
    autofill_metrics::LogIndividualIbanSuggestionsEvent(
        autofill_metrics::IbanSuggestionsEvent::kIbanSuggestionSelectedOnce);
  }

  most_recent_suggestion_selected_field_global_id_ =
      most_recent_suggestions_shown_field_global_id_;
}

void IbanManager::SendIbanSuggestions(
    std::vector<const Iban*> ibans,
    const FormFieldData& field,
    OnSuggestionsReturnedCallback on_suggestions_returned,
    AutofillSuggestionTriggerSource trigger_source) {
  // If the input box content equals any of the available IBANs, then
  // assume the IBAN has been filled, and don't show any suggestions.
  if (!field.value.empty() &&
      base::Contains(ibans, field.value, &Iban::value)) {
    return;
  }

  FilterIbansToSuggest(field.value, ibans);

  if (ibans.empty()) {
    return;
  }

  std::move(on_suggestions_returned)
      .Run(field.global_id(), trigger_source,
           AutofillSuggestionGenerator::GetSuggestionsForIbans(ibans));

  uma_recorder_.OnIbanSuggestionsShown(field.global_id());
}

void IbanManager::FilterIbansToSuggest(const std::u16string& field_value,
                                       std::vector<const Iban*>& ibans) {
  std::erase_if(ibans, [&](const Iban* iban) {
    if (iban->record_type() == Iban::kLocalIban) {
      return !base::StartsWith(iban->value(), field_value);
    } else {
      CHECK_EQ(iban->record_type(), Iban::kServerIban);
      if (iban->prefix().empty()) {
        return field_value.length() >= kFieldLengthLimitOnServerIbanSuggestion;
      } else {
        return !(iban->prefix().starts_with(field_value) ||
                 field_value.starts_with(iban->prefix()));
      }
    }
  });
}

}  // namespace autofill
