// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_ON_DEVICE_MODEL_SERVICE_CONTROLLER_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_ON_DEVICE_MODEL_SERVICE_CONTROLLER_H_

#include <string_view>

#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/on_device_model/public/mojom/on_device_model.mojom.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace base {
class FilePath;
}  // namespace base

namespace optimization_guide {

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
  void Init(const base::FilePath& model_path);

  // Executes the model for `input` and the response will be sent to
  // `streaming_responder`. This will load the model if needed, before
  // execution.
  void Execute(std::string_view input,
               mojo::PendingRemote<on_device_model::mojom::StreamingResponder>
                   streaming_responder);

  // Launches the on-device model-service.
  virtual void LaunchService() = 0;

 private:
  friend class ChromeOnDeviceModelServiceController;
  friend class OnDeviceModelServiceControllerTest;
  friend class FakeOnDeviceModelServiceController;

  // Invoked at the end of model load, to continue with model execution.
  void OnLoadModelResult(
      std::string_view input,
      mojo::PendingRemote<on_device_model::mojom::StreamingResponder>
          streaming_responder,
      on_device_model::mojom::LoadModelResultPtr result);

  base::FilePath model_path_;
  mojo::Remote<on_device_model::mojom::OnDeviceModelService> service_remote_;
  mojo::Remote<on_device_model::mojom::OnDeviceModel> model_remote_;

  SEQUENCE_CHECKER(sequence_checker_);

  // Used to get `weak_ptr_` to self.
  base::WeakPtrFactory<OnDeviceModelServiceController> weak_ptr_factory_{this};
};

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_ON_DEVICE_MODEL_SERVICE_CONTROLLER_H_
