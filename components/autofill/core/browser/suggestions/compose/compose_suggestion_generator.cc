// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/suggestions/compose/compose_suggestion_generator.h"

#include "base/containers/to_vector.h"
#include "components/autofill/core/common/autofill_util.h"

namespace autofill {

ComposeSuggestionGenerator::ComposeSuggestionGenerator(
    AutofillComposeDelegate* compose_delegate,
    AutofillSuggestionTriggerSource trigger_source)
    : compose_delegate_(compose_delegate), trigger_source_(trigger_source) {}

ComposeSuggestionGenerator::~ComposeSuggestionGenerator() = default;

void ComposeSuggestionGenerator::GenerateSuggestions(
    const FormData& form,
    const FormFieldData& trigger_field,
    const FormStructure* form_structure,
    const AutofillField* trigger_autofill_field,
    const AutofillClient& client,
    base::OnceCallback<void(ReturnedSuggestions)> callback) {
  GenerateSuggestions(
      form, trigger_field, form_structure, trigger_autofill_field, client,
      [&callback](ReturnedSuggestions returned_suggestions) {
        std::move(callback).Run(std::move(returned_suggestions));
      });
}

void ComposeSuggestionGenerator::GenerateSuggestions(
    const FormData& form,
    const FormFieldData& trigger_field,
    const FormStructure* form_structure,
    const AutofillField* trigger_autofill_field,
    const AutofillClient& client,
    base::FunctionRef<void(ReturnedSuggestions)> callback) {
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

  callback({SuggestionDataSource::kCompose,
            {compose_delegate_->GetSuggestion(form, trigger_field,
                                              trigger_source_)}});
}

}  // namespace autofill
