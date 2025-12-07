// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERMISSIONS_PREDICTION_SERVICE_PERMISSIONS_AIV4_HANDLER_H_
#define COMPONENTS_PERMISSIONS_PREDICTION_SERVICE_PERMISSIONS_AIV4_HANDLER_H_

#include <string>

#include "base/task/sequenced_task_runner.h"
#include "base/timer/timer.h"
#include "components/optimization_guide/core/inference/model_handler.h"
#include "components/optimization_guide/proto/models.pb.h"
#include "components/permissions/prediction_service/permissions_aiv4_executor.h"
#include "components/permissions/prediction_service/permissions_aiv4_model_metadata.pb.h"
#include "components/permissions/prediction_service/prediction_model_metadata.pb.h"
#include "components/permissions/prediction_service/prediction_service_messages.pb.h"

namespace permissions {

class PermissionsAiv4Handler : public optimization_guide::ModelHandler<
                                   PermissionsAiv4Executor::ModelOutput,
                                   const PermissionsAiv4Executor::ModelInput&> {
 public:
  // The timeout for the model execution. If the model execution takes longer
  // than this timeout, the callback will be called with a nullopt result.
  static const int kModelExecutionTimeout = 2;
  using ModelInput = PermissionsAiv4Executor::ModelInput;
  using ModelOutput = PermissionsAiv4Executor::ModelOutput;

  PermissionsAiv4Handler(
      optimization_guide::OptimizationGuideModelProvider* model_provider,
      optimization_guide::proto::OptimizationTarget optimization_target,
      RequestType request_type,
      const std::optional<download::SchedulingParams>& scheduling_params);
  PermissionsAiv4Handler(
      optimization_guide::OptimizationGuideModelProvider* model_provider,
      optimization_guide::proto::OptimizationTarget optimization_target,
      RequestType request_type,
      std::unique_ptr<PermissionsAiv4Executor> model_executor,
      const std::optional<download::SchedulingParams>& scheduling_params,
      scoped_refptr<base::SequencedTaskRunner> model_executor_task_runner =
          base::ThreadPool::CreateSequencedTaskRunner(
              {base::MayBlock(), base::TaskPriority::USER_BLOCKING}),
      scoped_refptr<base::SequencedTaskRunner> reply_task_runner =
          base::SequencedTaskRunner::GetCurrentDefault());
  ~PermissionsAiv4Handler() override;

  PermissionsAiv4Handler(const PermissionsAiv4Handler&) = delete;
  PermissionsAiv4Handler& operator=(const PermissionsAiv4Handler&) = delete;

  void OnModelUpdated(
      optimization_guide::proto::OptimizationTarget optimization_target,
      base::optional_ref<const optimization_guide::ModelInfo> model_info)
      override;

  virtual void ExecuteModel(ExecutionCallback callback, ModelInput model_input);

 private:
  // Called when the model execution is complete. This is a wrapper around the
  // callback provided to `ExecuteModel` that verifies that the callback is
  // still valid.
  void OnModelExecutionComplete(
      const std::optional<PermissionRequestRelevance>& relevance);

  // Called when the model execution times out.
  void OnModelExecutionTimeout(
      const std::optional<PermissionRequestRelevance>& relevance);

  std::optional<PermissionsAiv4ModelMetadata> model_metadata_;

  // Because there is no way to cancel a model execution once it has started, we
  // will return an empty response to the new callback if a new execution is
  // requested while the previous one is still in progress.
  bool is_execution_in_progress_ = false;

  // Whether the callback passed to ExecuteModel is still valid. It is no longer
  // valid if a new execution is requested while the previous one is still in
  // progress.
  bool is_callback_valid_ = true;

  base::OneShotTimer timeout_timer_;
  ExecutionCallback current_callback_;

  base::WeakPtrFactory<PermissionsAiv4Handler> weak_factory_{this};
};

}  // namespace permissions
#endif  // COMPONENTS_PERMISSIONS_PREDICTION_SERVICE_PERMISSIONS_AIV4_HANDLER_H_
