// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_AI_CORE_BROWSER_SUGGESTION_AUTOFILL_AI_MODEL_EXECUTOR_H_
#define COMPONENTS_AUTOFILL_AI_CORE_BROWSER_SUGGESTION_AUTOFILL_AI_MODEL_EXECUTOR_H_

#include "base/containers/flat_map.h"
#include "base/functional/callback_forward.h"
#include "base/types/expected.h"
#include "components/autofill/core/common/unique_ids.h"
#include "components/optimization_guide/proto/features/forms_classifications.pb.h"

namespace autofill {
class FormData;
}

namespace optimization_guide::proto {
class AXTreeUpdate;
}  // namespace optimization_guide::proto

namespace autofill_ai {

// The filling engine that provides autofill predictions improvements.
class AutofillAiModelExecutor {
 public:
  using Predictions = optimization_guide::proto::AutofillAiTypeResponse;
  using PredictionCallback =
      base::OnceCallback<void(std::optional<Predictions>)>;

  virtual ~AutofillAiModelExecutor() = default;

  // Retrieves predictions for `form_data` with context of `ax_tree_update`.
  // Invokes `callback` when done. If the model encountered an error, the
  // callback's is called with `std::nullopt`.
  virtual void GetPredictions(
      autofill::FormData form_data,
      optimization_guide::proto::AXTreeUpdate ax_tree_update,
      PredictionCallback callback) = 0;
};

}  // namespace autofill_ai

#endif  // COMPONENTS_AUTOFILL_AI_CORE_BROWSER_SUGGESTION_AUTOFILL_AI_MODEL_EXECUTOR_H_
