// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_ON_DEVICE_MODEL_SERVICE_CONTROLLER_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_ON_DEVICE_MODEL_SERVICE_CONTROLLER_H_

#include <memory>
#include <string_view>

#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/types/pass_key.h"
#include "components/optimization_guide/core/model_execution/session_impl.h"
#include "components/optimization_guide/core/optimization_guide_model_executor.h"
#include "components/optimization_guide/proto/model_execution.pb.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/on_device_model/public/mojom/on_device_model.mojom.h"
#include "services/on_device_model/public/mojom/on_device_model_service.mojom.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

class OptimizationGuideLogger;

namespace base {
class FilePath;
}  // namespace base

namespace optimization_guide {
class OnDeviceModelAccessController;
class OnDeviceModelExecutionConfigInterpreter;

// Controls the lifetime of the on-device model service, loading and unloading
// of the models, and executing them via the service.
//
// As all OnDeviceModelServiceController's share the same model, and we do not
// want to load duplicate models (would consume excessive amounts of memory), at
// most one instance of OnDeviceModelServiceController is created.
//
// TODO(b/302402576): Handle unloading the model, and stopping the service. The
// StreamingResponder should notify the controller upon completion to accomplish
// this. Also handle multiple requests gracefully and fail the subsequent
// requests, while handling the first one.
class OnDeviceModelServiceController
    : public base::RefCounted<OnDeviceModelServiceController> {
 public:
  explicit OnDeviceModelServiceController(
      std::unique_ptr<OnDeviceModelAccessController> access_controller);

  // Initializes the on-device model controller with the parameters, to be ready
  // to load models and execute.
  void Init(const base::FilePath& model_path,
            std::unique_ptr<OnDeviceModelExecutionConfigInterpreter>
                config_interpreter);
  // Calls multi-arg init with appropriate parameters.
  void Init();

  // Starts a session for `feature`. This will start the service and load the
  // model if it is not already loaded. The session will handle updating
  // context, executing input, and sending the response.
  std::unique_ptr<OptimizationGuideModelExecutor::Session> CreateSession(
      proto::ModelExecutionFeature feature,
      ExecuteRemoteFn execute_remote_fn,
      OptimizationGuideLogger* logger);

  // Launches the on-device model-service.
  virtual void LaunchService() = 0;

  // Starts the service and calls |callback| with the estimated performance
  // class. Will call with std::nullopt if the service crashes.
  using GetEstimatedPerformanceClassCallback = base::OnceCallback<void(
      std::optional<on_device_model::mojom::PerformanceClass>
          performance_class)>;
  void GetEstimatedPerformanceClass(
      GetEstimatedPerformanceClassCallback callback);

  OnDeviceModelAccessController* access_controller(base::PassKey<SessionImpl>) {
    return access_controller_.get();
  }

  bool ShouldStartNewSession() const;

  // Shuts down the service if there is no active model.
  void ShutdownServiceIfNoModelLoaded();

  bool IsConnectedForTesting() {
    return model_remote_.is_bound() || service_remote_.is_bound();
  }

 private:
  friend class base::RefCounted<OnDeviceModelServiceController>;
  friend class ChromeOnDeviceModelServiceController;
  friend class OnDeviceModelServiceControllerTest;
  friend class FakeOnDeviceModelServiceController;

  virtual ~OnDeviceModelServiceController();

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

  // Called when disconnected from the model.
  void OnDisconnected();

  // Called when the remote (either `service_remote_` or `_model_remote_` is
  // idle.
  void OnRemoteIdle();

  // This may be null in the destructor, otherwise non-null.
  std::unique_ptr<OnDeviceModelAccessController> access_controller_;
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
