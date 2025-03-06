// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_ML_MODEL_AUTOFILL_AI_AUTOFILL_AI_MODEL_EXECUTOR_IMPL_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_ML_MODEL_AUTOFILL_AI_AUTOFILL_AI_MODEL_EXECUTOR_IMPL_H_

#include <memory>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/types/expected.h"
#include "components/autofill/core/browser/ml_model/autofill_ai/autofill_ai_model_executor.h"
#include "components/optimization_guide/core/model_quality/model_quality_logs_uploader_service.h"
#include "components/optimization_guide/core/optimization_guide_model_executor.h"
#include "components/optimization_guide/proto/features/forms_classifications.pb.h"

namespace optimization_guide::proto {
class AXTreeUpdate;
}  // namespace optimization_guide::proto

namespace autofill {

class FormData;

class AutofillAiModelExecutorImpl : public AutofillAiModelExecutor {
 public:
  AutofillAiModelExecutorImpl(
      optimization_guide::OptimizationGuideModelExecutor* model_executor,
      optimization_guide::ModelQualityLogsUploaderService* logs_uploader);
  ~AutofillAiModelExecutorImpl() override;

  // AutofillAiModelExecutor:
  void GetPredictions(FormData form_data,
                      optimization_guide::proto::AXTreeUpdate ax_tree_update,
                      PredictionCallback callback) override;

 private:
  // Invokes `callback` when model execution response has been returned.
  void OnModelExecuted(
      FormData form_data,
      PredictionCallback callback,
      optimization_guide::OptimizationGuideModelExecutionResult
          execution_result,
      std::unique_ptr<
          optimization_guide::proto::FormsClassificationsLoggingData>
          logging_data);

  const raw_ref<optimization_guide::OptimizationGuideModelExecutor>
      model_executor_;
  base::WeakPtr<optimization_guide::ModelQualityLogsUploaderService>
      logs_uploader_;

  base::WeakPtrFactory<AutofillAiModelExecutorImpl> weak_ptr_factory_{this};
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_ML_MODEL_AUTOFILL_AI_AUTOFILL_AI_MODEL_EXECUTOR_IMPL_H_
