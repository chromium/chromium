// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_ON_DEVICE_MODEL_SERVICE_CONTROLLER_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_ON_DEVICE_MODEL_SERVICE_CONTROLLER_H_

#include <string_view>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "components/optimization_guide/core/optimization_guide_model_executor.h"
#include "components/optimization_guide/proto/model_execution.pb.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/on_device_model/public/mojom/on_device_model.mojom.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace base {
class FilePath;
}  // namespace base

namespace optimization_guide {
class OnDeviceModelExecutionConfigInterpreter;

// Controls the lifetime of the on-device model service, loading and unloading
// of the models, and executing them via the service.
//
// TODO(b/302402576): Handle unloading the model, and stopping the service. The
// StreamingResponder should notify the controller upon completion to accomplish
// this. Also handle multiple requests gracefully and fail the subsequent
// requests, while handling the first one.
class OnDeviceModelServiceController {
 public:
  OnDeviceModelServiceController();
  virtual ~OnDeviceModelServiceController();

  // Initializes the on-device model controller with the parameters, to be ready
  // to load models and execute.
  void Init(const base::FilePath& model_path,
            std::unique_ptr<OnDeviceModelExecutionConfigInterpreter>
                config_interpreter);

  // Starts a session for `feature`. This will start the service and load the
  // model if it is not already loaded. The session will handle updating
  // context, executing input, and sending the response.
  std::unique_ptr<OptimizationGuideModelExecutor::Session> StartSession(
      proto::ModelExecutionFeature feature);

  // Launches the on-device model-service.
  virtual void LaunchService() = 0;

 private:
  friend class ChromeOnDeviceModelServiceController;
  friend class OnDeviceModelServiceControllerTest;
  friend class FakeOnDeviceModelServiceController;

  // Makes sure the service is running and starts a mojo session.
  void StartMojoSession(
      mojo::PendingReceiver<on_device_model::mojom::Session> session);

  // Invoked at the end of model load, to continue with model execution.
  void OnLoadModelResult(on_device_model::mojom::LoadModelResult result);

  // Called when the model assets have been loaded from disk and are ready to be
  // sent to the service.
  void OnModelAssetsLoaded(
      mojo::PendingReceiver<on_device_model::mojom::OnDeviceModel> model,
      on_device_model::ModelAssets assets);

  base::FilePath model_path_;
  std::unique_ptr<OnDeviceModelExecutionConfigInterpreter> config_interpreter_;
  mojo::Remote<on_device_model::mojom::OnDeviceModelService> service_remote_;
  mojo::Remote<on_device_model::mojom::OnDeviceModel> model_remote_;

  SEQUENCE_CHECKER(sequence_checker_);

  // Used to get `weak_ptr_` to self.
  base::WeakPtrFactory<OnDeviceModelServiceController> weak_ptr_factory_{this};
};

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_ON_DEVICE_MODEL_SERVICE_CONTROLLER_H_
