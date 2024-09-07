// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/connectivity/public/cpp/fake_passpoint_service.h"

#include "chromeos/ash/services/connectivity/public/cpp/fake_passpoint_subscription.h"

namespace ash::connectivity {

using chromeos::connectivity::mojom::PasspointEventsListener;
using chromeos::connectivity::mojom::PasspointSubscription;
using chromeos::connectivity::mojom::PasspointSubscriptionPtr;

namespace {

FakePasspointService* g_instance = nullptr;

PasspointSubscriptionPtr FakePasspointSubscriptionToMojom(
    const FakePasspointSubscription& fake_passpoint_subscription) {
  auto mojom_passpoint_subscription = PasspointSubscription::New();
  mojom_passpoint_subscription->id = fake_passpoint_subscription.id();
  mojom_passpoint_subscription->friendly_name =
      fake_passpoint_subscription.friendly_name();
  mojom_passpoint_subscription->provisioning_source =
      fake_passpoint_subscription.provisioning_source();
  mojom_passpoint_subscription->expiration_epoch_ms =
      fake_passpoint_subscription.expiration_epoch_ms();
  if (fake_passpoint_subscription.trusted_ca()) {
    mojom_passpoint_subscription->trusted_ca =
        *fake_passpoint_subscription.trusted_ca();
  }
  mojom_passpoint_subscription->domains = fake_passpoint_subscription.domains();
  return mojom_passpoint_subscription;
}

}  // namespace

// static
void FakePasspointService::Initialize() {
  CHECK(!g_instance);
  g_instance = new FakePasspointService();
}

// static
bool FakePasspointService::IsInitialized() {
  return g_instance;
}

// static
void FakePasspointService::Shutdown() {
  CHECK(g_instance);
  delete g_instance;
  g_instance = nullptr;
}

// static
FakePasspointService* FakePasspointService::Get() {
  CHECK(g_instance) << "FakePasspointService::Get() called before Initialize()";
  return g_instance;
}

FakePasspointService::FakePasspointService() = default;

FakePasspointService::~FakePasspointService() = default;

void FakePasspointService::GetPasspointSubscription(
    const std::string& id,
    GetPasspointSubscriptionCallback callback) {
  const auto it = id_to_subscription_map_.find(id);
  if (it == id_to_subscription_map_.end()) {
    LOG(WARNING) << "No subscription found with id: " << id;
    std::move(callback).Run(nullptr);
    return;
  }
  std::move(callback).Run(FakePasspointSubscriptionToMojom(it->second));
}

void FakePasspointService::ListPasspointSubscriptions(
    ListPasspointSubscriptionsCallback callback) {
  std::vector<PasspointSubscriptionPtr> passpoint_subscriptions;
  for (const auto& id_fake_subscription_pair : id_to_subscription_map_) {
    passpoint_subscriptions.push_back(
        FakePasspointSubscriptionToMojom(id_fake_subscription_pair.second));
  }
  std::move(callback).Run(std::move(passpoint_subscriptions));
}

void FakePasspointService::DeletePasspointSubscription(
    const std::string& id,
    DeletePasspointSubscriptionCallback callback) {
  const auto it = id_to_subscription_map_.find(id);
  if (it == id_to_subscription_map_.end()) {
    std::move(callback).Run(/*success=*/false);
    return;
  }
  id_to_subscription_map_.erase(it);
  std::move(callback).Run(/*success=*/true);
  NotifyListenersSubscriptionRemoved(id);
}

void FakePasspointService::RegisterPasspointListener(
    mojo::PendingRemote<PasspointEventsListener> listener) {
  listeners_.Add(std::move(listener));
}

void FakePasspointService::AddFakePasspointSubscription(
    FakePasspointSubscription fake_passpoint_subscription) {
  const std::string id = fake_passpoint_subscription.id();
  if (id_to_subscription_map_.find(id) != id_to_subscription_map_.end()) {
    LOG(ERROR) << "Adding duplicate fake passpoint subscription with id: "
               << id;
    return;
  }
  id_to_subscription_map_.insert_or_assign(id, fake_passpoint_subscription);
  NotifyListenersSubscriptionAdded(id);
}

void FakePasspointService::ClearAll() {
  id_to_subscription_map_.clear();
}

void FakePasspointService::BindPendingReceiver(
    mojo::PendingReceiver<chromeos::connectivity::mojom::PasspointService>
        pending_receiver) {
  receivers_.Add(this, std::move(pending_receiver));
}

void FakePasspointService::NotifyListenersSubscriptionAdded(
    const std::string& id) {
  auto passpoint_subscription_added = PasspointSubscription::New();
  passpoint_subscription_added->id = id;
  for (auto& listener : listeners_) {
    listener->OnPasspointSubscriptionAdded(
        passpoint_subscription_added.Clone());
  }
}

void FakePasspointService::NotifyListenersSubscriptionRemoved(
    const std::string& id) {
  auto passpoint_subscription_removed = PasspointSubscription::New();
  passpoint_subscription_removed->id = id;
  for (auto& listener : listeners_) {
    listener->OnPasspointSubscriptionRemoved(
        passpoint_subscription_removed.Clone());
  }
}

}  // namespace ash::connectivity
