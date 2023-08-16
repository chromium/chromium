// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/ml_model/autofill_model_handler.h"

#include <vector>

#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/ml_model/autofill_model_executor.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/optimization_guide/core/model_handler.h"
#include "components/optimization_guide/core/optimization_guide_model_provider.h"

namespace autofill {

AutofillModelHandler::AutofillModelHandler(
    optimization_guide::OptimizationGuideModelProvider* model_provider,
    const base::FilePath& dictionary_path)
    : optimization_guide::ModelHandler<ServerFieldType, const FormFieldData&>(
          model_provider,
          base::ThreadPool::CreateSequencedTaskRunner(
              {base::MayBlock(), base::TaskPriority::USER_VISIBLE}),
          std::make_unique<AutofillModelExecutor>(dictionary_path),
          /*model_inference_timeout=*/absl::nullopt,
          optimization_guide::proto::OptimizationTarget::
              OPTIMIZATION_TARGET_AUTOFILL_FIELD_CLASSIFICATION,
          /*model_metadata=*/absl::nullopt) {
  // Store the model in memory since we query predictions very regularly.
  // TODO(crbug.com/1465926): Maybe change to true if we see memory
  // regressions during the rollout.
  SetShouldUnloadModelOnComplete(false);
}
AutofillModelHandler::~AutofillModelHandler() = default;

void AutofillModelHandler::GetModelPredictionsForForm(
    const FormData& form_data,
    base::OnceCallback<void(const std::vector<ServerFieldType>&)> callback) {
  // According to the description of `BatchExecuteModelWithInput()`, it
  // should be used in-time sensitive applications. But since the bigger model
  // will eventually take FormData as input, the function will be called once.
  // TODO(crbug.com/1465926): Change to ExecuteModelWithInput once we switch to
  // bigger model.
  BatchExecuteModelWithInput(
      base::BindOnce(
          [](base::OnceCallback<void(const std::vector<ServerFieldType>&)>
                 outer_callback,
             const std::vector<absl::optional<ServerFieldType>>& outputs) {
            std::vector<ServerFieldType> predictions;
            predictions.reserve(outputs.size());
            for (const absl::optional<ServerFieldType>& output : outputs) {
              predictions.push_back(output.value_or(UNKNOWN_TYPE));
            }
            std::move(outer_callback).Run(predictions);
          },
          std::move(callback)),
      form_data.fields);
}

}  // namespace autofill
