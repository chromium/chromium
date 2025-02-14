// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_INTEGRATORS_AUTOFILL_AI_DELEGATE_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_INTEGRATORS_AUTOFILL_AI_DELEGATE_H_

#include "components/autofill/core/browser/filling/field_filling_skip_reason.h"
#include "components/autofill/core/browser/filling/filling_product.h"
#include "components/autofill/core/browser/suggestions/suggestion.h"
#include "components/autofill/core/common/dense_set.h"
#include "components/autofill/core/common/unique_ids.h"

namespace autofill {

class AutofillField;
class FormData;
class FormFieldData;
class FormStructure;

// The interface for communication from //components/autofill to
// //components/autofill_ai.
class AutofillAiDelegate {
 public:
  using GetSuggestionsCallback =
      base::OnceCallback<void(std::vector<autofill::Suggestion>)>;

  using UpdateSuggestionsCallback =
      base::RepeatingCallback<void(std::vector<Suggestion>,
                                   AutofillSuggestionTriggerSource)>;

  virtual ~AutofillAiDelegate() = default;

  // Returns AutofillAi suggestions. These suggestions can be filling
  // suggestions when triggered via left click, or loading suggestions when
  // using manual fallbacks.
  virtual void GetSuggestions(autofill::FormGlobalId form_global_id,
                              autofill::FieldGlobalId field_global_id,
                              bool is_manual_fallback,
                              GetSuggestionsCallback callback) = 0;

  // Returns whether `form` and `field` are eligible for the Autofill AI
  // experience.
  virtual bool IsEligibleForAutofillAi(const FormStructure& form,
                                       const AutofillField& field) const = 0;

  // Returns whether the current user is eligible for the Autofill AI
  // experience.
  virtual bool IsUserEligible() const = 0;

  // Displays an import bubble for `form` if Autofill AI is interested in the
  // form and then calls `autofill_callback`. It is guaranteed that `form` is
  // non-null.
  //
  // CAUTION: `autofill_callback` *must* be called, independent of whether
  // Autofill AI is interested in the form or not. The passed `FormStructure`
  // *must* be identical to `form_structure`; in particular, it must be
  // non-null.
  //
  // The purpose of `autofill_callback` is to allow Autofill to import the form
  // on its own and/or send votes, for example. If Autofill AI has imported the
  // form, `autofill_ai_shows_bubble` is set to true; this is to avoid
  // conflicting import bubbles. The call happens synchronously or
  // asynchronously.
  virtual void MaybeImportForm(
      std::unique_ptr<FormStructure> form_structure,
      base::OnceCallback<void(std::unique_ptr<FormStructure> form,
                              bool autofill_ai_shows_bubble)>
          autofill_callback) = 0;

  // Returns whether we should suggest to the user enabling the Autofill AI pref
  // in chrome://settings.
  virtual bool ShouldDisplayIph(const AutofillField& field) const = 0;

  // Event handler called when suggestions are shown.
  virtual void OnSuggestionsShown(
      const DenseSet<SuggestionType>& shown_suggestion_types,
      const FormData& form,
      const FormFieldData& trigger_field,
      UpdateSuggestionsCallback update_suggestions_callback) = 0;

  // TODO(crbug.com/389629573): This method is only used for logging purposes.
  // Consider if we can have a difference approach.
  virtual void OnFormSeen(const FormStructure& form) = 0;

  virtual void OnDidFillSuggestion(FormGlobalId form_id) = 0;

  // Called when the user manually edits a field that was filled using Autofill
  // AI.
  virtual void OnEditedAutofilledField(FormGlobalId form_id) = 0;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_INTEGRATORS_AUTOFILL_AI_DELEGATE_H_
