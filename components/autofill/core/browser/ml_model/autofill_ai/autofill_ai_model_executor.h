// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_ML_MODEL_AUTOFILL_AI_AUTOFILL_AI_MODEL_EXECUTOR_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_ML_MODEL_AUTOFILL_AI_AUTOFILL_AI_MODEL_EXECUTOR_H_

#include <optional>

#include "base/functional/callback_forward.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/optimization_guide/proto/features/forms_classifications.pb.h"

namespace autofill {

class FormData;

// Responsible for managing calls to the AutofillAI server model via
// optimization guide infrastructure.
class AutofillAiModelExecutor : public KeyedService {
 public:
  using Predictions = optimization_guide::proto::AutofillAiTypeResponse;
  using PredictionCallback =
      base::OnceCallback<void(std::optional<Predictions>)>;

  // Retrieves predictions for `form_data`. Invokes `callback` when done and
  // also writes the result into the `AutofillAiModelCache` tied to the same
  // profile. If the model encountered an error, the callback is called with
  // `std::nullopt`.
  virtual void GetPredictions(
      FormData form_data,
      PredictionCallback callback) = 0;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_ML_MODEL_AUTOFILL_AI_AUTOFILL_AI_MODEL_EXECUTOR_H_
