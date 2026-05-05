// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_SUGGESTIONS_AUTOFILL_AI_AUTOFILL_AI_SUGGESTION_GENERATOR_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_SUGGESTIONS_AUTOFILL_AI_AUTOFILL_AI_SUGGESTION_GENERATOR_H_

#include "components/autofill/core/browser/suggestions/suggestion_generator.h"

namespace autofill {

class AutofillAiSuggestionGenerator : public SuggestionGenerator {
 public:
  explicit AutofillAiSuggestionGenerator();
  ~AutofillAiSuggestionGenerator() override;

  void GenerateSuggestions(
      const FormData& form,
      const FormFieldData& trigger_field,
      const FormStructure* form_structure,
      const AutofillField* trigger_autofill_field,
      AutofillClient& client,
      base::OnceCallback<void(ReturnedSuggestions)> callback) override;

  // Like SuggestionGenerator override, but takes a base::FunctionRef instead of
  // a base::OnceCallback. Calls that callback exactly once.
  void GenerateSuggestions(
      const FormData& form,
      const FormFieldData& trigger_field,
      const FormStructure* form_structure,
      const AutofillField* trigger_autofill_field,
      AutofillClient& client,
      base::FunctionRef<void(ReturnedSuggestions)> callback);
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_SUGGESTIONS_AUTOFILL_AI_AUTOFILL_AI_SUGGESTION_GENERATOR_H_
