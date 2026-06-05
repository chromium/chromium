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
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/proto/on_device_model_execution_config.pb.h"
#include "components/optimization_guide/proto/text_safety_model_metadata.pb.h"
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
                         mojom::ModelSolutionConfigPtr config,
                         on_device_model::Capabilities device_capabilities)
    : remote_(std::move(remote)),
      feature_adapter_(base::MakeRefCounted<OnDeviceModelFeatureAdapter>(
          *config->feature_config
               .As<proto::OnDeviceModelExecutionFeatureConfig>())),
      safety_config_(*config->text_safety_config
                          .As<proto::FeatureTextSafetyConfiguration>()),
      model_versions_(
          *config->model_versions.As<proto::OnDeviceModelVersions>()),
      capabilities_(config->model_capabilities),
      feature_(ToOnDeviceFeature(feature_adapter_->config().feature())
                   .value_or(mojom::OnDeviceFeature::kTest)) {
  // Tool use is assumed supported since it is gated by RuntimeEnabledFeatures
  // in Blink. TODO(crbug.com/422803232): Expose actual model tool use
  // capability from model metadata instead of assuming support.
  capabilities_.RetainAll(device_capabilities);
  capabilities_.Put(on_device_model::CapabilityFlags::kToolUse);

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
ModelSubscriberImpl::~ModelSubscriberImpl() {
  TRACE_EVENT("optimization_guide", "ModelSubscriberImpl::~ModelSubscriberImpl",
              perfetto::TerminatingFlow::FromPointer(this));
}

void ModelSubscriberImpl::CreateSession(
    const SessionConfigParams& config_params,
    CreateSessionCallback callback,
    base::WeakPtr<OptimizationGuideLogger> logger) {
  WaitForClient(base::BindOnce(&CreateSessionWithParams, config_params, logger)
                    .Then(std::move(callback)));
}

void ModelSubscriberImpl::WaitForClient(ClientCallback callback) {
  TRACE_EVENT("optimization_guide", "ModelSubscriberImpl::WaitForClient",
              perfetto::Flow::FromPointer(this));
  callbacks_.emplace_back(std::move(callback));
  FlushCallbacks();
}

void ModelSubscriberImpl::CanCreateSession(
    const on_device_model::Capabilities& capabilities,
    CanCreateSessionCallback callback) {
  TRACE_EVENT("optimization_guide", "ModelSubscriberImpl::CanCreateSession",
              perfetto::Flow::FromPointer(this));
  if (!features::IsOnDeviceExecutionEnabled()) {
    std::move(callback).Run(
        mojom::ModelUnavailableReason::kNotSupported,
        mojom::ModelNotSupportedDetailedReason::kFeatureNotEnabled);
    return;
  }

  can_create_session_callbacks_.emplace_back(capabilities, std::move(callback));
  FlushCanCreateSessionCallbacks();
}

void ModelSubscriberImpl::Unavailable(
    mojom::ModelUnavailableReason reason,
    std::optional<mojom::ModelNotSupportedDetailedReason> detailed_reason) {
  TRACE_EVENT("optimization_guide", "ModelSubscriberImpl::Unavailable",
              perfetto::Flow::FromPointer(this), "reason", reason);
  unavailable_reason_ = reason;
  detailed_reason_ = detailed_reason;
  client_.reset();
  FlushCallbacks();
  FlushCanCreateSessionCallbacks();
}

void ModelSubscriberImpl::Available(
    mojom::ModelSolutionConfigPtr config,
    mojo::PendingRemote<mojom::ModelSolution> remote) {
  TRACE_EVENT("optimization_guide", "ModelSubscriberImpl::Available",
              perfetto::Flow::FromPointer(this));
  unavailable_reason_ = std::nullopt;
  detailed_reason_ = std::nullopt;
  client_.emplace(std::move(remote), std::move(config),
                  capabilities_.value_or(on_device_model::Capabilities()));
  FlushCallbacks();
  FlushCanCreateSessionCallbacks();
}

void ModelSubscriberImpl::CapabilitiesUpdated(
    const on_device_model::Capabilities& capabilities) {
  TRACE_EVENT("optimization_guide", "ModelSubscriberImpl::CapabilitiesUpdated",
              perfetto::Flow::FromPointer(this));
  capabilities_ = capabilities;
  FlushCanCreateSessionCallbacks();
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

void ModelSubscriberImpl::FlushCanCreateSessionCallbacks() {
  // Make sure |capabilities_| and solution both are updated.
  if (!capabilities_.has_value() ||
      (!client_ && !unavailable_reason_.has_value())) {
    return;
  }

  std::vector to_call(std::move(can_create_session_callbacks_));
  can_create_session_callbacks_.clear();

  for (auto& [capability, callback] : to_call) {
    std::optional<mojom::ModelUnavailableReason> unavailable_reason =
        unavailable_reason_;
    std::optional<mojom::ModelNotSupportedDetailedReason> detailed_reason =
        unavailable_reason_ == mojom::ModelUnavailableReason::kNotSupported
            ? detailed_reason_
            : std::nullopt;

    if ((client_ && !client_->capabilities().HasAll(capability)) ||
        !capabilities_.value().HasAll(capability)) {
      unavailable_reason = mojom::ModelUnavailableReason::kNotSupported;
      detailed_reason =
          mojom::ModelNotSupportedDetailedReason::kModelAdaptationNotAvailable;
    }

    std::move(callback).Run(unavailable_reason, detailed_reason);
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
  Unavailable(mojom::ModelUnavailableReason::kNotSupported, std::nullopt);
}

ModelBrokerClient::ModelBrokerClient(
    mojo::PendingRemote<mojom::ModelBroker> remote,
    base::WeakPtr<OptimizationGuideLogger> logger)
    : remote_(std::move(remote)), logger_(logger) {}
ModelBrokerClient::~ModelBrokerClient() = default;

ModelSubscriber& ModelBrokerClient::GetSubscriber(const std::string& use_case) {
  std::unique_ptr<ModelSubscriber>& ptr = subscribers_[use_case];
  if (!ptr) {
    TRACE_EVENT("optimization_guide", "ModelBrokerClient::CreateSubscriber",
                "use_case", use_case);
    mojo::PendingRemote<mojom::ModelSubscriber> pending;
    ptr = std::make_unique<ModelSubscriber>(
        pending.InitWithNewPipeAndPassReceiver());
    remote_->Subscribe(mojom::ModelSubscriptionOptions::New(use_case),
                       std::move(pending));
  }
  return *ptr;
}

ModelSubscriber& ModelBrokerClient::GetSubscriber(
    mojom::OnDeviceFeature feature) {
  return GetSubscriber(ToUseCaseName(feature));
}

void ModelBrokerClient::RequestAssetsFor(const std::string& use_case) {
  remote_->RequestAssetsFor(use_case);
}

void ModelBrokerClient::RequestAssetsFor(mojom::OnDeviceFeature feature) {
  TRACE_EVENT("optimization_guide", "ModelBrokerClient::RequestAssetsFor");
  RequestAssetsFor(ToUseCaseName(feature));
}

bool ModelBrokerClient::HasSubscriber(const std::string& use_case) {
  return subscribers_.contains(use_case);
}

bool ModelBrokerClient::HasSubscriber(mojom::OnDeviceFeature feature) {
  return HasSubscriber(ToUseCaseName(feature));
}

void ModelBrokerClient::CreateSession(const std::string& use_case,
                                      const SessionConfigParams& config_params,
                                      CreateSessionCallback callback) {
  RequestAssetsFor(use_case);
  GetSubscriber(use_case).CreateSession(std::move(config_params),
                                        std::move(callback), logger_);
}

void ModelBrokerClient::CreateSession(mojom::OnDeviceFeature feature,
                                      const SessionConfigParams& config_params,
                                      CreateSessionCallback callback) {
  CreateSession(ToUseCaseName(feature), std::move(config_params),
                std::move(callback));
}

void ModelBrokerClient::GetConfig(mojom::OnDeviceFeature feature,
                                  GetConfigCallback callback) {
  remote_->GetConfig(feature, std::move(callback));
}

void ModelBrokerClient::AddModelDownloadProgressObserver(
    const std::string& use_case,
    mojo::PendingRemote<on_device_model::mojom::DownloadObserver> observer) {
  remote_->AddModelDownloadProgressObserver(use_case, std::move(observer));
}
}  // namespace optimization_guide
