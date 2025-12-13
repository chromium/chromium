// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/model_execution/model_broker_impl.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/strings/to_string.h"
#include "base/trace_event/trace_event.h"
#include "components/optimization_guide/core/model_execution/on_device_features.h"
#include "components/optimization_guide/core/model_execution/usage_tracker.h"
#include "components/optimization_guide/public/mojom/model_broker.mojom-data-view.h"

namespace optimization_guide {

ModelBrokerImpl::ModelBrokerImpl(UsageTracker& usage_tracker,
                                 EnsureInitCallback ensure_init_callback)
    : usage_tracker_(usage_tracker),
      ensure_init_callback_(std::move(ensure_init_callback)) {}

ModelBrokerImpl::~ModelBrokerImpl() = default;

void ModelBrokerImpl::BindBroker(
    mojo::PendingReceiver<mojom::ModelBroker> receiver) {
  TRACE_EVENT("optimization_guide", "ModelBrokerImpl::BindBroker");
  receivers_.Add(this, std::move(receiver));
}

void ModelBrokerImpl::Subscribe(
    mojom::ModelSubscriptionOptionsPtr options,
    mojo::PendingRemote<mojom::ModelSubscriber> subscriber) {
  TRACE_EVENT("optimization_guide", "ModelBrokerImpl::Subscribe");
  ensure_init_callback_.Run(base::BindOnce(
      &ModelBrokerImpl::SubscribeInternal, weak_ptr_factory_.GetWeakPtr(),
      std::move(options), std::move(subscriber)));
}

void ModelBrokerImpl::SubscribeInternal(
    mojom::ModelSubscriptionOptionsPtr options,
    mojo::PendingRemote<mojom::ModelSubscriber> subscriber) {
  TRACE_EVENT("optimization_guide", "ModelBrokerImpl::SubscribeInternal");
  if (options->mark_used) {
    usage_tracker_->OnDeviceEligibleFeatureUsed(options->feature);
  }
  GetSolutionProvider(options->feature).AddSubscriber(std::move(subscriber));
}

ModelBrokerImpl::SolutionProvider& ModelBrokerImpl::GetSolutionProvider(
    mojom::OnDeviceFeature feature) {
  return solution_providers_.emplace(feature, feature).first->second;
}

absl::flat_hash_set<mojom::OnDeviceFeature> ModelBrokerImpl::GetCapabilityKeys()
    const {
  absl::flat_hash_set<mojom::OnDeviceFeature> keys;
  for (const auto& [key, _] : solution_providers_) {
    keys.insert(key);
  }
  return keys;
}

ModelBrokerImpl::Solution::Solution() = default;
ModelBrokerImpl::Solution::~Solution() = default;

ModelBrokerImpl::SolutionProvider::SolutionProvider(
    mojom::OnDeviceFeature feature)
    : feature_(feature) {}

ModelBrokerImpl::SolutionProvider::~SolutionProvider() = default;

void ModelBrokerImpl::SolutionProvider::AddSubscriber(
    mojo::PendingRemote<mojom::ModelSubscriber> pending) {
  TRACE_EVENT("optimization_guide", "ModelBrokerImpl::AddSubscriber");
  auto id = subscribers_.Add(std::move(pending));
  UpdateSubscriber(*subscribers_.Get(id));
}

void ModelBrokerImpl::SolutionProvider::AddObserver(
    OnDeviceModelAvailabilityObserver* observer) {
  TRACE_EVENT("optimization_guide", "ModelBrokerImpl::AddObserver");
  observers_.AddObserver(observer);
}

void ModelBrokerImpl::SolutionProvider::RemoveObserver(
    OnDeviceModelAvailabilityObserver* observer) {
  TRACE_EVENT("optimization_guide", "ModelBrokerImpl::RemoveObserver");
  observers_.RemoveObserver(observer);
}

void ModelBrokerImpl::SolutionProvider::Update(MaybeSolution solution) {
  TRACE_EVENT("optimization_guide", "ModelBrokerImpl::SolutionProvider::Update",
              "feature", base::ToString(feature_));
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
              "ModelBrokerImpl::SolutionProvider::UpdateSubscriber", "feature",
              base::ToString(feature_));
  if (!solution_.has_value()) {
    subscriber.Unavailable(
        *AvailabilityFromEligibilityReason(solution_.error()));
    return;
  }
  if (!solution_.value() || !solution_.value()->IsValid()) {
    subscriber.Unavailable(mojom::ModelUnavailableReason::kPendingAssets);
    return;
  }
  auto config = solution_.value()->MakeConfig();
  mojo::PendingRemote<mojom::ModelSolution> pending;
  receivers_.Add(solution_.value().get(),
                 pending.InitWithNewPipeAndPassReceiver());
  subscriber.Available(std::move(config), std::move(pending));
}

void ModelBrokerImpl::SolutionProvider::UpdateObservers() {
  TRACE_EVENT("optimization_guide",
              "ModelBrokerImpl::SolutionProvider::UpdateObservers", "feature",
              base::ToString(feature_));
  for (auto& observer : observers_) {
    observer.OnDeviceModelAvailabilityChanged(
        feature_, solution_.error_or(OnDeviceModelEligibilityReason::kSuccess));
  }
}

}  // namespace optimization_guide
