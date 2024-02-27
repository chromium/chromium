// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_SUGGESTION_GENERATOR_TEST_API_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_SUGGESTION_GENERATOR_TEST_API_H_

#include "components/autofill/core/browser/autofill_suggestion_generator.h"

namespace autofill {

// Exposes some testing operations for BrowserAutofillManager.
class AutofillSuggestionGeneratorTestApi {
 public:
  explicit AutofillSuggestionGeneratorTestApi(
      AutofillSuggestionGenerator& suggestion_generator)
      : suggestion_generator_(suggestion_generator) {}

  std::vector<Suggestion> CreateSuggestionsFromProfiles(
      const std::vector<raw_ptr<const AutofillProfile, VectorExperimental>>&
          profiles,
      const FieldTypeSet& field_types,
      std::optional<FieldTypeSet> last_targeted_fields,
      FieldType trigger_field_type,
      uint64_t trigger_field_max_length,
      const std::set<std::string>& previously_hidden_profiles_guid = {}) {
    return suggestion_generator_->CreateSuggestionsFromProfiles(
        profiles, field_types, last_targeted_fields, trigger_field_type,
        trigger_field_max_length, previously_hidden_profiles_guid);
  }

 private:
  raw_ref<AutofillSuggestionGenerator> suggestion_generator_;
};

inline AutofillSuggestionGeneratorTestApi test_api(
    AutofillSuggestionGenerator& suggestion_generator) {
  return AutofillSuggestionGeneratorTestApi(suggestion_generator);
}

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_SUGGESTION_GENERATOR_TEST_API_H_
