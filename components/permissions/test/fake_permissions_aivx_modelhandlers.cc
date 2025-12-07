// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "components/permissions/test/fake_permissions_aivx_modelhandlers.h"

#include <memory>
#include <string>

#include "components/permissions/prediction_service/permissions_aiv3_executor.h"

namespace test {
using permissions::PermissionsAiv3Executor;
using permissions::PermissionsAiv3Handler;
using permissions::PermissionsAiv4Executor;
using permissions::PermissionsAiv4Handler;

inline PermissionsAiv3HandlerFake::~PermissionsAiv3HandlerFake() = default;
inline PermissionsAiv4HandlerFake::~PermissionsAiv4HandlerFake() = default;

void PermissionsAivXHandlerFakeBase::ExecuteModelWrapper(
    PermissionsAivXHandlerFakeBase::ExecutionCallback callback,
    const std::optional<PermissionsAiv3Executor::ModelOutput>& output) {
  std::move(callback).Run(output);
  model_execute_run_loop_for_testing_.Quit();
}

void PermissionsAivXHandlerFakeBase::WaitForModelLoadForTesting() {
  model_load_run_loop_for_testing_.Run();
}

void PermissionsAivXHandlerFakeBase::WaitForModelExecutionForTesting() {
  model_execute_run_loop_for_testing_.Run();
}

void PermissionsAivXHandlerFakeBase::OnModelUpdated(
    base::optional_ref<const optimization_guide::ModelInfo> model_info) {
  if (model_info.has_value()) {
    model_load_run_loop_for_testing_.Quit();
  }
}

PermissionsAiv3HandlerFake::PermissionsAiv3HandlerFake(
    optimization_guide::OptimizationGuideModelProvider* model_provider,
    optimization_guide::proto::OptimizationTarget optimization_target,
    permissions::RequestType request_type)
    : PermissionsAiv3Handler(
          model_provider,
          optimization_target,
          request_type,
          std::make_unique<PermissionsAiv3Executor>(request_type)) {}

void PermissionsAiv3HandlerFake::OnModelUpdated(
    optimization_guide::proto::OptimizationTarget optimization_target,
    base::optional_ref<const optimization_guide::ModelInfo> model_info) {
  PermissionsAiv3Handler::OnModelUpdated(optimization_target, model_info);
  PermissionsAivXHandlerFakeBase::OnModelUpdated(model_info);
}
void PermissionsAiv3HandlerFake::ExecuteModel(
    PermissionsAiv3Handler::ExecutionCallback callback,
    ModelInput model_input) {
  PermissionsAiv3Handler::ExecuteModel(
      base::BindOnce(&PermissionsAivXHandlerFakeBase::ExecuteModelWrapper,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)),
      std::move(model_input));
}

PermissionsAiv4HandlerFake::PermissionsAiv4HandlerFake(
    optimization_guide::OptimizationGuideModelProvider* model_provider,
    optimization_guide::proto::OptimizationTarget optimization_target,
    permissions::RequestType request_type)
    : PermissionsAiv4Handler(model_provider,
                             optimization_target,
                             request_type,
                             /*scheduling_params=*/std::nullopt) {}

void PermissionsAiv4HandlerFake::OnModelUpdated(
    optimization_guide::proto::OptimizationTarget optimization_target,
    base::optional_ref<const optimization_guide::ModelInfo> model_info) {
  PermissionsAiv4Handler::OnModelUpdated(optimization_target, model_info);
  PermissionsAivXHandlerFakeBase::OnModelUpdated(model_info);
}

void PermissionsAiv4HandlerFake::ExecuteModel(
    PermissionsAiv4Handler::ExecutionCallback callback,
    ModelInput model_input) {
  PermissionsAiv4Handler::ExecuteModel(
      base::BindOnce(&PermissionsAivXHandlerFakeBase::ExecuteModelWrapper,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)),
      std::move(model_input));
}

}  // namespace test
