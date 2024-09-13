// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_PREDICTION_IMPROVEMENTS_DELEGATE_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_PREDICTION_IMPROVEMENTS_DELEGATE_H_

#include "components/autofill/core/browser/field_filling_skip_reason.h"
#include "components/autofill/core/browser/filling_product.h"
#include "components/autofill/core/browser/ui/suggestion.h"
#include "components/autofill/core/common/unique_ids.h"

namespace optimization_guide::proto {
class UserAnnotationsEntry;
}

namespace autofill {

class FormData;
class FormFieldData;
class FormStructure;

// The interface for communication from //components/autofill to
// //components/autofill/autofill_prediction_improvements.
class AutofillPredictionImprovementsDelegate {
 public:
  using HasData = base::StrongAlias<class HasDataTag, bool>;
  using HasDataCallback = base::OnceCallback<void(HasData)>;

  // Specifies the types of feedback users can give.
  enum class UserFeedback { kThumbsUp, kThumbsDown };

  using UpdateSuggestionsCallback =
      base::RepeatingCallback<void(std::vector<autofill::Suggestion>,
                                   autofill::AutofillSuggestionTriggerSource)>;
  // `ImportFormCallback` carries `to_be_upserted_entries` that will be shown in
  // the Autofill prediction improvements prompt. The prompt then notifies the
  // `UserAnnotationsService` about the user decision by running
  // `prompt_acceptance_callback`, that is also provided by
  // `ImportFormCallback`.
  using ImportFormCallback = base::OnceCallback<void(
      std::vector<optimization_guide::proto::UserAnnotationsEntry>
          to_be_upserted_entries,
      base::OnceCallback<void(bool prompt_was_accepted)>
          prompt_acceptance_callback)>;

  virtual ~AutofillPredictionImprovementsDelegate() = default;

  // Returns `true` if it set or updated `address_suggestions`. That will happen
  // if there are cached prediction improvements for `field` or
  // `should_add_trigger_suggestion` is `true`.
  virtual bool MaybeUpdateSuggestions(
      std::vector<Suggestion>& address_suggestions,
      const FormFieldData& field,
      bool should_add_trigger_suggestion) = 0;

  // Returns whether `form` is eligible for the improved prediction experience.
  virtual bool IsFormEligible(const FormStructure& form) = 0;

  // Returns `true` if the corresponding feature is enabled and optimization can
  // be applied.
  virtual bool ShouldProvidePredictionImprovements(const GURL& url) = 0;

  // Called when a feedback about the feature is given by the user.
  virtual void UserFeedbackReceived(UserFeedback feedback) = 0;

  // Called when users click the "learn more" link.
  virtual void UserClickedLearnMore() = 0;

  // Called when the `SuggestionType::kRetrievePredictionImprovements`
  // suggestion was accepted.
  virtual void OnClickedTriggerSuggestion(
      const autofill::FormData& form,
      const autofill::FormFieldData& trigger_field,
      UpdateSuggestionsCallback update_suggestions_callback) = 0;

  // Forwards `form` and `callback` to the user annotations service which calls
  // `callback` with its response.
  virtual void MaybeImportForm(const autofill::FormData& form,
                               const autofill::FormStructure& form_structure,
                               ImportFormCallback callback) = 0;

  // Checks if there is any data stored in the profile's user annotations that can be used for
  // filling and runs the `callback` accordingly.
  virtual void HasDataStored(HasDataCallback callback) = 0;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_PREDICTION_IMPROVEMENTS_DELEGATE_H_
