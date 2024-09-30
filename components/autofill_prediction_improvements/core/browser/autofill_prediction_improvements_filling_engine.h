// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_PREDICTION_IMPROVEMENTS_CORE_BROWSER_AUTOFILL_PREDICTION_IMPROVEMENTS_FILLING_ENGINE_H_
#define COMPONENTS_AUTOFILL_PREDICTION_IMPROVEMENTS_CORE_BROWSER_AUTOFILL_PREDICTION_IMPROVEMENTS_FILLING_ENGINE_H_

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

namespace autofill_prediction_improvements {

// The filling engine that provides autofill predictions improvements.
class AutofillPredictionImprovementsFillingEngine {
 public:
  struct Prediction {
    Prediction(std::u16string value, std::u16string label);
    Prediction(std::u16string value,
               std::u16string label,
               std::optional<std::u16string> select_option_text);
    Prediction(const Prediction& other);
    ~Prediction();

    // The value to be filled into a field. Also shown as the main text in the
    // suggestion unless `select_option_text` is set.
    std::u16string value;
    // The label to be shown in the suggestion.
    std::u16string label;
    // Shown as main text in the suggestion if set.
    std::optional<std::u16string> select_option_text = std::nullopt;

   private:
    // For tests to readably print an instance of this struct.
    friend void PrintTo(
        const AutofillPredictionImprovementsFillingEngine::Prediction&
            prediction,
        std::ostream* os);
  };
  using PredictionsByGlobalId =
      base::flat_map<autofill::FieldGlobalId, Prediction>;
  using PredictionsOrError = base::expected<PredictionsByGlobalId, bool>;
  using PredictionsReceivedCallback =
      base::OnceCallback<void(PredictionsOrError,
                              std::optional<std::string> feedback_id)>;

  virtual ~AutofillPredictionImprovementsFillingEngine() = default;

  // Retrieves predictions for `form_data` with context of `ax_tree_update`.
  // Invokes `callback` when done. The unexpected value is always `false` if
  // there was an error retrieving predictions.
  virtual void GetPredictions(
      autofill::FormData form_data,
      optimization_guide::proto::AXTreeUpdate ax_tree_update,
      PredictionsReceivedCallback callback) = 0;
};

}  // namespace autofill_prediction_improvements

#endif  // COMPONENTS_AUTOFILL_PREDICTION_IMPROVEMENTS_CORE_BROWSER_AUTOFILL_PREDICTION_IMPROVEMENTS_FILLING_ENGINE_H_
