// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/suggestions/plus_addresses/plus_address_suggestion_generator.h"

#include <optional>

#include "base/containers/map_util.h"
#include "base/containers/to_vector.h"
#include "base/functional/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/data_model/transliterator.h"
#include "components/autofill/core/browser/filling/filling_product.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/integrators/password_form_classification.h"
#include "components/autofill/core/browser/integrators/plus_addresses/autofill_plus_address_delegate.h"
#include "components/autofill/core/browser/suggestions/plus_addresses/plus_address.h"
#include "components/autofill/core/browser/suggestions/suggestion.h"
#include "components/autofill/core/browser/suggestions/suggestion_generator.h"
#include "components/autofill/core/browser/suggestions/suggestion_util.h"
#include "components/autofill/core/common/autofill_util.h"

namespace autofill {
namespace {

constexpr FieldTypeSet kPlusAddressRelevantFieldTypes =
    Union(FieldTypesOfGroup(FieldTypeGroup::kEmail),
          FieldTypeSet{USERNAME, SINGLE_USERNAME});

}  // namespace

std::vector<Suggestion> GetSuggestionsFromPlusAddresses(
    const FormData& form,
    const FormFieldData& trigger_field,
    const FormStructure* form_structure,
    const AutofillField* trigger_autofill_field,
    AutofillClient& client,
    bool is_manually_triggered,
    const std::vector<std::string>& plus_addresses) {
  if (!client.GetPlusAddressDelegate()) {
    return {};
  }

  PlusAddressSuggestionGenerator generator(client.GetPlusAddressDelegate(),
                                           is_manually_triggered);
  std::vector<SuggestionGenerator::SuggestionData>
      plus_address_suggestion_data =
          base::ToVector(plus_addresses, [](const std::string& plus_address) {
            return SuggestionGenerator::SuggestionData(
                PlusAddress(plus_address));
          });

  std::vector<Suggestion> suggestions;
  // Execution of the `PlusAddressSuggestionGenerator::GenerateSuggestions()` is
  // synchronous, so returning `suggestions` is safe.
  auto on_suggestions_generated =
      [&suggestions](
          SuggestionGenerator::ReturnedSuggestions returned_suggestions) {
        suggestions = std::move(returned_suggestions.second);
      };

  base::flat_map<SuggestionGenerator::SuggestionDataSource,
                 std::vector<SuggestionGenerator::SuggestionData>>
      all_suggestion_data = {
          {SuggestionGenerator::SuggestionDataSource::kPlusAddress,
           std::move(plus_address_suggestion_data)}};

  generator.GenerateSuggestions(form, trigger_field, form_structure,
                                trigger_autofill_field, client,
                                all_suggestion_data, on_suggestions_generated);
  return suggestions;
}

PlusAddressSuggestionGenerator::PlusAddressSuggestionGenerator(
    AutofillPlusAddressDelegate* plus_address_delegate,
    bool is_manually_triggered)
    : plus_address_delegate_(plus_address_delegate),
      is_manually_triggered_(is_manually_triggered) {}

PlusAddressSuggestionGenerator::~PlusAddressSuggestionGenerator() = default;

void PlusAddressSuggestionGenerator::FetchSuggestionData(
    const FormData& form,
    const FormFieldData& trigger_field,
    const FormStructure* form_structure,
    const AutofillField* trigger_autofill_field,
    const AutofillClient& client,
    base::OnceCallback<void(std::pair<SuggestionDataSource,
                                      std::vector<SuggestionData>>)> callback) {
  if (!plus_address_delegate_) {
    std::move(callback).Run({SuggestionDataSource::kPlusAddress, {}});
    return;
  }

  std::optional<SuggestionDataSource> source_to_suggest =
      GetSourceToSuggest(trigger_autofill_field, client);
  if (!source_to_suggest) {
    std::move(callback).Run({SuggestionDataSource::kPlusAddress, {}});
    return;
  }

  auto plus_address_fetch_callback =
      base::BindOnce(
          [](SuggestionDataSource source_to_suggest,
             bool was_triggered_manually, bool field_was_autofilled,
             std::u16string field_value,
             std::vector<std::string> plus_addresses) {
            std::vector<SuggestionData> plus_address_data;
            for (const std::string& affiliated_plus_address : plus_addresses) {
              // Generally, plus address suggestions are only available on
              // previously autofilled or if suggestions were manually
              // triggered, no prefix matching should be applied.
              if (was_triggered_manually || field_was_autofilled ||
                  affiliated_plus_address.starts_with(
                      base::UTF16ToUTF8(field_value))) {
                plus_address_data.emplace_back(
                    PlusAddress(affiliated_plus_address));
              }
            }
            return std::make_pair(source_to_suggest,
                                  std::move(plus_address_data));
          },
          *source_to_suggest, is_manually_triggered_,
          trigger_field.is_autofilled(),
          autofill::RemoveDiacriticsAndConvertToLowerCase(
              trigger_field.value()))
          .Then(std::move(callback));

  plus_address_delegate_->GetAffiliatedPlusAddresses(
      client.GetLastCommittedPrimaryMainFrameOrigin(),
      std::move(plus_address_fetch_callback));
}

void PlusAddressSuggestionGenerator::GenerateSuggestions(
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

void PlusAddressSuggestionGenerator::GenerateSuggestions(
    const FormData& form,
    const FormFieldData& trigger_field,
    const FormStructure* form_structure,
    const AutofillField* trigger_autofill_field,
    const AutofillClient& client,
    const base::flat_map<SuggestionDataSource, std::vector<SuggestionData>>&
        all_suggestion_data,
    base::FunctionRef<void(ReturnedSuggestions)> callback) {
  const std::vector<SuggestionData>* plus_addresses_data =
      base::FindOrNull(all_suggestion_data, SuggestionDataSource::kPlusAddress);
  if (!plus_addresses_data || plus_addresses_data->empty()) {
    callback({FillingProduct::kPlusAddresses, {}});
    return;
  }

  std::vector<std::string> plus_addresses_to_suggest = base::ToVector(
      *plus_addresses_data, [](const SuggestionData& suggestion_data) {
        return std::get<PlusAddress>(std::move(suggestion_data)).value();
      });

  std::vector<Suggestion> suggestions =
      plus_address_delegate_->GetSuggestionsFromPlusAddresses(
          plus_addresses_to_suggest);
  callback({FillingProduct::kPlusAddresses, std::move(suggestions)});
}

std::optional<PlusAddressSuggestionGenerator::SuggestionDataSource>
PlusAddressSuggestionGenerator::GetSourceToSuggest(
    const AutofillField* trigger_autofill_field,
    const AutofillClient& client) {
  if (is_manually_triggered_) {
    return SuggestionDataSource::kPlusAddress;
  }

  if (!trigger_autofill_field ||
      !plus_address_delegate_->IsFieldEligibleForPlusAddress(
          *trigger_autofill_field) ||
      !plus_address_delegate_->IsPlusAddressFillingEnabled(
          client.GetLastCommittedPrimaryMainFrameOrigin()) ||
      SuppressSuggestionsForAutocompleteUnrecognizedField(
          *trigger_autofill_field)) {
    return std::nullopt;
  }

  return trigger_autofill_field->Type().GetTypes().contains_none(
             kPlusAddressRelevantFieldTypes)
             ? SuggestionDataSource::kPlusAddressForAddress
             : SuggestionDataSource::kPlusAddress;
}

}  // namespace autofill
