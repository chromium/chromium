// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/suggestions/plus_addresses/plus_address_suggestion_generator.h"

#include <optional>

#include "base/containers/map_util.h"
#include "base/containers/to_vector.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
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
  if (!plus_address_delegate_) {
    std::move(callback).Run({FillingProduct::kPlusAddresses, {}});
    return;
  }

  const std::vector<SuggestionData>* plus_addresses_data =
      base::FindOrNull(all_suggestion_data, SuggestionDataSource::kPlusAddress);
  if (!plus_addresses_data || plus_addresses_data->empty()) {
    std::move(callback).Run({FillingProduct::kPlusAddresses, {}});
    return;
  }

  std::vector<std::string> plus_addresses_to_suggest = base::ToVector(
      *plus_addresses_data, [](const SuggestionData& suggestion_data) {
        return std::get<PlusAddress>(std::move(suggestion_data)).value();
      });

  std::vector<Suggestion> suggestions =
      plus_address_delegate_->GetSuggestionsFromPlusAddresses(
          plus_addresses_to_suggest,
          client.GetLastCommittedPrimaryMainFrameOrigin(), trigger_field,
          is_manually_triggered_);
  std::move(callback).Run(
      {FillingProduct::kPlusAddresses, std::move(suggestions)});
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
