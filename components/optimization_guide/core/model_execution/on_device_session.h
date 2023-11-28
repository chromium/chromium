// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_ON_DEVICE_SESSION_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_ON_DEVICE_SESSION_H_

#include <string>

#include "base/memory/weak_ptr.h"
#include "components/optimization_guide/core/model_execution/optimization_guide_model_execution_error.h"
#include "components/optimization_guide/core/optimization_guide_model_executor.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/on_device_model/public/mojom/on_device_model.mojom.h"

namespace optimization_guide {
class OnDeviceModelExecutionConfigInterpreter;
class OnDeviceModelServiceController;

// A session backed by the on device service.
class OnDeviceSession : public OptimizationGuideModelExecutor::Session,
                        public on_device_model::mojom::StreamingResponder {
 public:
  using StartSessionFn = base::RepeatingCallback<void(
      mojo::PendingReceiver<on_device_model::mojom::Session>)>;

  OnDeviceSession(
      StartSessionFn start_session_fn,
      proto::ModelExecutionFeature feature,
      const OnDeviceModelExecutionConfigInterpreter* config_interpreter,
      base::WeakPtr<OnDeviceModelServiceController> controller);
  ~OnDeviceSession() override;

  // optimization_guide::OptimizationGuideModelExecutor::Session:
  void AddContext(
      const google::protobuf::MessageLite& request_metadata) override;
  void ExecuteModel(
      const google::protobuf::MessageLite& request_metadata,
      optimization_guide::OptimizationGuideModelExecutionResultStreamingCallback
          callback) override;

  // on_device_model::mojom::StreamingResponder:
  void OnResponse(const std::string& response) override;
  void OnComplete() override;

 private:
  class ContextProcessor;

  // Gets the active session or restarts a session if the session is reset.
  on_device_model::mojom::Session& GetOrCreateSession();

  // Resets response state.
  void ResetResponse();

  // Cancels any pending response and resets response state.
  void CancelPendingResponse(
      OptimizationGuideModelExecutionError::ModelExecutionError error =
          OptimizationGuideModelExecutionError::ModelExecutionError::
              kCancelled);

  // Called when the connection to the service is dropped.
  void OnDisconnect();

  // Sends `current_response_` to the client.
  void SendResponse(bool is_complete);

  base::WeakPtr<OnDeviceModelServiceController> controller_;
  mojo::Remote<on_device_model::mojom::Session> session_;
  const proto::ModelExecutionFeature feature_;
  raw_ptr<const OnDeviceModelExecutionConfigInterpreter> config_interpreter_;
  StartSessionFn start_session_fn_;
  std::unique_ptr<ContextProcessor> context_processor_;

  // These fields handle the currently active response.
  optimization_guide::OptimizationGuideModelExecutionResultStreamingCallback
      callback_;
  mojo::Receiver<on_device_model::mojom::StreamingResponder> receiver_{this};
  std::string current_response_;
  base::TimeTicks start_;

  // If true, the context is added before execution. This is set to true if
  // a disconnect happens.
  bool add_context_before_execute_ = false;
};

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_ON_DEVICE_SESSION_H_
