// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERMISSIONS_TEST_FAKE_PERMISSIONS_AIVX_MODELHANDLERS_H_
#define COMPONENTS_PERMISSIONS_TEST_FAKE_PERMISSIONS_AIVX_MODELHANDLERS_H_

#include <memory>
#include <string>

#include "base/run_loop.h"
#include "components/permissions/prediction_service/permissions_aiv3_executor.h"
#include "components/permissions/prediction_service/permissions_aiv3_handler.h"
#include "components/permissions/prediction_service/permissions_aiv4_executor.h"
#include "components/permissions/prediction_service/permissions_aiv4_handler.h"

// Contains fake classes to be used in tests for AIvX model handlers.
namespace test {

class PermissionsAivXHandlerFakeBase {
 public:
  // All AivX models share the same model execution callback for now
  using ExecutionCallback =
      permissions::PermissionsAiv3Handler::ExecutionCallback;
  PermissionsAivXHandlerFakeBase() = default;

  void ExecuteModelWrapper(
      ExecutionCallback callback,
      const std::optional<permissions::PermissionsAiv3Executor::ModelOutput>&
          output);

  void OnModelUpdated(
      base::optional_ref<const optimization_guide::ModelInfo> model_info);

  void WaitForModelLoadForTesting();
  void WaitForModelExecutionForTesting();

 protected:
  base::RunLoop model_execute_run_loop_for_testing_;
  base::RunLoop model_load_run_loop_for_testing_;
};

class PermissionsAiv3HandlerFake : public permissions::PermissionsAiv3Handler,
                                   public PermissionsAivXHandlerFakeBase {
 public:
  PermissionsAiv3HandlerFake(
      optimization_guide::OptimizationGuideModelProvider* model_provider,
      optimization_guide::proto::OptimizationTarget optimization_target,
      permissions::RequestType request_type);

  ~PermissionsAiv3HandlerFake() override;

  void OnModelUpdated(
      optimization_guide::proto::OptimizationTarget optimization_target,
      base::optional_ref<const optimization_guide::ModelInfo> model_info)
      override;

  void ExecuteModel(PermissionsAiv3Handler::ExecutionCallback callback,
                    ModelInput model_input) override;

 private:
  base::WeakPtrFactory<PermissionsAiv3HandlerFake> weak_ptr_factory_{this};
};

class PermissionsAiv4HandlerFake : public permissions::PermissionsAiv4Handler,
                                   public PermissionsAivXHandlerFakeBase {
 public:
  PermissionsAiv4HandlerFake(
      optimization_guide::OptimizationGuideModelProvider* model_provider,
      optimization_guide::proto::OptimizationTarget optimization_target,
      permissions::RequestType request_type);

  ~PermissionsAiv4HandlerFake() override;

  void OnModelUpdated(
      optimization_guide::proto::OptimizationTarget optimization_target,
      base::optional_ref<const optimization_guide::ModelInfo> model_info)
      override;

  void ExecuteModel(PermissionsAiv4Handler::ExecutionCallback callback,
                    ModelInput model_input) override;

 private:
  base::WeakPtrFactory<PermissionsAiv4HandlerFake> weak_ptr_factory_{this};
};

}  // namespace test

#endif  // COMPONENTS_PERMISSIONS_TEST_FAKE_PERMISSIONS_AIVX_MODELHANDLERS_H_
