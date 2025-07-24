// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERMISSIONS_PREDICTION_SERVICE_PERMISSIONS_AIV3_HANDLER_H_
#define COMPONENTS_PERMISSIONS_PREDICTION_SERVICE_PERMISSIONS_AIV3_HANDLER_H_

#include "base/task/sequenced_task_runner.h"
#include "components/optimization_guide/core/inference/model_handler.h"
#include "components/optimization_guide/proto/models.pb.h"
#include "components/permissions/prediction_service/permissions_aiv3_encoder.h"
#include "components/permissions/prediction_service/permissions_aiv3_model_metadata.pb.h"
#include "components/permissions/prediction_service/prediction_model_metadata.pb.h"
#include "components/permissions/prediction_service/prediction_service_messages.pb.h"

namespace permissions {

class PermissionsAiv3Handler : public optimization_guide::ModelHandler<
                                   PermissionsAiv3Encoder::ModelOutput,
                                   const PermissionsAiv3Encoder::ModelInput&> {
 public:
  PermissionsAiv3Handler(
      optimization_guide::OptimizationGuideModelProvider* model_provider,
      optimization_guide::proto::OptimizationTarget optimization_target,
      RequestType request_type);
  PermissionsAiv3Handler(
      optimization_guide::OptimizationGuideModelProvider* model_provider,
      optimization_guide::proto::OptimizationTarget optimization_target,
      RequestType request_type,
      std::unique_ptr<PermissionsAiv3Encoder> model_executor,
      scoped_refptr<base::SequencedTaskRunner> model_executor_task_runner =
          base::ThreadPool::CreateSequencedTaskRunner(
              {base::MayBlock(), base::TaskPriority::USER_BLOCKING}),
      scoped_refptr<base::SequencedTaskRunner> reply_task_runner =
          base::SequencedTaskRunner::GetCurrentDefault());
  ~PermissionsAiv3Handler() override;

  PermissionsAiv3Handler(const PermissionsAiv3Handler&) = delete;
  PermissionsAiv3Handler& operator=(const PermissionsAiv3Handler&) = delete;

  void OnModelUpdated(
      optimization_guide::proto::OptimizationTarget optimization_target,
      base::optional_ref<const optimization_guide::ModelInfo> model_info)
      override;

  virtual void ExecuteModel(ExecutionCallback callback,
                            std::unique_ptr<SkBitmap> snapshot);

 private:
  // Called when the model execution is complete. This is a wrapper around the
  // callback provided to `ExecuteModel` that verifies that the callback is
  // still valid.
  void OnModelExecutionComplete(
      ExecutionCallback original_callback,
      const std::optional<PermissionRequestRelevance>& relevance);

  std::optional<PermissionsAiv3ModelMetadata> model_metadata_;

  // Because there is no way to cancel a model execution once it has started, we
  // will return an empty response to the new callback if a new execution is
  // requested while the previous one is still in progress.
  bool is_execution_in_progress_ = false;

  // Whether the callback passed to ExecuteModel is still valid. It is no longer
  // valid if a new execution is requested while the previous one is still in
  // progress.
  bool is_callback_valid_ = true;

  base::WeakPtrFactory<PermissionsAiv3Handler> weak_factory_{this};
};

}  // namespace permissions
#endif  // COMPONENTS_PERMISSIONS_PREDICTION_SERVICE_PERMISSIONS_AIV3_HANDLER_H_
