// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_SUGGESTIONS_AUTOFILL_AI_AUTOFILL_AI_SUGGESTION_GENERATOR_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_SUGGESTIONS_AUTOFILL_AI_AUTOFILL_AI_SUGGESTION_GENERATOR_H_

#include "components/autofill/core/browser/suggestions/suggestion_generator.h"

namespace autofill {

class AutofillAiSuggestionGenerator : public SuggestionGenerator {
 public:
  AutofillAiSuggestionGenerator();
  ~AutofillAiSuggestionGenerator() override;

  void FetchSuggestionData(
      const FormData& form_data,
      const FormFieldData& field_data,
      const FormStructure* form,
      const AutofillField* field,
      const AutofillClient& client,
      base::OnceCallback<
          void(std::pair<FillingProduct,
                         std::vector<SuggestionGenerator::SuggestionData>>)>
          callback) override;

  void GenerateSuggestions(
      const FormData& form_data,
      const FormFieldData& field_data,
      const FormStructure* form,
      const AutofillField* field,
      const std::vector<std::pair<FillingProduct, std::vector<SuggestionData>>>&
          all_suggestion_data,
      base::OnceCallback<void(ReturnedSuggestions)> callback) override;

  // Like SuggestionGenerator override, but takes a base::FunctionRef instead of
  // a base::OnceCallback. Calls that callback exactly once.
  void FetchSuggestionData(
      const FormData& form_data,
      const FormFieldData& field_data,
      const FormStructure* form,
      const AutofillField* field,
      const AutofillClient& client,
      base::FunctionRef<
          void(std::pair<FillingProduct,
                         std::vector<SuggestionGenerator::SuggestionData>>)>
          callback);

  // Like SuggestionGenerator override, but takes a base::FunctionRef instead of
  // a base::OnceCallback. Calls that callback exactly once.
  void GenerateSuggestions(
      const FormData& form_data,
      const FormFieldData& field_data,
      const FormStructure* form,
      const AutofillField* field,
      const std::vector<std::pair<FillingProduct, std::vector<SuggestionData>>>&
          all_suggestion_data,
      base::FunctionRef<void(ReturnedSuggestions)> callback);

  base::WeakPtr<AutofillAiSuggestionGenerator> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  std::string app_locale_;

  base::WeakPtrFactory<AutofillAiSuggestionGenerator> weak_ptr_factory_{this};
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_SUGGESTIONS_AUTOFILL_AI_AUTOFILL_AI_SUGGESTION_GENERATOR_H_
