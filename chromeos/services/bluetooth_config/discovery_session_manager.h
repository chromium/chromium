// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_BLUETOOTH_CONFIG_DISCOVERY_SESSION_MANAGER_H_
#define CHROMEOS_SERVICES_BLUETOOTH_CONFIG_DISCOVERY_SESSION_MANAGER_H_

#include "base/scoped_observation.h"
#include "chromeos/services/bluetooth_config/adapter_state_controller.h"
#include "chromeos/services/bluetooth_config/device_cache.h"
#include "chromeos/services/bluetooth_config/public/mojom/cros_bluetooth_config.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote_set.h"

namespace chromeos {
namespace bluetooth_config {

// Handles requests to start discovery sessions, which are used to initiate
// pairing of new devices. Clients invoke StartDiscovery() to begin the flow and
// disconnect the delegate passed to StartDiscovery() to end the flow.
//
// When a client starts a discovery session, it is immediately informed of
// the current discovered Bluetooth devices and will continue to receive updates
// whenever a device is added, updated or removed. Once the session has ended,
// clients will no longer receive updates.
//
// Internally, this class ensures that Bluetooth discovery remains active as
// long as at least one discovery client is active. Note that this class is only
// responsible for starting and stopping discovery and does not handle pairing
// attempts.
class DiscoverySessionManager : public AdapterStateController::Observer,
                                public DeviceCache::Observer {
 public:
  ~DiscoverySessionManager() override;

  // Starts a discovery attempt. |delegate| is notified when the discovery
  // session has started and stopped. To cancel a discovery attempt, disconnect
  // |delegate|.
  void StartDiscovery(
      mojo::PendingRemote<mojom::BluetoothDiscoveryDelegate> delegate);

 protected:
  DiscoverySessionManager(AdapterStateController* adapter_state_controller,
                          DeviceCache* device_cache);

  void NotifyDiscoveryStarted();
  void NotifyDiscoveryStoppedAndClearActiveClients();
  bool HasAtLeastOneDiscoveryClient() const;
  void NotifyDiscoveredDevicesListChanged();

  virtual bool IsDiscoverySessionActive() const = 0;

  // Derived classes can override this function to be notified when the first
  // client is added or the last client is removed. This callback can be used to
  // start or stop a discovery session.
  virtual void OnHasAtLeastOneDiscoveryClientChanged() {}

 private:
  friend class DiscoverySessionManagerImplTest;

  // AdapterStateController::Observer:
  void OnAdapterStateChanged() override;

  // DeviceCache::Observer:
  void OnUnpairedDevicesListChanged() override;

  bool IsBluetoothEnabled() const;
  void OnDelegateDisconnected(mojo::RemoteSetElementId id);

  // Flushes queued Mojo messages in unit tests.
  void FlushForTesting();

  AdapterStateController* adapter_state_controller_;
  DeviceCache* device_cache_;

  base::ScopedObservation<AdapterStateController,
                          AdapterStateController::Observer>
      adapter_state_controller_observation_{this};
  base::ScopedObservation<DeviceCache, DeviceCache::Observer>
      device_cache_observation_{this};

  mojo::RemoteSet<mojom::BluetoothDiscoveryDelegate> delegates_;
};

}  // namespace bluetooth_config
}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_BLUETOOTH_CONFIG_DISCOVERY_SESSION_MANAGER_H_
