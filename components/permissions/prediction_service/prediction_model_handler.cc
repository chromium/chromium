// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/permissions/prediction_service/prediction_model_handler.h"

#include <memory>

#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/optimization_guide/core/optimization_guide_model_provider.h"
#include "components/optimization_guide/proto/models.pb.h"
#include "components/permissions/prediction_service/prediction_model_executor.h"
#include "components/permissions/prediction_service/prediction_request_features.h"
#include "content/public/browser/browser_context.h"

namespace permissions {

PredictionModelHandler::PredictionModelHandler(
    optimization_guide::OptimizationGuideModelProvider* model_provider,
    scoped_refptr<base::SequencedTaskRunner> background_task_runner)
    : ModelHandler<GeneratePredictionsResponse,
                   const GeneratePredictionsRequest&>(
          model_provider,
          background_task_runner,
          std::make_unique<PredictionModelExecutor>(),
          optimization_guide::proto::OptimizationTarget::
              OPTIMIZATION_TARGET_NOTIFICATION_PERMISSION_PREDICTIONS,
          absl::nullopt) {}

void PredictionModelHandler::OnModelUpdated(
    optimization_guide::proto::OptimizationTarget optimization_target,
    const optimization_guide::ModelInfo& model_info) {
  // First invoke parent to update internal status.
  optimization_guide::ModelHandler<
      GeneratePredictionsResponse,
      const GeneratePredictionsRequest&>::OnModelUpdated(optimization_target,
                                                         model_info);
  model_load_run_loop_.Quit();
}

void PredictionModelHandler::WaitForModelLoadForTesting() {
  model_load_run_loop_.Run();
}

}  // namespace permissions
