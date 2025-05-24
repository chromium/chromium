// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/model_execution/model_broker_client.h"

#include <memory>

#include "base/functional/callback_helpers.h"
#include "base/memory/weak_ptr.h"
#include "base/types/pass_key.h"
#include "components/optimization_guide/core/model_execution/feature_keys.h"
#include "components/optimization_guide/core/model_execution/on_device_context.h"
#include "components/optimization_guide/core/model_execution/safety_checker.h"
#include "components/optimization_guide/core/model_execution/session_impl.h"
#include "components/optimization_guide/core/optimization_guide_model_executor.h"
#include "components/optimization_guide/proto/on_device_model_execution_config.pb.h"
#include "components/optimization_guide/proto/text_safety_model_metadata.pb.h"
#include "components/optimization_guide/public/mojom/model_broker.mojom-shared.h"
#include "components/optimization_guide/public/mojom/model_broker.mojom.h"
#include "mojo/public/cpp/base/proto_wrapper.h"
#include "mojo/public/cpp/bindings/pending_remote.h"

namespace optimization_guide {

namespace {

std::unique_ptr<OptimizationGuideModelExecutor::Session>
CreateSessionWithParams(const CreateSessionArgs& args,
                        const std::optional<SessionConfigParams>& config_params,
                        base::WeakPtr<ModelClient> client) {
  if (!client) {
    return nullptr;
  }
  return client->CreateSession(args, config_params);
}

}  // namespace

CreateSessionArgs::CreateSessionArgs(
    base::WeakPtr<OptimizationGuideLogger> logger,
    ExecuteRemoteFn remote_fn)
    : logger_(std::move(logger)), remote_fn_(std::move(remote_fn)) {}
CreateSessionArgs::~CreateSessionArgs() = default;

CreateSessionArgs::CreateSessionArgs(const CreateSessionArgs&) = default;

class ModelClient::OnDeviceOptionsClient final
    : public OnDeviceOptions::Client {
 public:
  explicit OnDeviceOptionsClient(base::WeakPtr<ModelClient> client)
      : client_(std::move(client)) {}
  ~OnDeviceOptionsClient() override;

  std::unique_ptr<Client> Clone() const override {
    return std::make_unique<OnDeviceOptionsClient>(client_);
  }

  bool ShouldUse() override { return !!client_; }

  void StartSession(
      mojo::PendingReceiver<on_device_model::mojom::Session> pending,
      on_device_model::mojom::SessionParamsPtr params) override {
    client_->remote_->CreateSession(std::move(pending), std::move(params));
  }

  void OnResponseCompleted() override {
    client_->remote_->ReportHealthyCompletion();
    return;
  }

 private:
  base::WeakPtr<ModelClient> client_;
};

ModelClient::OnDeviceOptionsClient::~OnDeviceOptionsClient() = default;

ModelClient::ModelClient(mojo::PendingRemote<mojom::ModelSolution> remote,
                         mojom::ModelSolutionConfigPtr config)
    : remote_(std::move(remote)),
      feature_adapter_(base::MakeRefCounted<OnDeviceModelFeatureAdapter>(
          *config->feature_config
               .As<proto::OnDeviceModelExecutionFeatureConfig>())),
      safety_config_(*config->text_safety_config
                          .As<proto::FeatureTextSafetyConfiguration>()),
      model_versions_(
          *config->model_versions.As<proto::OnDeviceModelVersions>()),
      max_tokens_(config->max_tokens),
      key_(ToModelBasedCapabilityKey(feature_adapter_->config().feature())) {
  remote_.set_disconnect_handler(
      base::BindOnce(&ModelClient::OnDisconnect, base::Unretained(this)));
}
ModelClient::~ModelClient() = default;

void ModelClient::StartSession(
    mojo::PendingReceiver<on_device_model::mojom::TextSafetySession> session) {
  remote_->CreateTextSafetySession(std::move(session));
}

std::unique_ptr<OptimizationGuideModelExecutor::Session>
ModelClient::CreateSession(
    const CreateSessionArgs& args,
    const std::optional<SessionConfigParams>& config_params) {
  OnDeviceOptions opts;
  opts.model_client = std::make_unique<ModelClient::OnDeviceOptionsClient>(
      weak_ptr_factory_.GetWeakPtr());
  opts.model_versions = model_versions_;
  opts.adapter = feature_adapter_;
  opts.safety_checker = std::make_unique<SafetyChecker>(
      weak_ptr_factory_.GetWeakPtr(), SafetyConfig(safety_config_));
  opts.token_limits = feature_adapter_->GetTokenLimits();

  opts.logger = args.logger_;

  if (config_params) {
    opts.capabilities = config_params->capabilities;
    if (config_params->sampling_params) {
      opts.sampling_params = *config_params->sampling_params;
    }
  }

  return std::make_unique<SessionImpl>(key_, std::move(opts), args.remote_fn_,
                                       std::nullopt);
}

void ModelClient::OnDisconnect() {
  weak_ptr_factory_.InvalidateWeakPtrs();
}

ModelSubscriber::ModelSubscriber(
    mojo::PendingReceiver<mojom::ModelSubscriber> pending)
    : receiver_(this, std::move(pending)) {
  receiver_.set_disconnect_handler(
      base::BindOnce(&ModelSubscriber::Unavailable, base::Unretained(this),
                     mojom::ModelUnavailableReason::kNotSupported));
}
ModelSubscriber::~ModelSubscriber() = default;

void ModelSubscriber::CreateSession(
    const CreateSessionArgs& args,
    const std::optional<SessionConfigParams>& config_params,
    CreateSessionCallback callback) {
  WaitForClient(base::BindOnce(&CreateSessionWithParams, args, config_params)
                    .Then(std::move(callback)));
}

void ModelSubscriber::WaitForClient(ClientCallback callback) {
  callbacks_.emplace_back(std::move(callback));
  FlushCallbacks();
}

void ModelSubscriber::Unavailable(mojom::ModelUnavailableReason reason) {
  unavailable_reason_ = reason;
  client_.reset();
  FlushCallbacks();
}

void ModelSubscriber::Available(
    mojom::ModelSolutionConfigPtr config,
    mojo::PendingRemote<mojom::ModelSolution> remote) {
  unavailable_reason_ = std::nullopt;
  client_.emplace(std::move(remote), std::move(config));
  FlushCallbacks();
}

void ModelSubscriber::FlushCallbacks() {
  if (client_) {
    std::vector to_call(std::move(callbacks_));
    callbacks_.clear();
    for (auto& callback : to_call) {
      std::move(callback).Run(client_->GetWeakPtr());
    }
    return;
  }
  if (unavailable_reason_ == mojom::ModelUnavailableReason::kNotSupported) {
    std::vector to_call(std::move(callbacks_));
    callbacks_.clear();
    for (auto& callback : to_call) {
      std::move(callback).Run(nullptr);
    }
    return;
  }
}

ModelBrokerClient::ModelBrokerClient(
    mojo::PendingRemote<mojom::ModelBroker> remote,
    CreateSessionArgs args)
    : remote_(std::move(remote)), args_(std::move(args)) {}
ModelBrokerClient::~ModelBrokerClient() = default;

ModelSubscriber& ModelBrokerClient::GetSubscriber(
    mojom::ModelBasedCapabilityKey key) {
  std::unique_ptr<ModelSubscriber>& ptr = subscribers_[key];
  if (!ptr) {
    mojo::PendingRemote<mojom::ModelSubscriber> pending;
    ptr = std::make_unique<ModelSubscriber>(
        pending.InitWithNewPipeAndPassReceiver());
    remote_->Subscribe(mojom::ModelSubscriptionOptions::New(key, true),
                       std::move(pending));
  }
  return *ptr;
}

void ModelBrokerClient::CreateSession(
    mojom::ModelBasedCapabilityKey key,
    const std::optional<SessionConfigParams>& config_params,
    CreateSessionCallback callback) {
  GetSubscriber(key).CreateSession(args_, std::move(config_params),
                                   std::move(callback));
}

}  // namespace optimization_guide
