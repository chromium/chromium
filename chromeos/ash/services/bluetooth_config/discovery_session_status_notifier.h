// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_BLUETOOTH_CONFIG_DISCOVERY_SESSION_STATUS_NOTIFIER_H_
#define CHROMEOS_ASH_SERVICES_BLUETOOTH_CONFIG_DISCOVERY_SESSION_STATUS_NOTIFIER_H_

#include "chromeos/ash/services/bluetooth_config/public/mojom/cros_bluetooth_config.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/remote_set.h"

namespace ash::bluetooth_config {

// Notifiers all listeners of changes in discovery session status.
class DiscoverySessionStatusNotifier {
 public:
  virtual ~DiscoverySessionStatusNotifier();

  // Adds an observer of discovery session status. |observer| will be notified
  // each time discovery session status changes. To stop observing, clients
  // should disconnect the Mojo pipe to |observer| by deleting the associated
  // Receiver.
  void ObserveDiscoverySessionStatusChanges(
      mojo::PendingRemote<mojom::DiscoverySessionStatusObserver> observer);

 protected:
  DiscoverySessionStatusNotifier();

  // Notifies all observers when "having at least one discovery session" changes
  // between true and false.
  void NotifyHasAtLeastOneDiscoverySessionChanged(
      bool has_at_least_one_discovery_session);

 private:
  mojo::RemoteSet<mojom::DiscoverySessionStatusObserver> observers_;
};

}  // namespace ash::bluetooth_config

#endif  // CHROMEOS_ASH_SERVICES_BLUETOOTH_CONFIG_DISCOVERY_SESSION_STATUS_NOTIFIER_H_
