// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_PREDICTION_IMPROVEMENTS_CORE_BROWSER_AUTOFILL_PREDICTION_IMPROVEMENTS_FILLING_ENGINE_H_
#define COMPONENTS_AUTOFILL_PREDICTION_IMPROVEMENTS_CORE_BROWSER_AUTOFILL_PREDICTION_IMPROVEMENTS_FILLING_ENGINE_H_

#include "base/functional/callback_forward.h"
#include "base/types/expected.h"

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
  using PredictionsReceivedCallback =
      base::OnceCallback<void(base::expected<autofill::FormData, bool>)>;

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
