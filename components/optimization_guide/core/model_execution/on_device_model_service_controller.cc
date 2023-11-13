// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/model_execution/on_device_model_service_controller.h"

#include "base/task/thread_pool.h"
#include "components/optimization_guide/core/model_execution/on_device_model_execution_config_interpreter.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "services/on_device_model/public/cpp/model_assets.h"
#include "services/on_device_model/public/mojom/on_device_model.mojom.h"

namespace optimization_guide {
namespace {

class OnDeviceSession
    : public optimization_guide::OptimizationGuideModelExecutor::Session,
      public on_device_model::mojom::StreamingResponder {
 public:
  explicit OnDeviceSession(
      mojo::PendingRemote<on_device_model::mojom::Session> session,
      proto::ModelExecutionFeature feature,
      const OnDeviceModelExecutionConfigInterpreter* config_interpreter)
      : session_(std::move(session)),
        feature_(feature),
        config_interpreter_(config_interpreter) {}
  ~OnDeviceSession() override = default;

  void SetDisconnectHandler(base::OnceClosure on_disconnect) override {
    session_.set_disconnect_handler(std::move(on_disconnect));
  }

  // optimization_guide::OptimizationGuideModelExecutor::Session:
  void AddContext(
      const google::protobuf::MessageLite& request_metadata) override {
    auto input = config_interpreter_->ConstructInputString(
        feature_, request_metadata, /*want_input_context=*/true);
    if (!input) {
      // TODO(b/302402576): Add error handling.
      LOG(ERROR) << "Error constructing input string.";
      return;
    }
    // TODO(b/304890244): Handle passing context until request comes in.
    session_->AddContext(
        on_device_model::mojom::InputOptions::New(
            input->input_string, /*max_tokens=*/1024,
            /*token_offset=*/std::nullopt, input->should_ignore_input_context),
        {});
  }

  void ExecuteModel(
      const google::protobuf::MessageLite& request_metadata,
      optimization_guide::OptimizationGuideModelExecutionResultStreamingCallback
          callback) override {
    auto input = config_interpreter_->ConstructInputString(
        feature_, request_metadata, /*want_input_context=*/false);
    if (!input) {
      // TODO(b/302402576): Add error handling.
      LOG(ERROR) << "Error constructing input string.";
      return;
    }

    // Make sure to cancel any pending response.
    Reset();

    callback_ = std::move(callback);
    session_->Execute(
        on_device_model::mojom::InputOptions::New(
            input->input_string,
            /*max_tokens=*/std::nullopt, /*token_offset=*/std::nullopt,
            input->should_ignore_input_context),
        receiver_.BindNewPipeAndPassRemote());
    receiver_.set_disconnect_handler(
        base::BindOnce(&OnDeviceSession::OnError, base::Unretained(this)));
  }

  // on_device_model::mojom::StreamingResponder:
  void OnResponse(const std::string& response) override {
    current_response_ += response;
    SendResponse(/*is_complete=*/false);
  }

  void OnComplete() override {
    SendResponse(/*is_complete=*/true);
    Reset();
  }

 private:
  void Reset() {
    receiver_.reset();
    callback_.Reset();
    current_response_ = "";
  }

  void OnError() {
    if (callback_) {
      callback_.Run(
          base::unexpected(
              OptimizationGuideModelExecutionError::FromModelExecutionError(
                  OptimizationGuideModelExecutionError::ModelExecutionError::
                      kGenericFailure)),
          nullptr);
    }
    Reset();
  }

  void SendResponse(bool is_complete) {
    if (!callback_) {
      return;
    }

    auto output = config_interpreter_->ConstructOutputMetadata(
        feature_, current_response_);
    if (!output) {
      OnError();
      return;
    }

    // TODO(b/302327957): Add logging.
    callback_.Run(
        StreamingResponse{
            .response = *output,
            .is_complete = is_complete,
        },
        nullptr);
  }

  mojo::Remote<on_device_model::mojom::Session> session_;
  const proto::ModelExecutionFeature feature_;
  raw_ptr<const OnDeviceModelExecutionConfigInterpreter> config_interpreter_;

  // These fields handle the currently active response.
  optimization_guide::OptimizationGuideModelExecutionResultStreamingCallback
      callback_;
  mojo::Receiver<on_device_model::mojom::StreamingResponder> receiver_{this};
  std::string current_response_;
};

}  // namespace

OnDeviceModelServiceController::OnDeviceModelServiceController() = default;
OnDeviceModelServiceController::~OnDeviceModelServiceController() = default;

void OnDeviceModelServiceController::Init(
    const base::FilePath& model_path,
    std::unique_ptr<OnDeviceModelExecutionConfigInterpreter>
        config_interpreter) {
  CHECK(model_path_.empty());
  model_path_ = model_path;
  config_interpreter_ = std::move(config_interpreter);
  config_interpreter_->UpdateConfigWithFileDir(model_path_);
}

std::unique_ptr<OptimizationGuideModelExecutor::Session>
OnDeviceModelServiceController::StartSession(
    proto::ModelExecutionFeature feature) {
  if (!base::FeatureList::IsEnabled(
          features::kOptimizationGuideOnDeviceModel) ||
      !config_interpreter_->HasConfigForFeature(feature)) {
    return nullptr;
  }
  if (!model_remote_) {
    LaunchService();
    base::ThreadPool::PostTaskAndReplyWithResult(
        FROM_HERE, {base::MayBlock()},
        base::BindOnce(&on_device_model::LoadModelAssets, model_path_),
        base::BindOnce(&OnDeviceModelServiceController::OnModelAssetsLoaded,
                       weak_ptr_factory_.GetWeakPtr(),
                       model_remote_.BindNewPipeAndPassReceiver()));
    model_remote_.reset_on_disconnect();
  }
  mojo::PendingRemote<on_device_model::mojom::Session> session;
  model_remote_->StartSession(session.InitWithNewPipeAndPassReceiver());
  return std::make_unique<OnDeviceSession>(std::move(session), feature,
                                           config_interpreter_.get());
}

void OnDeviceModelServiceController::OnModelAssetsLoaded(
    mojo::PendingReceiver<on_device_model::mojom::OnDeviceModel> model,
    on_device_model::ModelAssets assets) {
  if (!service_remote_) {
    return;
  }
  // TODO(b/302402959): Choose max_tokens based on device.
  service_remote_->LoadModel(
      on_device_model::mojom::LoadModelParams::New(std::move(assets),
                                                   /*max_tokens=*/4096),
      std::move(model),
      base::BindOnce(&OnDeviceModelServiceController::OnLoadModelResult,
                     weak_ptr_factory_.GetWeakPtr()));
}

void OnDeviceModelServiceController::OnLoadModelResult(
    const std::optional<std::string>& error) {
  if (error.has_value()) {
    // TODO(b/302402576): Add error handling.
    LOG(ERROR) << *error;
  }
}

}  // namespace optimization_guide
