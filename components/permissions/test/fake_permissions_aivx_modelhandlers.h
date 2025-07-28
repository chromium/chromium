// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERMISSIONS_TEST_FAKE_PERMISSIONS_AIVX_MODELHANDLERS_H_
#define COMPONENTS_PERMISSIONS_TEST_FAKE_PERMISSIONS_AIVX_MODELHANDLERS_H_

#include <memory>
#include <string>

#include "base/run_loop.h"
#include "components/permissions/prediction_service/permissions_aiv3_handler.h"
#include "components/permissions/prediction_service/permissions_aiv4_handler.h"

// Contains fake classes to be used in tests for AIvX model handlers.
namespace test {

class PermissionsAiv3HandlerFake : public permissions::PermissionsAiv3Handler {
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

  void ExecuteModelWrapper(
      PermissionsAiv3Handler::ExecutionCallback callback,
      const std::optional<permissions::PermissionsAiv3Encoder::ModelOutput>&
          output);

  void ExecuteModel(PermissionsAiv3Handler::ExecutionCallback callback,
                    std::unique_ptr<SkBitmap> snapshot) override;

  void WaitForModelLoadForTesting();
  void WaitForModelExecutionForTesting();

 private:
  base::RunLoop model_execute_run_loop_for_testing_;
  base::RunLoop model_load_run_loop_for_testing_;
  base::WeakPtrFactory<PermissionsAiv3HandlerFake> weak_ptr_factory_{this};
};

}  // namespace test

#endif  // COMPONENTS_PERMISSIONS_TEST_FAKE_PERMISSIONS_AIVX_MODELHANDLERS_H_
