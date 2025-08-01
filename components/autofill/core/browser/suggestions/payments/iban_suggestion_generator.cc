// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/suggestions/payments/iban_suggestion_generator.h"

#include "base/containers/contains.h"
#include "base/containers/to_vector.h"
#include "base/functional/function_ref.h"
#include "base/strings/string_util.h"
#include "components/autofill/core/browser/data_manager/payments/payments_data_manager.h"
#include "components/autofill/core/browser/integrators/optimization_guide/autofill_optimization_guide.h"
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
    const FormData& form_data,
    const FormFieldData& field_data,
    const FormStructure* form,
    const AutofillField* field,
    const AutofillClient& client,
    base::OnceCallback<
        void(std::pair<FillingProduct,
                       std::vector<SuggestionGenerator::SuggestionData>>)>
        callback) {
  FetchSuggestionData(
      form_data, field_data, form, field, client,
      [&callback](std::pair<FillingProduct,
                            std::vector<SuggestionGenerator::SuggestionData>>
                      suggestion_data) {
        std::move(callback).Run(std::move(suggestion_data));
      });
}

void IbanSuggestionGenerator::GenerateSuggestions(
    const FormData& form_data,
    const FormFieldData& field_data,
    const FormStructure* form,
    const AutofillField* field,
    const std::vector<std::pair<FillingProduct, std::vector<SuggestionData>>>&
        all_suggestion_data,
    base::OnceCallback<void(ReturnedSuggestions)> callback) {
  GenerateSuggestions(
      form_data, field_data, form, field, all_suggestion_data,
      [&callback](ReturnedSuggestions returned_suggestions) {
        std::move(callback).Run(std::move(returned_suggestions));
      });
}

void IbanSuggestionGenerator::FetchSuggestionData(
    const FormData& form_data,
    const FormFieldData& field_data,
    const FormStructure* form,
    const AutofillField* field,
    const AutofillClient& client,
    base::FunctionRef<
        void(std::pair<FillingProduct,
                       std::vector<SuggestionGenerator::SuggestionData>>)>
        callback) {
  // The field is eligible only if it's focused on an IBAN field.
  if (!field || !field->Type().GetTypes().contains(IBAN_VALUE)) {
    callback({FillingProduct::kIban, {}});
    return;
  }
  if (!client.GetPaymentsAutofillClient()
           ->GetPaymentsDataManager()
           .IsAutofillPaymentMethodsEnabled()) {
    callback({FillingProduct::kIban, {}});
    return;
  }
  // AutofillOptimizationGuide will not be present on unsupported platforms.
  if (auto* autofill_optimization_guide =
          client.GetAutofillOptimizationGuide()) {
    if (autofill_optimization_guide->ShouldBlockSingleFieldSuggestions(
            client.GetLastCommittedPrimaryMainFrameOrigin().GetURL(), field)) {
      autofill_metrics::LogIbanSuggestionBlockListStatusMetric(
          autofill_metrics::IbanSuggestionBlockListStatus::kBlocked);
      callback({FillingProduct::kIban, {}});
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
  FilterIbansToSuggest(field->value(), ibans);
  std::vector<SuggestionData> suggestion_data = base::ToVector(
      std::move(ibans),
      [](Iban& iban) { return SuggestionData(std::move(iban)); });
  callback({FillingProduct::kIban, std::move(suggestion_data)});
}

void IbanSuggestionGenerator::GenerateSuggestions(
    const FormData& form_data,
    const FormFieldData& field_data,
    const FormStructure* form,
    const AutofillField* field,
    const std::vector<std::pair<FillingProduct, std::vector<SuggestionData>>>&
        all_suggestion_data,
    base::FunctionRef<void(ReturnedSuggestions)> callback) {
  std::vector<SuggestionData> iban_suggestion_data =
      ExtractSuggestionDataForFillingProduct(all_suggestion_data,
                                             FillingProduct::kIban);

  std::vector<Iban> ibans = base::ToVector(
      std::move(iban_suggestion_data), [](SuggestionData& suggestion_data) {
        return std::get<autofill::Iban>(std::move(suggestion_data));
      });
  // If the input box content equals any of the available IBANs, then
  // assume the IBAN has been filled, and don't show any suggestions.
  if (!field || (!field->value().empty() &&
                 base::Contains(ibans, field->value(), &Iban::value))) {
    callback({FillingProduct::kIban, {}});
    return;
  }

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
