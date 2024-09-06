// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_PREDICTION_IMPROVEMENTS_DELEGATE_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_PREDICTION_IMPROVEMENTS_DELEGATE_H_

#include "components/autofill/core/browser/filling_product.h"
#include "components/autofill/core/browser/form_filler.h"
#include "components/autofill/core/browser/ui/suggestion.h"
#include "components/autofill/core/common/unique_ids.h"

namespace autofill {

class FormData;
class FormFieldData;

// The interface for communication from //components/autofill to
// //components/autofill/autofill_prediction_improvements.
class AutofillPredictionImprovementsDelegate {
 public:
  // Specifies the types of feedback users can give.
  enum class UserFeedback { kThumbsUp, kThumbsDown };

  using FillPredictionsCallback = base::OnceCallback<void(
      mojom::ActionPersistence action_persistence,
      FillingProduct filling_product,
      const FieldTypeSet& field_types_to_fill,
      const DenseSet<autofill::FieldFillingSkipReason>& ignorable_skip_reasons,
      const FormData& form,
      const FormFieldData& trigger_field,
      const base::flat_map<FieldGlobalId, std::u16string>& values_to_fill)>;

  virtual ~AutofillPredictionImprovementsDelegate() = default;

  // Returns the prediction improvements suggestions if available for the
  // `field`.
  virtual std::vector<Suggestion> CreateFillingSuggestion(
      const FormFieldData& field) = 0;

  // Returns whether improved predictions exist for the `field`. Used to decide
  // whether a context menu entry is displayed or not.
  virtual bool HasImprovedPredictionsForField(const FormFieldData& field) = 0;

  // Whether improved predictions was used for the `field`. Mostly for metrics
  // logging.
  virtual bool UsedImprovedPredictionsForField(const FormFieldData& field) = 0;

  // Receives the predictions for all fields in `form`, then calls
  // `fill_callback` on each field.
  virtual void ExtractImprovedPredictionsForFormFields(
      const FormData& form,
      const FormFieldData& trigger_field,
      FillPredictionsCallback fill_callback) = 0;

  // Creates a suggestion shown while improved predictions are loaded.
  virtual std::vector<autofill::Suggestion> CreateLoadingSuggestion() = 0;

  // Creates a suggestion that calls `ExtractImprovedPredictionsForFormFields()`
  // when invoked.
  virtual std::vector<autofill::Suggestion> CreateTriggerSuggestion(
      bool add_separator) = 0;

  // Returns `true` if the corresponding feature is enabled and optimization can
  // be applied.
  virtual bool ShouldProvidePredictionImprovements(const GURL& url) = 0;

  // Called when a feedback about the feature is given by the user.
  virtual void UserFeedbackReceived(UserFeedback feedback) = 0;

  // Called when users click the "learn more" link.
  virtual void UserClickedLearnMore() = 0;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_PREDICTION_IMPROVEMENTS_DELEGATE_H_
