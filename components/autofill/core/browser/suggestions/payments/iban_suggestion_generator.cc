// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/suggestions/payments/iban_suggestion_generator.h"

#include "base/containers/contains.h"
#include "base/containers/to_vector.h"
#include "base/functional/function_ref.h"
#include "base/strings/string_util.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/data_manager/payments/payments_data_manager.h"
#include "components/autofill/core/browser/integrators/optimization_guide/autofill_optimization_guide_decider.h"
#include "components/autofill/core/browser/payments/iban_manager.h"
#include "components/autofill/core/browser/suggestions/payments/payments_suggestion_generator.h"

namespace autofill {
namespace {
// The server-based IBAN suggestions will be returned if the IBAN's prefix is
// absent and the length of the input field is less than
// `kFieldLengthLimitOnServerIbanSuggestion` characters.
constexpr int kFieldLengthLimitOnServerIbanSuggestion = 6;
}  // namespace

IbanSuggestionGenerator::IbanSuggestionGenerator() = default;
IbanSuggestionGenerator::~IbanSuggestionGenerator() = default;

void IbanSuggestionGenerator::FetchSuggestionData(
    const FormData& form,
    const FormFieldData& trigger_field,
    const FormStructure* form_structure,
    const AutofillField* trigger_autofill_field,
    const AutofillClient& client,
    base::OnceCallback<
        void(std::pair<SuggestionDataSource,
                       std::vector<SuggestionGenerator::SuggestionData>>)>
        callback) {
  FetchSuggestionData(
      form, trigger_field, form_structure, trigger_autofill_field, client,
      [&callback](std::pair<SuggestionDataSource,
                            std::vector<SuggestionGenerator::SuggestionData>>
                      suggestion_data) {
        std::move(callback).Run(std::move(suggestion_data));
      });
}

void IbanSuggestionGenerator::GenerateSuggestions(
    const FormData& form,
    const FormFieldData& trigger_field,
    const FormStructure* form_structure,
    const AutofillField* trigger_autofill_field,
    const AutofillClient& client,
    const base::flat_map<SuggestionDataSource, std::vector<SuggestionData>>&
        all_suggestion_data,
    base::OnceCallback<void(ReturnedSuggestions)> callback) {
  GenerateSuggestions(
      form, trigger_field, form_structure, trigger_autofill_field, client,
      all_suggestion_data,
      [&callback](ReturnedSuggestions returned_suggestions) {
        std::move(callback).Run(std::move(returned_suggestions));
      });
}

void IbanSuggestionGenerator::FetchSuggestionData(
    const FormData& form,
    const FormFieldData& trigger_field,
    const FormStructure* form_structure,
    const AutofillField* trigger_autofill_field,
    const AutofillClient& client,
    base::FunctionRef<
        void(std::pair<SuggestionDataSource,
                       std::vector<SuggestionGenerator::SuggestionData>>)>
        callback) {
  // The field is eligible only if it's focused on an IBAN field.
  if (!trigger_autofill_field ||
      !trigger_autofill_field->Type().GetTypes().contains(IBAN_VALUE)) {
    callback({SuggestionDataSource::kIban, {}});
    return;
  }
  if (!client.GetPaymentsAutofillClient()
           ->GetPaymentsDataManager()
           .IsAutofillPaymentMethodsEnabled()) {
    callback({SuggestionDataSource::kIban, {}});
    return;
  }
  // AutofillOptimizationGuideDecider will not be present on unsupported
  // platforms.
  if (auto* autofill_optimization_guide =
          client.GetAutofillOptimizationGuideDecider()) {
    if (autofill_optimization_guide->ShouldBlockSingleFieldSuggestions(
            client.GetLastCommittedPrimaryMainFrameOrigin().GetURL(),
            trigger_autofill_field)) {
      autofill_metrics::LogIbanSuggestionBlockListStatusMetric(
          autofill_metrics::IbanSuggestionBlockListStatus::kBlocked);
      callback({SuggestionDataSource::kIban, {}});
      return;
    }
    autofill_metrics::LogIbanSuggestionBlockListStatusMetric(
        autofill_metrics::IbanSuggestionBlockListStatus::kAllowed);
  } else {
    autofill_metrics::LogIbanSuggestionBlockListStatusMetric(
        autofill_metrics::IbanSuggestionBlockListStatus::
            kBlocklistIsNotAvailable);
  }

  std::vector<Iban> ibans = client.GetPaymentsAutofillClient()
                                ->GetPaymentsDataManager()
                                .GetOrderedIbansToSuggest();
  // If the input box content equals any of the available IBANs, then
  // assume the IBAN has been filled, and don't show any suggestions.
  if (!trigger_autofill_field ||
      (!trigger_autofill_field->value().empty() &&
       base::Contains(ibans, trigger_autofill_field->value(), &Iban::value))) {
    callback({SuggestionDataSource::kIban, {}});
    return;
  }

  FilterIbansToSuggest(trigger_autofill_field->value(), ibans);
  std::vector<SuggestionData> suggestion_data = base::ToVector(
      std::move(ibans),
      [](Iban& iban) { return SuggestionData(std::move(iban)); });
  callback({SuggestionDataSource::kIban, std::move(suggestion_data)});
}

void IbanSuggestionGenerator::GenerateSuggestions(
    const FormData& form,
    const FormFieldData& trigger_field,
    const FormStructure* form_structure,
    const AutofillField* trigger_autofill_field,
    const AutofillClient& client,
    const base::flat_map<SuggestionDataSource, std::vector<SuggestionData>>&
        all_suggestion_data,
    base::FunctionRef<void(ReturnedSuggestions)> callback) {
  auto it = all_suggestion_data.find(SuggestionDataSource::kIban);
  std::vector<SuggestionData> iban_suggestion_data =
      it != all_suggestion_data.end() ? it->second
                                      : std::vector<SuggestionData>();
  if (iban_suggestion_data.empty()) {
    callback({FillingProduct::kIban, {}});
    return;
  }

  std::vector<Iban> ibans = base::ToVector(
      std::move(iban_suggestion_data), [](SuggestionData& suggestion_data) {
        return std::get<autofill::Iban>(std::move(suggestion_data));
      });

  callback({FillingProduct::kIban, GetSuggestionsForIbans(ibans)});
}

void IbanSuggestionGenerator::FilterIbansToSuggest(
    const std::u16string& field_value,
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
