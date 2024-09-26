// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERMISSIONS_PREDICTION_SERVICE_PREDICTION_MODEL_HANDLER_H_
#define COMPONENTS_PERMISSIONS_PREDICTION_SERVICE_PREDICTION_MODEL_HANDLER_H_

#include "base/run_loop.h"
#include "components/optimization_guide/core/model_executor.h"
#include "components/optimization_guide/core/model_handler.h"
#include "components/optimization_guide/proto/models.pb.h"
#include "components/permissions/prediction_service/prediction_model_executor.h"
#include "components/permissions/prediction_service/prediction_model_metadata.pb.h"
#include "components/permissions/prediction_service/prediction_service_messages.pb.h"

namespace permissions {
class PredictionModelHandler : public optimization_guide::ModelHandler<
                                   GeneratePredictionsResponse,
                                   const PredictionModelExecutorInput&> {
 public:
  explicit PredictionModelHandler(
      optimization_guide::OptimizationGuideModelProvider* model_provider,
      optimization_guide::proto::OptimizationTarget optimization_target);

  ~PredictionModelHandler() override = default;
  PredictionModelHandler(const PredictionModelHandler&) = delete;
  PredictionModelHandler& operator=(const PredictionModelHandler&) = delete;

  // optimization_guide::ModelHandler overrides.
  void OnModelUpdated(
      optimization_guide::proto::OptimizationTarget optimization_target,
      base::optional_ref<const optimization_guide::ModelInfo> model_info)
      override;

  void WaitForModelLoadForTesting();

  void ExecuteModelWithMetadata(
      ExecutionCallback callback,
      std::unique_ptr<GeneratePredictionsRequest> proto_request);

 private:
  base::RunLoop model_load_run_loop_;

  std::optional<WebPermissionPredictionsModelMetadata> GetModelMetaData();

  std::unique_ptr<
      optimization_guide::ModelExecutor<GeneratePredictionsResponse,
                                        const PredictionModelExecutorInput&>>
  GetExecutor();
};

}  // namespace permissions
#endif  // COMPONENTS_PERMISSIONS_PREDICTION_SERVICE_PREDICTION_MODEL_HANDLER_H_
