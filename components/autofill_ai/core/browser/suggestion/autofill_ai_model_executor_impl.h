// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_AI_CORE_BROWSER_SUGGESTION_AUTOFILL_AI_MODEL_EXECUTOR_IMPL_H_
#define COMPONENTS_AUTOFILL_AI_CORE_BROWSER_SUGGESTION_AUTOFILL_AI_MODEL_EXECUTOR_IMPL_H_

#include <memory>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/types/expected.h"
#include "components/autofill_ai/core/browser/suggestion/autofill_ai_model_executor.h"
#include "components/optimization_guide/core/optimization_guide_model_executor.h"
#include "components/optimization_guide/proto/features/forms_predictions.pb.h"
#include "components/user_annotations/user_annotations_types.h"

namespace autofill {
class FormData;
}  // namespace autofill

namespace optimization_guide::proto {
class AXTreeUpdate;
class FilledFormData;
}  // namespace optimization_guide::proto

namespace user_annotations {
class UserAnnotationsService;
}  // namespace user_annotations

namespace autofill_ai {

class AutofillAiModelExecutorImpl : public AutofillAiModelExecutor {
 public:
  AutofillAiModelExecutorImpl(
      optimization_guide::OptimizationGuideModelExecutor* model_executor,
      user_annotations::UserAnnotationsService* user_annotations_service);
  ~AutofillAiModelExecutorImpl() override;

  // AutofillAiModelExecutor:
  void GetPredictions(
      autofill::FormData form_data,
      base::flat_map<autofill::FieldGlobalId, bool> field_eligibility_map,
      base::flat_map<autofill::FieldGlobalId, bool> field_sensitivity_map,
      optimization_guide::proto::AXTreeUpdate ax_tree_update,
      PredictionsReceivedCallback callback) override;
  const std::optional<optimization_guide::proto::FormsPredictionsRequest>&
  GetLatestRequest() const override;
  const std::optional<optimization_guide::proto::FormsPredictionsResponse>&
  GetLatestResponse() const override;

 private:
  // Invokes `callback` when user annotations were retrieved.
  void OnUserAnnotationsRetrieved(
      autofill::FormData form_data,
      const base::flat_map<autofill::FieldGlobalId, bool>&
          field_eligibility_map,
      const base::flat_map<autofill::FieldGlobalId, bool>&
          field_sensitivity_map,
      optimization_guide::proto::AXTreeUpdate ax_tree_update,
      PredictionsReceivedCallback callback,
      user_annotations::UserAnnotationsEntries user_annotations);

  // Invokes `callback` when model execution response has been returned.
  void OnModelExecuted(
      autofill::FormData form_data,
      PredictionsReceivedCallback callback,
      optimization_guide::OptimizationGuideModelExecutionResult
          execution_result,
      std::unique_ptr<optimization_guide::ModelQualityLogEntry> log_entry);

  static PredictionsByGlobalId ExtractPredictions(
      const autofill::FormData& form_data,
      const optimization_guide::proto::FilledFormData& form_data_proto);

  // Setter for `latest_request_`. Also resets `latest_response_`.
  void SetLatestRequestForDebugging(
      optimization_guide::proto::FormsPredictionsRequest request);

  // Setter for `latest_response_`.
  // Note that within a tab and thus per model executor instance, there cannot
  // be multiple requests as per the
  // `AutofillAiManager::prediction_retrieval_state_`.
  void SetLatestResponseForDebugging(
      std::optional<optimization_guide::proto::FormsPredictionsResponse>
          response);

  // Latest request made to the optimization guide.
  std::optional<optimization_guide::proto::FormsPredictionsRequest>
      latest_request_ = std::nullopt;

  // Response received for `latest_request_`, if any.
  std::optional<optimization_guide::proto::FormsPredictionsResponse>
      latest_response_ = std::nullopt;

  raw_ptr<optimization_guide::OptimizationGuideModelExecutor> model_executor_ =
      nullptr;
  raw_ptr<user_annotations::UserAnnotationsService> user_annotations_service_ =
      nullptr;

  base::WeakPtrFactory<AutofillAiModelExecutorImpl> weak_ptr_factory_{this};
};

}  // namespace autofill_ai

#endif  // COMPONENTS_AUTOFILL_AI_CORE_BROWSER_SUGGESTION_AUTOFILL_AI_MODEL_EXECUTOR_IMPL_H_
