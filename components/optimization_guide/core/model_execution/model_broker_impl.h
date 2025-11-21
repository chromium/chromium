// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_MODEL_BROKER_IMPL_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_MODEL_BROKER_IMPL_H_

#include <map>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/types/expected.h"
#include "components/optimization_guide/core/model_execution/model_broker_client.h"
#include "components/optimization_guide/core/model_execution/on_device_capability.h"
#include "components/optimization_guide/public/mojom/model_broker.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote_set.h"
#include "services/on_device_model/public/mojom/on_device_model.mojom.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_set.h"

namespace optimization_guide {

class UsageTracker;

// A ModelBroker implementation that serves solutions fed to it.
class ModelBrokerImpl final : public mojom::ModelBroker {
 public:
  // A function that calls a closure once some init has been completed.
  using EnsureInitCallback = base::RepeatingCallback<void(base::OnceClosure)>;

  // A set of (references to) compatible, versioned dependencies that implement
  // an OnDeviceFeature.
  // e.g. "You can summarize with this model by building the prompt this way."
  class Solution : public mojom::ModelSolution {
   public:
    Solution();
    ~Solution() override;

    // Whether all of the dependencies are still available.
    virtual bool IsValid() const = 0;

    // Creates a config describing this solution;
    virtual mojom::ModelSolutionConfigPtr MakeConfig() const = 0;
  };

  using MaybeSolution =
      base::expected<std::unique_ptr<Solution>, OnDeviceModelEligibilityReason>;

  // Keeps subscribers updated with the current solution.
  class SolutionProvider final {
   public:
    explicit SolutionProvider(mojom::OnDeviceFeature feature);
    ~SolutionProvider();

    void AddSubscriber(mojo::PendingRemote<mojom::ModelSubscriber> pending);
    void AddObserver(OnDeviceModelAvailabilityObserver* observer);
    void RemoveObserver(OnDeviceModelAvailabilityObserver* observer);

    void Update(MaybeSolution solution);

    MaybeSolution& solution() { return solution_; }
    // The local subscriber is a special subscriber that gets updated
    // synchronously.
    ModelSubscriberImpl& local_subscriber() { return local_subscriber_; }

   private:
    void UpdateSubscribers();
    void UpdateSubscriber(mojom::ModelSubscriber& client);
    void UpdateObservers();

    mojom::OnDeviceFeature feature_;
    mojo::RemoteSet<mojom::ModelSubscriber> subscribers_;
    base::ObserverList<OnDeviceModelAvailabilityObserver> observers_;
    ModelSubscriberImpl local_subscriber_;
    MaybeSolution solution_ =
        base::unexpected(OnDeviceModelEligibilityReason::kUnknown);
    mojo::ReceiverSet<mojom::ModelSolution> receivers_;
  };

  ModelBrokerImpl(UsageTracker& usage_tracker,
                  EnsureInitCallback ensure_init_callback);
  ~ModelBrokerImpl() override;

  void BindBroker(mojo::PendingReceiver<mojom::ModelBroker> receiver);

  // Get (or construct) the solution provider for the feature.
  SolutionProvider& GetSolutionProvider(mojom::OnDeviceFeature feature);

  // Get the set of capability keys that have solutions or subscribers.
  absl::flat_hash_set<mojom::OnDeviceFeature> GetCapabilityKeys() const;

 private:
  // mojom::ModelBroker:
  void Subscribe(
      mojom::ModelSubscriptionOptionsPtr options,
      mojo::PendingRemote<mojom::ModelSubscriber> subscriber) override;

  // Finishes Subscribe after initialization is finished.
  void SubscribeInternal(
      mojom::ModelSubscriptionOptionsPtr options,
      mojo::PendingRemote<mojom::ModelSubscriber> subscriber);

  raw_ref<UsageTracker> usage_tracker_;
  EnsureInitCallback ensure_init_callback_;
  std::map<mojom::OnDeviceFeature, SolutionProvider> solution_providers_;
  mojo::ReceiverSet<mojom::ModelBroker> receivers_;
  base::WeakPtrFactory<ModelBrokerImpl> weak_ptr_factory_{this};
};

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_MODEL_BROKER_IMPL_H_
