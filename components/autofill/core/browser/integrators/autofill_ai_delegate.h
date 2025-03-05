// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_INTEGRATORS_AUTOFILL_AI_DELEGATE_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_INTEGRATORS_AUTOFILL_AI_DELEGATE_H_

#include "base/functional/callback_forward.h"
#include "components/autofill/core/browser/suggestions/suggestion_type.h"
#include "components/autofill/core/common/dense_set.h"
#include "components/autofill/core/common/unique_ids.h"

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

  // Returns whether `form` and `field` are eligible for the Autofill AI
  // experience.
  virtual bool IsFormAndFieldEligibleForAutofillAi(
      const FormStructure& form,
      const AutofillField& field) const = 0;

  // Returns whether the current user is eligible for the Autofill AI
  // experience. This is not dependent on whether the user has enabled the flag
  // or not.
  virtual bool IsUserEligible() const = 0;

  // Returns whether the current user can get a save/update dialog and fill a
  // form using AutofillAi. This checks both that the user is eligible and that
  // it has expected pref enabled.
  virtual bool IsUserEligibleForFillingAndImporting() const = 0;

  // Displays an import bubble for `form` if Autofill AI is interested in the
  // form and then calls `autofill_callback`. Returns whether an import bubble
  // will be shown.
  virtual bool MaybeImportForm(const FormStructure& form_structure) = 0;

  // Returns whether we should suggest to the user enabling the Autofill AI pref
  // in chrome://settings.
  virtual bool ShouldDisplayIph(const AutofillField& field) const = 0;

  // TODO(crbug.com/389629573): The "On*" methods below are used only for
  // logging purposes. Explore different approaches.
  virtual void OnSuggestionsShown(
      const DenseSet<SuggestionType>& shown_suggestion_types,
      const FormGlobalId& form_id) = 0;
  virtual void OnFormSeen(const FormStructure& form) = 0;
  virtual void OnDidFillSuggestion(FormGlobalId form_id) = 0;
  virtual void OnEditedAutofilledField(FormGlobalId form_id) = 0;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_INTEGRATORS_AUTOFILL_AI_DELEGATE_H_
