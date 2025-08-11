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
    const FormData& form_data,
    const FormFieldData& field_data,
    const FormStructure* form,
    const AutofillField* field,
    const AutofillClient& client,
    base::OnceCallback<
        void(std::pair<FillingProduct,
                       std::vector<SuggestionGenerator::SuggestionData>>)>
        callback) {
  FetchSuggestionData(
      form_data, field_data, form, field, client,
      [&callback](std::pair<FillingProduct,
                            std::vector<SuggestionGenerator::SuggestionData>>
                      suggestion_data) {
        std::move(callback).Run(std::move(suggestion_data));
      });
}

void ComposeSuggestionGenerator::GenerateSuggestions(
    const FormData& form_data,
    const FormFieldData& field_data,
    const FormStructure* form,
    const AutofillField* field,
    const std::vector<std::pair<FillingProduct, std::vector<SuggestionData>>>&
        all_suggestion_data,
    base::OnceCallback<void(ReturnedSuggestions)> callback) {
  GenerateSuggestions(
      form_data, field_data, form, field, all_suggestion_data,
      [&callback](ReturnedSuggestions returned_suggestions) {
        std::move(callback).Run(std::move(returned_suggestions));
      });
}

void ComposeSuggestionGenerator::FetchSuggestionData(
    const FormData& form_data,
    const FormFieldData& field_data,
    const FormStructure* form,
    const AutofillField* field,
    const AutofillClient& client,
    base::FunctionRef<
        void(std::pair<FillingProduct,
                       std::vector<SuggestionGenerator::SuggestionData>>)>
        callback) {
  // Compose suggestion generator does not fetch any data.
  callback({FillingProduct::kCompose, {}});
}

void ComposeSuggestionGenerator::GenerateSuggestions(
    const FormData& form_data,
    const FormFieldData& field_data,
    const FormStructure* form,
    const AutofillField* field,
    const std::vector<std::pair<FillingProduct, std::vector<SuggestionData>>>&
        all_suggestion_data,
    base::FunctionRef<void(ReturnedSuggestions)> callback) {
  if (!compose_delegate_ ||
      (field_data.form_control_type() != FormControlType::kTextArea &&
       field_data.form_control_type() != FormControlType::kContentEditable)) {
    callback({FillingProduct::kCompose, {}});
    return;
  }

  bool other_products_have_suggestion_data = std::ranges::any_of(
      all_suggestion_data,
      [](const std::pair<FillingProduct, std::vector<SuggestionData>>& data) {
        return !data.second.empty();
      });
  if (other_products_have_suggestion_data ||
      IsAutofillManuallyTriggered(trigger_source_) ||
      trigger_source_ == AutofillSuggestionTriggerSource::
                             kShowPromptAfterDialogClosedNonManualFallback) {
    callback({FillingProduct::kCompose, {}});
    return;
  }

  std::optional<Suggestion> suggestion =
      compose_delegate_->GetSuggestion(form_data, field_data, trigger_source_);
  if (!suggestion) {
    callback({FillingProduct::kCompose, {}});
    return;
  }
  callback({FillingProduct::kCompose, {*suggestion}});
}

}  // namespace autofill
