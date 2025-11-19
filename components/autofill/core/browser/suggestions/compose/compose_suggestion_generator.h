// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_SUGGESTIONS_COMPOSE_COMPOSE_SUGGESTION_GENERATOR_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_SUGGESTIONS_COMPOSE_COMPOSE_SUGGESTION_GENERATOR_H_

#include "components/autofill/core/browser/foundations/autofill_client.h"
#include "components/autofill/core/browser/integrators/compose/autofill_compose_delegate.h"
#include "components/autofill/core/browser/suggestions/suggestion_generator.h"

namespace autofill {

class ComposeSuggestionGenerator : public SuggestionGenerator {
 public:
  ComposeSuggestionGenerator(AutofillComposeDelegate* delegate,
                             AutofillSuggestionTriggerSource trigger_source);
  ~ComposeSuggestionGenerator() override;

  void FetchSuggestionData(
      const FormData& form,
      const FormFieldData& trigger_field,
      const FormStructure* form_structure,
      const AutofillField* trigger_autofill_field,
      const AutofillClient& client,
      base::OnceCallback<
          void(std::pair<SuggestionDataSource,
                         std::vector<SuggestionGenerator::SuggestionData>>)>
          callback) override;

  void GenerateSuggestions(
      const FormData& form,
      const FormFieldData& trigger_field,
      const FormStructure* form_structure,
      const AutofillField* trigger_autofill_field,
      const AutofillClient& client,
      const base::flat_map<SuggestionDataSource, std::vector<SuggestionData>>&
          all_suggestion_data,
      base::OnceCallback<void(ReturnedSuggestions)> callback) override;

  // Like SuggestionGenerator override, but takes a base::FunctionRef instead of
  // a base::OnceCallback. Calls that callback exactly once.
  void FetchSuggestionData(
      const FormData& form,
      const FormFieldData& trigger_field,
      const FormStructure* form_structure,
      const AutofillField* trigger_autofill_field,
      const AutofillClient& client,
      base::FunctionRef<
          void(std::pair<SuggestionDataSource,
                         std::vector<SuggestionGenerator::SuggestionData>>)>
          callback);

  // Like SuggestionGenerator override, but takes a base::FunctionRef instead of
  // a base::OnceCallback. Calls that callback exactly once.
  void GenerateSuggestions(
      const FormData& form,
      const FormFieldData& trigger_field,
      const FormStructure* form_structure,
      const AutofillField* trigger_autofill_field,
      const AutofillClient& client,
      const base::flat_map<SuggestionDataSource, std::vector<SuggestionData>>&
          all_suggestion_data,
      base::FunctionRef<void(ReturnedSuggestions)> callback);

 private:
  raw_ptr<AutofillComposeDelegate> compose_delegate_;
  AutofillSuggestionTriggerSource trigger_source_;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_SUGGESTIONS_COMPOSE_COMPOSE_SUGGESTION_GENERATOR_H_
