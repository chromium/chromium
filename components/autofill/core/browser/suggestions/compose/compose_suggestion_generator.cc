// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/suggestions/compose/compose_suggestion_generator.h"

#include "base/containers/to_vector.h"
#include "components/autofill/core/browser/suggestions/compose/compose_availability.h"
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
  if (!compose_delegate_ ||
      (trigger_field.form_control_type() != FormControlType::kTextArea &&
       trigger_field.form_control_type() !=
           FormControlType::kContentEditable)) {
    callback({SuggestionDataSource::kCompose, {}});
    return;
  }

  if (!compose_delegate_->ShouldTriggerComposePopup(form, trigger_field,
                                                    trigger_source_)) {
    callback({SuggestionDataSource::kCompose, {}});
    return;
  }

  callback({SuggestionDataSource::kCompose, {ComposeAvailability(true)}});
}

void ComposeSuggestionGenerator::GenerateSuggestions(
    const FormData& form,
    const FormFieldData& trigger_field,
    const FormStructure* form_structure,
    const AutofillField* trigger_autofill_field,
    const AutofillClient& client,
    const base::flat_map<SuggestionDataSource, std::vector<SuggestionData>>&
        all_suggestion_data,
    base::FunctionRef<void(ReturnedSuggestions)> callback) {
  auto it = all_suggestion_data.find(SuggestionDataSource::kCompose);
  std::vector<SuggestionData> compose_data =
      it != all_suggestion_data.end() ? it->second
                                      : std::vector<SuggestionData>();
  if (compose_data.empty()) {
    callback({FillingProduct::kCompose, {}});
    return;
  }

  CHECK_EQ(compose_data.size(), 1u);
  CHECK(std::holds_alternative<ComposeAvailability>(compose_data[0]));
  CHECK(std::get<ComposeAvailability>(compose_data[0]).value());
  callback({FillingProduct::kCompose,
            {compose_delegate_->GetSuggestion(form, trigger_field,
                                              trigger_source_)}});
}

}  // namespace autofill
