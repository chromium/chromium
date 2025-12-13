// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/model_execution/model_broker_client.h"

#include <memory>

#include "base/functional/callback_helpers.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/to_string.h"
#include "base/trace_event/trace_event.h"
#include "base/types/pass_key.h"
#include "components/optimization_guide/core/model_execution/on_device_capability.h"
#include "components/optimization_guide/core/model_execution/on_device_context.h"
#include "components/optimization_guide/core/model_execution/on_device_features.h"
#include "components/optimization_guide/core/model_execution/safety_checker.h"
#include "components/optimization_guide/core/model_execution/session_impl.h"
#include "components/optimization_guide/proto/on_device_model_execution_config.pb.h"
#include "components/optimization_guide/proto/text_safety_model_metadata.pb.h"
#include "components/optimization_guide/public/mojom/model_broker.mojom-shared.h"
#include "components/optimization_guide/public/mojom/model_broker.mojom.h"
#include "mojo/public/cpp/base/proto_wrapper.h"
#include "mojo/public/cpp/bindings/pending_remote.h"

namespace optimization_guide {

namespace {

std::unique_ptr<OnDeviceSession> CreateSessionWithParams(
    const SessionConfigParams& config_params,
    base::WeakPtr<OptimizationGuideLogger> logger,
    base::WeakPtr<ModelClient> client) {
  if (!client) {
    return nullptr;
  }
  return client->CreateSession(config_params, logger);
}

}  // namespace

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
      feature_(*ToOnDeviceFeature(feature_adapter_->config().feature())) {
  remote_.set_disconnect_handler(
      base::BindOnce(&ModelClient::OnDisconnect, base::Unretained(this)));
}
ModelClient::~ModelClient() = default;

void ModelClient::StartSession(
    mojo::PendingReceiver<on_device_model::mojom::TextSafetySession> session) {
  TRACE_EVENT("optimization_guide", "ModelClient::StartSession");
  remote_->CreateTextSafetySession(std::move(session));
}

std::unique_ptr<OnDeviceSession> ModelClient::CreateSession(
    const SessionConfigParams& config_params,
    base::WeakPtr<OptimizationGuideLogger> logger) {
  TRACE_EVENT("optimization_guide", "ModelClient::CreateSession");
  OnDeviceOptions opts;
  opts.model_client = std::make_unique<ModelClient::OnDeviceOptionsClient>(
      weak_ptr_factory_.GetWeakPtr());
  opts.model_versions = model_versions_;
  opts.adapter = feature_adapter_;
  opts.safety_checker = std::make_unique<SafetyChecker>(
      weak_ptr_factory_.GetWeakPtr(), SafetyConfig(safety_config_));
  opts.token_limits = feature_adapter_->GetTokenLimits();
  opts.logger = logger;
  opts.session_params = config_params;
  if (!opts.session_params.sampling_params) {
    opts.session_params.sampling_params =
        feature_adapter_->GetDefaultSamplingParams();
  }
  return std::make_unique<SessionImpl>(feature_, std::move(opts));
}

void ModelClient::OnDisconnect() {
  TRACE_EVENT("optimization_guide", "ModelClient::OnDisconnect");
  weak_ptr_factory_.InvalidateWeakPtrs();
}

ModelSubscriberImpl::ModelSubscriberImpl() = default;
ModelSubscriberImpl::~ModelSubscriberImpl() = default;

void ModelSubscriberImpl::CreateSession(
    const SessionConfigParams& config_params,
    CreateSessionCallback callback,
    base::WeakPtr<OptimizationGuideLogger> logger) {
  WaitForClient(base::BindOnce(&CreateSessionWithParams, config_params, logger)
                    .Then(std::move(callback)));
}

void ModelSubscriberImpl::WaitForClient(ClientCallback callback) {
  TRACE_EVENT("optimization_guide", "ModelSubscriberImpl::WaitForClient");
  callbacks_.emplace_back(std::move(callback));
  FlushCallbacks();
}

void ModelSubscriberImpl::Unavailable(mojom::ModelUnavailableReason reason) {
  TRACE_EVENT("optimization_guide", "ModelSubscriberImpl::Unavailable",
              "reason", reason);
  unavailable_reason_ = reason;
  client_.reset();
  FlushCallbacks();
}

void ModelSubscriberImpl::Available(
    mojom::ModelSolutionConfigPtr config,
    mojo::PendingRemote<mojom::ModelSolution> remote) {
  TRACE_EVENT("optimization_guide", "ModelSubscriberImpl::Available");
  unavailable_reason_ = std::nullopt;
  client_.emplace(std::move(remote), std::move(config));
  FlushCallbacks();
}

void ModelSubscriberImpl::FlushCallbacks() {
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

ModelSubscriber::ModelSubscriber(
    mojo::PendingReceiver<mojom::ModelSubscriber> pending)
    : receiver_(this, std::move(pending)) {
  receiver_.set_disconnect_handler(
      base::BindOnce(&ModelSubscriber::OnDisconnect, base::Unretained(this)));
}
ModelSubscriber::~ModelSubscriber() = default;

void ModelSubscriber::OnDisconnect() {
  TRACE_EVENT("optimization_guide", "ModelSubscriber::OnDisconnect");
  Unavailable(mojom::ModelUnavailableReason::kNotSupported);
}

ModelBrokerClient::ModelBrokerClient(
    mojo::PendingRemote<mojom::ModelBroker> remote,
    base::WeakPtr<OptimizationGuideLogger> logger)
    : remote_(std::move(remote)), logger_(logger) {}
ModelBrokerClient::~ModelBrokerClient() = default;

ModelSubscriber& ModelBrokerClient::GetSubscriber(
    mojom::OnDeviceFeature feature) {
  std::unique_ptr<ModelSubscriber>& ptr = subscribers_[feature];
  if (!ptr) {
    TRACE_EVENT("optimization_guide", "ModelBrokerClient::CreateSubscriber",
                "feature", base::ToString(feature));
    mojo::PendingRemote<mojom::ModelSubscriber> pending;
    ptr = std::make_unique<ModelSubscriber>(
        pending.InitWithNewPipeAndPassReceiver());
    remote_->Subscribe(mojom::ModelSubscriptionOptions::New(feature, true),
                       std::move(pending));
  }
  return *ptr;
}

bool ModelBrokerClient::HasSubscriber(mojom::OnDeviceFeature feature) {
  return subscribers_.contains(feature);
}

void ModelBrokerClient::CreateSession(mojom::OnDeviceFeature feature,
                                      const SessionConfigParams& config_params,
                                      CreateSessionCallback callback) {
  GetSubscriber(feature).CreateSession(std::move(config_params),
                                       std::move(callback), logger_);
}

}  // namespace optimization_guide
