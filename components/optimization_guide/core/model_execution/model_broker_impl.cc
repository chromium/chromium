// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/model_execution/model_broker_impl.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/strings/to_string.h"
#include "base/trace_event/trace_event.h"
#include "components/optimization_guide/core/model_execution/on_device_capability.h"
#include "components/optimization_guide/core/model_execution/on_device_features.h"
#include "components/optimization_guide/core/model_execution/usage_tracker.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/public/mojom/model_broker.mojom.h"
#include "components/optimization_guide/public/mojom/model_broker_debug.mojom.h"
#include "mojo/public/cpp/bindings/message.h"

namespace optimization_guide {

ModelBrokerImpl::ModelBrokerImpl(
    UsageTracker& usage_tracker,
    EnsureInitCallback ensure_init_callback,
    AddDownloadProgressObserverCallback add_download_progress_observer_callback)
    : usage_tracker_(usage_tracker),
      ensure_init_callback_(std::move(ensure_init_callback)),
      add_download_progress_observer_callback_(
          std::move(add_download_progress_observer_callback)) {}

ModelBrokerImpl::~ModelBrokerImpl() {
  TRACE_EVENT("optimization_guide", "ModelBrokerImpl::BindBroker",
              perfetto::TerminatingFlow::FromPointer(this));
}

void ModelBrokerImpl::BindBroker(
    mojo::PendingReceiver<mojom::ModelBroker> receiver) {
  TRACE_EVENT("optimization_guide", "ModelBrokerImpl::BindBroker",
              perfetto::Flow::FromPointer(this));
  receivers_.Add(this, std::move(receiver));
}

void ModelBrokerImpl::Subscribe(
    mojom::ModelSubscriptionOptionsPtr options,
    mojo::PendingRemote<mojom::ModelSubscriber> subscriber) {
  TRACE_EVENT("optimization_guide", "ModelBrokerImpl::Subscribe",
              perfetto::Flow::FromPointer(this));
  ensure_init_callback_.Run(
      base::BindOnce(&ModelBrokerImpl::SubscribeInternal,
                     weak_ptr_factory_.GetWeakPtr(), std::move(options),
                     std::move(subscriber), mojo::GetBadMessageCallback()));
}

void ModelBrokerImpl::SubscribeInternal(
    mojom::ModelSubscriptionOptionsPtr options,
    mojo::PendingRemote<mojom::ModelSubscriber> subscriber,
    mojo::ReportBadMessageCallback bad_message_callback,
    const on_device_model::Capabilities& capabilities) {
  TRACE_EVENT("optimization_guide", "ModelBrokerImpl::SubscribeInternal",
              perfetto::Flow::FromPointer(this));
  const std::string& use_case = options->use_case;
  if (!solution_providers_.contains(use_case) &&
      !GetFeatureForUseCase(use_case)) {
    std::move(bad_message_callback).Run("Unsupported use case");
    return;
  }
  GetSolutionProvider(use_case).AddSubscriber(std::move(subscriber),
                                              capabilities);
}

void ModelBrokerImpl::GetConfig(mojom::OnDeviceFeature feature,
                                GetConfigCallback callback) {
  TRACE_EVENT("optimization_guide", "ModelBrokerImpl::GetConfig");
  ensure_init_callback_.Run(base::BindOnce(&ModelBrokerImpl::GetConfigInternal,
                                           weak_ptr_factory_.GetWeakPtr(),
                                           feature, std::move(callback)));
}

void ModelBrokerImpl::GetConfigInternal(
    mojom::OnDeviceFeature feature,
    GetConfigCallback callback,
    const on_device_model::Capabilities& capabilities) {
  TRACE_EVENT("optimization_guide", "ModelBrokerImpl::GetConfigInternal");
  auto it = feature_configs_.find(feature);
  if (it == feature_configs_.end()) {
    std::move(callback).Run(std::nullopt);
    return;
  }
  std::move(callback).Run(mojo_base::ProtoWrapper(it->second));
}

void ModelBrokerImpl::RequestAssetsFor(const std::string& use_case) {
  TRACE_EVENT("optimization_guide", "ModelBrokerImpl::RequestAssetsFor",
              perfetto::Flow::FromPointer(this));
  ensure_init_callback_.Run(base::BindOnce(
      &ModelBrokerImpl::RequestAssetsForInternal,
      weak_ptr_factory_.GetWeakPtr(), use_case, mojo::GetBadMessageCallback()));
}

void ModelBrokerImpl::RequestAssetsForInternal(
    const std::string& use_case,
    mojo::ReportBadMessageCallback bad_message_callback,
    const on_device_model::Capabilities& capabilities) {
  TRACE_EVENT("optimization_guide", "ModelBrokerImpl::RequestAssetsForInternal",
              perfetto::Flow::FromPointer(this));
  if (!solution_providers_.contains(use_case) &&
      !GetFeatureForUseCase(use_case)) {
    std::move(bad_message_callback).Run("Unsupported use case");
    return;
  }
  usage_tracker_->OnDeviceEligibleUseCaseUsed(use_case);
}

ModelBrokerImpl::SolutionProvider& ModelBrokerImpl::GetSolutionProvider(
    const std::string& use_case) {
  auto it = solution_providers_.find(use_case);
  if (it == solution_providers_.end()) {
    // TODO(crbug.com/500382778): Avoid unbounded map growth by not adding
    // entries for unsupported use-cases.
    it = solution_providers_.emplace(use_case, use_case).first;
  }
  return it->second;
}

ModelBrokerImpl::SolutionProvider& ModelBrokerImpl::GetSolutionProvider(
    mojom::OnDeviceFeature feature) {
  return GetSolutionProvider(ToUseCaseName(feature));
}

void ModelBrokerImpl::SetFeatureConfigs(
    base::flat_map<mojom::OnDeviceFeature, proto::Any> feature_configs) {
  feature_configs_ = std::move(feature_configs);
}

std::vector<mojom::BrokerUseCaseInfoPtr> ModelBrokerImpl::GetBrokerUseCaseInfo()
    const {
  std::vector<mojom::BrokerUseCaseInfoPtr> use_cases;
  for (const auto& [use_case, provider] : solution_providers_) {
    auto info = mojom::BrokerUseCaseInfo::New();
    info->name = use_case;
    info->assets_requested = usage_tracker_->WasUseCaseRecentlyUsed(use_case);
    info->unavailable_reason = AvailabilityFromEligibilityReason(
        provider.solution().error_or(OnDeviceModelEligibilityReason::kSuccess));
    use_cases.push_back(std::move(info));
  }
  return use_cases;
}

void ModelBrokerImpl::AddModelDownloadProgressObserver(
    const std::string& use_case,
    mojo::PendingRemote<on_device_model::mojom::DownloadObserver> observer) {
  TRACE_EVENT("optimization_guide",
              "ModelBrokerImpl::AddModelDownloadProgressObserver",
              perfetto::Flow::FromPointer(this));
  ensure_init_callback_.Run(
      base::BindOnce(&ModelBrokerImpl::AddModelDownloadProgressObserverInternal,
                     weak_ptr_factory_.GetWeakPtr(), use_case,
                     std::move(observer), mojo::GetBadMessageCallback()));
}

void ModelBrokerImpl::AddModelDownloadProgressObserverInternal(
    const std::string& use_case,
    mojo::PendingRemote<on_device_model::mojom::DownloadObserver> observer,
    mojo::ReportBadMessageCallback bad_message_callback,
    const on_device_model::Capabilities& capabilities) {
  TRACE_EVENT("optimization_guide",
              "ModelBrokerImpl::AddModelDownloadProgressObserverInternal",
              perfetto::Flow::FromPointer(this));
  if (!solution_providers_.contains(use_case) &&
      !GetFeatureForUseCase(use_case)) {
    std::move(bad_message_callback).Run("Unsupported use case");
    return;
  }
  add_download_progress_observer_callback_.Run(use_case, std::move(observer));
}

ModelBrokerImpl::Solution::Solution() = default;
ModelBrokerImpl::Solution::~Solution() = default;

ModelBrokerImpl::SolutionProvider::SolutionProvider(const std::string& use_case)
    : use_case_(use_case) {}

ModelBrokerImpl::SolutionProvider::~SolutionProvider() {
  TRACE_EVENT("optimization_guide",
              "ModelBrokerImpl::SolutionProvider::~SolutionProvider",
              perfetto::TerminatingFlow::FromPointer(this));
}

void ModelBrokerImpl::SolutionProvider::AddSubscriber(
    mojo::PendingRemote<mojom::ModelSubscriber> pending,
    const on_device_model::Capabilities& capabilities) {
  TRACE_EVENT("optimization_guide", "ModelBrokerImpl::AddSubscriber",
              perfetto::Flow::FromPointer(this));
  auto id = subscribers_.Add(std::move(pending));
  UpdatePossibleCapabilities(*subscribers_.Get(id), capabilities);
  UpdateSubscriber(*subscribers_.Get(id));
}

void ModelBrokerImpl::SolutionProvider::AddObserver(
    OnDeviceModelAvailabilityObserver* observer) {
  TRACE_EVENT("optimization_guide", "ModelBrokerImpl::AddObserver",
              perfetto::Flow::FromPointer(this));
  observers_.AddObserver(observer);
}

void ModelBrokerImpl::SolutionProvider::RemoveObserver(
    OnDeviceModelAvailabilityObserver* observer) {
  TRACE_EVENT("optimization_guide", "ModelBrokerImpl::RemoveObserver",
              perfetto::Flow::FromPointer(this));
  observers_.RemoveObserver(observer);
}

void ModelBrokerImpl::SolutionProvider::Update(MaybeSolution solution) {
  TRACE_EVENT("optimization_guide", "ModelBrokerImpl::SolutionProvider::Update",
              perfetto::Flow::FromPointer(this), "use_case", use_case_);
  CHECK(!solution.has_value() || solution.value());
  if (solution.has_value()) {
    if (solution_.has_value() && solution_.value()->IsValid()) {
      // Previous solution still valid, don't update.
      return;
    }
  } else if (!solution_.has_value() && solution.error() == solution_.error()) {
    // Same error, don't update.
    return;
  }
  receivers_.Clear();
  solution_ = std::move(solution);
  UpdateSubscribers();
  UpdateObservers();
}

void ModelBrokerImpl::SolutionProvider::UpdateSubscribers() {
  for (auto& subscriber : subscribers_) {
    UpdateSubscriber(*subscriber);
  }
  UpdateSubscriber(local_subscriber_);
}

void ModelBrokerImpl::SolutionProvider::UpdateSubscriber(
    mojom::ModelSubscriber& subscriber) {
  TRACE_EVENT("optimization_guide",
              "ModelBrokerImpl::SolutionProvider::UpdateSubscriber",
              perfetto::Flow::FromPointer(this), "use_case", use_case_);
  if (!solution_.has_value()) {
    subscriber.Unavailable(
        *AvailabilityFromEligibilityReason(solution_.error()),
        NotSupportedDetailedReasonFromEligibilityReason(solution_.error()));
    return;
  }
  if (!solution_.value() || !solution_.value()->IsValid()) {
    subscriber.Unavailable(mojom::ModelUnavailableReason::kPendingAssets,
                           std::nullopt);
    return;
  }
  auto config = solution_.value()->MakeConfig();
  mojo::PendingRemote<mojom::ModelSolution> pending;
  receivers_.Add(solution_.value().get(),
                 pending.InitWithNewPipeAndPassReceiver());
  subscriber.Available(std::move(config), std::move(pending));
}

void ModelBrokerImpl::SolutionProvider::UpdateObservers() {
  auto feature = GetFeatureForUseCase(use_case_);
  if (!feature) {
    return;
  }
  TRACE_EVENT("optimization_guide",
              "ModelBrokerImpl::SolutionProvider::UpdateObservers",
              perfetto::Flow::FromPointer(this), "feature",
              base::ToString(*feature));
  for (auto& observer : observers_) {
    observer.OnDeviceModelAvailabilityChanged(
        *feature, solution_.error_or(OnDeviceModelEligibilityReason::kSuccess));
  }
}

void ModelBrokerImpl::SolutionProvider::UpdatePossibleCapabilities(
    mojom::ModelSubscriber& subscriber,
    const on_device_model::Capabilities& capabilities) {
  TRACE_EVENT("optimization_guide",
              "ModelBrokerImpl::SolutionProvider::UpdatePossibleCapabilities",
              perfetto::Flow::FromPointer(this), "use_case", use_case_);
  subscriber.CapabilitiesUpdated(capabilities);
}

}  // namespace optimization_guide
