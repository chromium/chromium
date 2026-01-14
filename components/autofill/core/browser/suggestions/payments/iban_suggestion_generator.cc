// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/suggestions/payments/iban_suggestion_generator.h"

#include <algorithm>

#include "base/containers/to_vector.h"
#include "base/functional/function_ref.h"
#include "base/strings/string_util.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/data_manager/payments/payments_data_manager.h"
#include "components/autofill/core/browser/integrators/optimization_guide/autofill_optimization_guide_decider.h"
#include "components/autofill/core/browser/payments/iban_manager.h"
#include "components/autofill/core/browser/suggestions/payments/payments_suggestion_generator_util.h"
#include "components/grit/components_scaled_resources.h"
#include "ui/base/resource/resource_bundle.h"

namespace autofill {
namespace {
// The server-based IBAN suggestions will be returned if the IBAN's prefix is
// absent and the length of the input field is less than
// `kFieldLengthLimitOnServerIbanSuggestion` characters.
constexpr int kFieldLengthLimitOnServerIbanSuggestion = 6;

// Generates a footer suggestion "Manage payment methods..." menu item which
// will redirect to Chrome payment settings page.
//
// The difference between `CreateManageCreditCardsSuggestion()` and
// `CreateManageIbansSuggestion()` is that they use a different
// `SuggestionType`. This distinction is needed for metrics recording.
Suggestion CreateManageIbansSuggestion() {
  return CreateManagePaymentMethodsEntry(SuggestionType::kManageIban,
                                         /*with_gpay_logo=*/false);
}

// Generates suggestions for all available IBANs.
std::vector<Suggestion> GetSuggestionsForIbans(const std::vector<Iban>& ibans) {
  if (ibans.empty()) {
    return {};
  }
  std::vector<Suggestion> suggestions;
  suggestions.reserve(ibans.size() + 2);
  for (const Iban& iban : ibans) {
    Suggestion suggestion(SuggestionType::kIbanEntry);
    suggestion.custom_icon =
        ui::ResourceBundle::GetSharedInstance().GetImageNamed(
            ShouldUseNewFopDisplay() ? IDR_AUTOFILL_IBAN
                                     : IDR_AUTOFILL_IBAN_OLD);
    suggestion.icon = Suggestion::Icon::kIban;
    if (iban.record_type() == Iban::kLocalIban) {
      suggestion.payload = Suggestion::Guid(iban.guid());
    } else {
      CHECK(iban.record_type() == Iban::kServerIban);
      suggestion.payload = Suggestion::InstrumentId(iban.instrument_id());
    }

    std::u16string iban_identifier =
        iban.GetIdentifierStringForAutofillDisplay();
    if constexpr (BUILDFLAG(IS_ANDROID)) {
      // For Android keyboard accessory, the displayed value will be nickname +
      // identifier string, if the nickname is too long to fit due to bubble
      // width limitation, it will be truncated.
      if (!iban.nickname().empty()) {
        suggestion.main_text.value = iban.nickname();
        suggestion.minor_texts.emplace_back(iban_identifier);
      } else {
        suggestion.main_text.value = std::move(iban_identifier);
      }
    } else {
      if (iban.nickname().empty()) {
        suggestion.main_text = Suggestion::Text(
            iban_identifier, Suggestion::Text::IsPrimary(true));
      } else {
        suggestion.main_text = Suggestion::Text(
            iban.nickname(), Suggestion::Text::IsPrimary(true));
        suggestion.labels = {{Suggestion::Text(iban_identifier)}};
      }
    }
    suggestions.push_back(suggestion);
  }

  suggestions.emplace_back(SuggestionType::kSeparator);
  suggestions.push_back(CreateManageIbansSuggestion());
  return suggestions;
}

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
       std::ranges::contains(ibans, trigger_autofill_field->value(),
                             &Iban::value))) {
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
