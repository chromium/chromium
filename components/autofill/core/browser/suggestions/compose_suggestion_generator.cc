// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/suggestions/compose_suggestion_generator.h"

#include "base/containers/to_vector.h"
#include "components/autofill/core/common/autofill_util.h"

namespace autofill {

ComposeSuggestionGenerator::ComposeSuggestionGenerator(
    AutofillComposeDelegate* compose_delegate,
    AutofillSuggestionTriggerSource trigger_source)
    : compose_delegate_(compose_delegate), trigger_source_(trigger_source) {}

ComposeSuggestionGenerator::~ComposeSuggestionGenerator() = default;

void ComposeSuggestionGenerator::FetchSuggestionData(
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

void ComposeSuggestionGenerator::GenerateSuggestions(
    const FormData& form,
    const FormFieldData& trigger_field,
    const FormStructure* form_structure,
    const AutofillField* trigger_autofill_field,
    const base::flat_map<SuggestionDataSource, std::vector<SuggestionData>>&
        all_suggestion_data,
    base::OnceCallback<void(ReturnedSuggestions)> callback) {
  GenerateSuggestions(
      form, trigger_field, form_structure, trigger_autofill_field,
      all_suggestion_data,
      [&callback](ReturnedSuggestions returned_suggestions) {
        std::move(callback).Run(std::move(returned_suggestions));
      });
}

void ComposeSuggestionGenerator::FetchSuggestionData(
    const FormData& form,
    const FormFieldData& trigger_field,
    const FormStructure* form_structure,
    const AutofillField* trigger_autofill_field,
    const AutofillClient& client,
    base::FunctionRef<
        void(std::pair<SuggestionDataSource,
                       std::vector<SuggestionGenerator::SuggestionData>>)>
        callback) {
  // Compose suggestion generator does not fetch any data.
  callback({SuggestionDataSource::kCompose, {}});
}

void ComposeSuggestionGenerator::GenerateSuggestions(
    const FormData& form,
    const FormFieldData& trigger_field,
    const FormStructure* form_structure,
    const AutofillField* trigger_autofill_field,
    const base::flat_map<SuggestionDataSource, std::vector<SuggestionData>>&
        all_suggestion_data,
    base::FunctionRef<void(ReturnedSuggestions)> callback) {
  if (!compose_delegate_ ||
      (trigger_field.form_control_type() != FormControlType::kTextArea &&
       trigger_field.form_control_type() !=
           FormControlType::kContentEditable)) {
    callback({FillingProduct::kCompose, {}});
    return;
  }

  bool other_products_have_suggestion_data = std::ranges::any_of(
      all_suggestion_data,
      [](const std::pair<SuggestionDataSource, std::vector<SuggestionData>>&
             data) { return !data.second.empty(); });
  if (other_products_have_suggestion_data) {
    callback({FillingProduct::kCompose, {}});
    return;
  }

  std::optional<Suggestion> suggestion =
      compose_delegate_->GetSuggestion(form, trigger_field, trigger_source_);
  if (!suggestion) {
    callback({FillingProduct::kCompose, {}});
    return;
  }
  callback({FillingProduct::kCompose, {*suggestion}});
}

}  // namespace autofill
