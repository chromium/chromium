// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_INTEGRATORS_AUTOFILL_AI_AUTOFILL_AI_DELEGATE_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_INTEGRATORS_AUTOFILL_AI_AUTOFILL_AI_DELEGATE_H_

#include "base/functional/callback_forward.h"
#include "base/uuid.h"
#include "components/autofill/core/browser/suggestions/suggestion_type.h"
#include "components/autofill/core/common/dense_set.h"
#include "components/autofill/core/common/unique_ids.h"
#include "services/metrics/public/cpp/ukm_source_id.h"

namespace autofill {

class AutofillField;
class FormStructure;
struct Suggestion;

// The interface for communication from //components/autofill to
// //components/autofill_ai.
class AutofillAiDelegate {
 public:
  virtual ~AutofillAiDelegate() = default;

  // Generates AutofillAi suggestions.
  virtual std::vector<autofill::Suggestion> GetSuggestions(
      autofill::FormGlobalId form_global_id,
      autofill::FieldGlobalId field_global_id) = 0;

  // Attempts to display an import bubble for `form` if Autofill AI is
  // interested in the form. Returns whether an import bubble will be shown.
  // Also contains metric logging logic.
  virtual bool OnFormSubmitted(const FormStructure& form,
                               ukm::SourceId ukm_source_id) = 0;

  // Indicates whether to try to display IPH for opting into AutofillAI. It
  // checks that all of the following is true:
  // - The user is eligible for AutofillAI and has not already opted in.
  // - The user has at least one address or payments instrument saved.
  // - `field` has AutofillAI predictions.
  // - If `form` is submitted (with appropriate values), there is at least one
  //   entity that meets the criteria for import.
  virtual bool ShouldDisplayIph(autofill::FormGlobalId form,
                                autofill::FieldGlobalId field) const = 0;

  // TODO(crbug.com/389629573): The "On*" methods below are used only for
  // logging purposes. Explore different approaches.

  //
  virtual void OnSuggestionsShown(const FormStructure& form,
                                  const AutofillField& field,
                                  ukm::SourceId ukm_source_id) = 0;
  virtual void OnFormSeen(const FormStructure& form) = 0;
  virtual void OnDidFillSuggestion(
      const base::Uuid& guid,
      const FormStructure& form,
      const AutofillField& field,
      base::span<const autofill::AutofillField* const> filled_fields,
      ukm::SourceId ukm_source_id) = 0;
  virtual void OnEditedAutofilledField(const FormStructure& form,
                                       const AutofillField& field,
                                       ukm::SourceId ukm_source_id) = 0;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_INTEGRATORS_AUTOFILL_AI_AUTOFILL_AI_DELEGATE_H_
