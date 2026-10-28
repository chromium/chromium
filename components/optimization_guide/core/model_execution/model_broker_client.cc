// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/model_execution/model_broker_client.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/containers/extend.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/to_string.h"
#include "base/trace_event/trace_event.h"
#include "base/types/pass_key.h"
#include "components/optimization_guide/core/model_execution/on_device_capability.h"
#include "components/optimization_guide/core/model_execution/on_device_context.h"
#include "components/optimization_guide/core/model_execution/on_device_features.h"
#include "components/optimization_guide/core/model_execution/session_impl.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/proto/on_device_model_execution_config.pb.h"
#include "components/optimization_guide/public/mojom/model_broker.mojom.h"
#include "mojo/public/cpp/base/proto_wrapper.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"
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

void RunAll(std::vector<base::OnceClosure> closures) {
  for (auto& closure : closures) {
    std::move(closure).Run();
  }
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
    // An execution can finish after its solution has been replaced. Its
    // ModelClient is then gone, so there is no current solution to notify.
    if (client_) {
      client_->remote_->ReportHealthyCompletion();
    }
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

std::unique_ptr<OnDeviceSession> ModelClient::CreateSession(
    const SessionConfigParams& config_params,
    base::WeakPtr<OptimizationGuideLogger> logger) {
  TRACE_EVENT("optimization_guide", "ModelClient::CreateSession");
  OnDeviceOptions opts;
  opts.model_client = std::make_unique<ModelClient::OnDeviceOptionsClient>(
      weak_ptr_factory_.GetWeakPtr());
  opts.model_versions = model_versions_;
  opts.adapter = feature_adapter_;
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
  RunAll(FlushModelClientCallbacks());
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
  RunAll(FlushCanCreateSessionCallbacks());
}

void ModelSubscriberImpl::Unavailable(
    mojom::ModelUnavailableReason reason,
    std::optional<mojom::ModelNotSupportedDetailedReason> detailed_reason) {
  TRACE_EVENT("optimization_guide", "ModelSubscriberImpl::Unavailable",
              perfetto::Flow::FromPointer(this), "reason", reason);
  unavailable_reason_ = reason;
  detailed_reason_ = detailed_reason;
  client_.reset();
  RunAll(FlushAllCallbacks());
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
  RunAll(FlushAllCallbacks());
}

void ModelSubscriberImpl::CapabilitiesUpdated(
    const on_device_model::Capabilities& capabilities) {
  TRACE_EVENT("optimization_guide", "ModelSubscriberImpl::CapabilitiesUpdated",
              perfetto::Flow::FromPointer(this));
  capabilities_ = capabilities;
  RunAll(FlushCanCreateSessionCallbacks());
}

std::vector<base::OnceClosure>
ModelSubscriberImpl::FlushModelClientCallbacks() {
  std::vector<base::OnceClosure> closures;
  if (client_) {
    base::WeakPtr<ModelClient> client = client_->GetWeakPtr();
    for (auto& callback : std::exchange(callbacks_, {})) {
      closures.push_back(base::BindOnce(std::move(callback), client));
    }
  } else if (unavailable_reason_ ==
             mojom::ModelUnavailableReason::kNotSupported) {
    for (auto& callback : std::exchange(callbacks_, {})) {
      closures.push_back(base::BindOnce(std::move(callback), nullptr));
    }
  }
  return closures;
}

std::vector<base::OnceClosure>
ModelSubscriberImpl::FlushCanCreateSessionCallbacks() {
  std::vector<base::OnceClosure> closures;
  // Wait until `CapabilitiesUpdated()` has reported device capabilities and
  // either `Available()` or `Unavailable()` has reported the solution state.
  const bool is_solution_state_known =
      client_.has_value() || unavailable_reason_.has_value();
  if (!capabilities_.has_value() || !is_solution_state_known) {
    return closures;
  }

  for (auto& [capability, callback] :
       std::exchange(can_create_session_callbacks_, {})) {
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

    closures.push_back(base::BindOnce(std::move(callback), unavailable_reason,
                                      detailed_reason));
  }
  return closures;
}

std::vector<base::OnceClosure> ModelSubscriberImpl::FlushAllCallbacks() {
  std::vector<base::OnceClosure> closures = FlushModelClientCallbacks();
  base::Extend(closures, FlushCanCreateSessionCallbacks());
  return closures;
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
  // Disconnects only occur if `BindModelBroker()` rejected the initial
  // connection (e.g. on-device execution is disabled or stubbed out) or if
  // `ModelBrokerImpl` is destroyed during browser shutdown.
  capabilities_.emplace();
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
  remote_->GetConfig(
      feature,
      mojo::WrapCallbackWithDefaultInvokeIfNotRun(std::move(callback),
                                                  std::nullopt));
}

void ModelBrokerClient::AddModelDownloadProgressObserver(
    const std::string& use_case,
    mojo::PendingRemote<on_device_model::mojom::DownloadObserver> observer) {
  remote_->AddModelDownloadProgressObserver(use_case, std::move(observer));
}
}  // namespace optimization_guide
