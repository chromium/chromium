// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_CONNECTIVITY_PUBLIC_CPP_FAKE_PASSPOINT_SERVICE_H_
#define CHROMEOS_ASH_SERVICES_CONNECTIVITY_PUBLIC_CPP_FAKE_PASSPOINT_SERVICE_H_

#include "base/component_export.h"
#include "base/containers/flat_map.h"
#include "chromeos/ash/services/connectivity/public/mojom/passpoint.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/remote_set.h"

namespace ash::connectivity {

class FakePasspointSubscription;

// Implements a fake version of the PasspointService mojo interface for testing
// purpose.
class COMPONENT_EXPORT(FAKE_PASSPOINT_SERVICE) FakePasspointService
    : public chromeos::connectivity::mojom::PasspointService {
 public:
  // Creates the global instance with a fake implementation.
  static void Initialize();
  static bool IsInitialized();
  static void Shutdown();
  static FakePasspointService* Get();

  FakePasspointService(const FakePasspointService&) = delete;
  FakePasspointService& operator=(const FakePasspointService&) = delete;

  // chromeos::connectivity::mojom::PasspointService
  void GetPasspointSubscription(
      const std::string& id,
      GetPasspointSubscriptionCallback callback) override;
  void ListPasspointSubscriptions(
      ListPasspointSubscriptionsCallback callback) override;
  void DeletePasspointSubscription(
      const std::string& id,
      DeletePasspointSubscriptionCallback callback) override;
  void RegisterPasspointListener(
      mojo::PendingRemote<
          chromeos::connectivity::mojom::PasspointEventsListener> listener)
      override;

  // Binds a PendingReceiver to this instance.
  void BindPendingReceiver(
      mojo::PendingReceiver<chromeos::connectivity::mojom::PasspointService>
          pending_receiver);

  void AddFakePasspointSubscription(
      FakePasspointSubscription fake_subscription);

  // Clear all existing fake passpoint subscriptions, and it does not notify
  // its listeners.
  void ClearAll();

 private:
  FakePasspointService();
  ~FakePasspointService() override;

  void NotifyListenersSubscriptionAdded(const std::string& id);
  void NotifyListenersSubscriptionRemoved(const std::string& id);

  base::flat_map<std::string, FakePasspointSubscription>
      id_to_subscription_map_;

  mojo::RemoteSet<chromeos::connectivity::mojom::PasspointEventsListener>
      listeners_;
  mojo::ReceiverSet<chromeos::connectivity::mojom::PasspointService> receivers_;
};

}  // namespace ash::connectivity

#endif  // CHROMEOS_ASH_SERVICES_CONNECTIVITY_PUBLIC_CPP_FAKE_PASSPOINT_SERVICE_H_
