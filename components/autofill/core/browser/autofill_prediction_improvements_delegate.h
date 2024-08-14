// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_PREDICTION_IMPROVEMENTS_DELEGATE_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_PREDICTION_IMPROVEMENTS_DELEGATE_H_

#include "components/autofill/core/browser/ui/suggestion.h"

namespace autofill {

class FormData;
class FormFieldData;

// The interface for communication from //components/autofill to
// //components/autofill/autofill_prediction_improvements.
class AutofillPredictionImprovementsDelegate {
 public:
  using FillPredictionsCallback =
      base::RepeatingCallback<void(mojom::ActionPersistence action_persistence,
                                   mojom::FieldActionType action_type,
                                   const FormData& form,
                                   const FormFieldData& field,
                                   const std::u16string& value,
                                   SuggestionType type,
                                   std::optional<FieldType> field_type_used)>;

  virtual ~AutofillPredictionImprovementsDelegate() = default;

  // Returns the prediction improvements suggestions if available for the
  // `field`.
  virtual std::vector<Suggestion> GetSuggestions(
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
      FillPredictionsCallback fill_callback) = 0;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_PREDICTION_IMPROVEMENTS_DELEGATE_H_
