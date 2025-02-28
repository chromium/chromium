// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_AI_CORE_BROWSER_SUGGESTION_AUTOFILL_AI_MODEL_EXECUTOR_IMPL_H_
#define COMPONENTS_AUTOFILL_AI_CORE_BROWSER_SUGGESTION_AUTOFILL_AI_MODEL_EXECUTOR_IMPL_H_

#include <memory>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/types/expected.h"
#include "components/autofill_ai/core/browser/suggestion/autofill_ai_model_executor.h"
#include "components/optimization_guide/core/model_quality/model_quality_logs_uploader_service.h"
#include "components/optimization_guide/core/optimization_guide_model_executor.h"
#include "components/optimization_guide/proto/features/forms_classifications.pb.h"

namespace autofill {
class FormData;
}  // namespace autofill

namespace optimization_guide::proto {
class AXTreeUpdate;
}  // namespace optimization_guide::proto

namespace autofill_ai {

class AutofillAiModelExecutorImpl : public AutofillAiModelExecutor {
 public:
  AutofillAiModelExecutorImpl(
      optimization_guide::OptimizationGuideModelExecutor* model_executor,
      optimization_guide::ModelQualityLogsUploaderService* logs_uploader);
  ~AutofillAiModelExecutorImpl() override;

  // AutofillAiModelExecutor:
  void GetPredictions(
      autofill::FormData form_data,
      optimization_guide::proto::AXTreeUpdate ax_tree_update,
      PredictionsReceivedCallback callback) override;
  const std::optional<optimization_guide::proto::AutofillAiTypeRequest>&
  GetLatestRequest() const override;
  const std::optional<optimization_guide::proto::AutofillAiTypeResponse>&
  GetLatestResponse() const override;

 private:
  // Invokes `callback` when model execution response has been returned.
  void OnModelExecuted(
      autofill::FormData form_data,
      PredictionsReceivedCallback callback,
      optimization_guide::OptimizationGuideModelExecutionResult
          execution_result,
      std::unique_ptr<
          optimization_guide::proto::FormsClassificationsLoggingData>
          logging_data);

  // TODO(crbug.com/389631477): Move into anonymous namespace.
  static PredictionsByGlobalId ExtractPredictions(
      const autofill::FormData& form_data,
      const std::optional<optimization_guide::proto::AutofillAiTypeResponse>&
          model_response);

  // TODO(crbug.com/389631477): Remove these and the state connected to them.
  void SetLatestRequestForDebugging(
      optimization_guide::proto::AutofillAiTypeRequest request);
  void SetLatestResponseForDebugging(
      std::optional<optimization_guide::proto::AutofillAiTypeResponse>
          response);

  // Latest request made to the optimization guide.
  std::optional<optimization_guide::proto::AutofillAiTypeRequest>
      latest_request_;

  // Response received for `latest_request_`, if any.
  std::optional<optimization_guide::proto::AutofillAiTypeResponse>
      latest_response_;

  const raw_ref<optimization_guide::OptimizationGuideModelExecutor>
      model_executor_;
  // TODO(crbug.com/389631477): Remove until we need it.
  base::WeakPtr<optimization_guide::ModelQualityLogsUploaderService>
      logs_uploader_;

  base::WeakPtrFactory<AutofillAiModelExecutorImpl> weak_ptr_factory_{this};
};

}  // namespace autofill_ai

#endif  // COMPONENTS_AUTOFILL_AI_CORE_BROWSER_SUGGESTION_AUTOFILL_AI_MODEL_EXECUTOR_IMPL_H_
