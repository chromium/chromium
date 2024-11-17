// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_AI_CORE_BROWSER_SUGGESTION_AUTOFILL_AI_MODEL_EXECUTOR_H_
#define COMPONENTS_AUTOFILL_AI_CORE_BROWSER_SUGGESTION_AUTOFILL_AI_MODEL_EXECUTOR_H_

#include "base/containers/flat_map.h"
#include "base/functional/callback_forward.h"
#include "base/types/expected.h"
#include "components/autofill/core/common/unique_ids.h"

namespace autofill {
class FormData;
}

namespace optimization_guide::proto {
class AXTreeUpdate;
}

namespace autofill_ai {

// The filling engine that provides autofill predictions improvements.
class AutofillAiModelExecutor {
 public:
  struct Prediction {
    Prediction(std::u16string value, std::u16string label, bool is_focusable);
    Prediction(std::u16string value,
               std::u16string label,
               bool is_focusable,
               std::optional<std::u16string> select_option_text);
    Prediction(const Prediction& other);
    ~Prediction();

    // The value to be filled into a field. Also shown as the main text in the
    // suggestion unless `select_option_text` is set.
    std::u16string value;
    // The label to be shown in the suggestion.
    std::u16string label;
    // True when the field targeted for this prediction is focusable.
    bool is_focusable;
    // Shown as main text in the suggestion if set.
    std::optional<std::u16string> select_option_text = std::nullopt;
  };
  using PredictionsByGlobalId =
      base::flat_map<autofill::FieldGlobalId, Prediction>;
  using PredictionsOrError = base::expected<PredictionsByGlobalId, bool>;
  using PredictionsReceivedCallback =
      base::OnceCallback<void(PredictionsOrError,
                              std::optional<std::string> feedback_id)>;

  virtual ~AutofillAiModelExecutor() = default;

  // Retrieves predictions for `form_data` with context of `ax_tree_update`.
  // Invokes `callback` when done. The unexpected value is always `false` if
  // there was an error retrieving predictions.
  virtual void GetPredictions(
      autofill::FormData form_data,
      base::flat_map<autofill::FieldGlobalId, bool> field_eligibility_map,
      base::flat_map<autofill::FieldGlobalId, bool> field_sensitivity_map,
      optimization_guide::proto::AXTreeUpdate ax_tree_update,
      PredictionsReceivedCallback callback) = 0;
};

}  // namespace autofill_ai

#endif  // COMPONENTS_AUTOFILL_AI_CORE_BROWSER_SUGGESTION_AUTOFILL_AI_MODEL_EXECUTOR_H_
