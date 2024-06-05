// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_ADDRESS_SUGGESTION_GENERATOR_TEST_API_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_ADDRESS_SUGGESTION_GENERATOR_TEST_API_H_

#include "components/autofill/core/browser/address_suggestion_generator.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/metrics/payments/card_metadata_metrics.h"
#include "components/autofill/core/common/aliases.h"
#include "components/autofill/core/common/form_field_data.h"

namespace autofill {

// Exposes some testing operations for BrowserAutofillManager.
class AddressSuggestionGeneratorTestApi {
 public:
  explicit AddressSuggestionGeneratorTestApi(
      AddressSuggestionGenerator& suggestion_generator)
      : suggestion_generator_(suggestion_generator) {}

  std::vector<raw_ptr<const AutofillProfile, VectorExperimental>>
  GetProfilesToSuggest(
      FieldType trigger_field_type,
      const std::u16string& field_contents,
      bool field_is_autofilled,
      const FieldTypeSet& field_types,
      AutofillSuggestionTriggerSource trigger_source =
          AutofillSuggestionTriggerSource::kFormControlElementClicked) {
    return suggestion_generator_->GetProfilesToSuggest(
        trigger_field_type, field_contents, field_is_autofilled, field_types,
        suggestion_generator_->GetProfilesToSuggestOptions(
            trigger_field_type, field_contents, field_is_autofilled,
            trigger_source));
  }

  std::vector<Suggestion> CreateSuggestionsFromProfiles(
      const std::vector<raw_ptr<const AutofillProfile, VectorExperimental>>&
          profiles,
      const FieldTypeSet& field_types,
      SuggestionType suggestion_type,
      FieldType trigger_field_type,
      uint64_t trigger_field_max_length) {
    return suggestion_generator_->CreateSuggestionsFromProfiles(
        profiles, field_types, suggestion_type, trigger_field_type,
        trigger_field_max_length);
  }

 private:
  raw_ref<AddressSuggestionGenerator> suggestion_generator_;
};

inline AddressSuggestionGeneratorTestApi test_api(
    AddressSuggestionGenerator& suggestion_generator) {
  return AddressSuggestionGeneratorTestApi(suggestion_generator);
}

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_ADDRESS_SUGGESTION_GENERATOR_TEST_API_H_
