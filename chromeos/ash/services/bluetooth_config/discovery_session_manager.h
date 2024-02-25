// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_BLUETOOTH_CONFIG_DISCOVERY_SESSION_MANAGER_H_
#define CHROMEOS_ASH_SERVICES_BLUETOOTH_CONFIG_DISCOVERY_SESSION_MANAGER_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "chromeos/ash/services/bluetooth_config/adapter_state_controller.h"
#include "chromeos/ash/services/bluetooth_config/device_pairing_handler.h"
#include "chromeos/ash/services/bluetooth_config/discovered_devices_provider.h"
#include "chromeos/ash/services/bluetooth_config/discovery_session_status_notifier.h"
#include "chromeos/ash/services/bluetooth_config/public/mojom/cros_bluetooth_config.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote_set.h"

namespace ash::bluetooth_config {

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
                                public DiscoveredDevicesProvider::Observer,
                                public DiscoverySessionStatusNotifier {
 public:
  ~DiscoverySessionManager() override;

  // Starts a discovery attempt. |delegate| is notified when the discovery
  // session has started and stopped. To cancel a discovery attempt, disconnect
  // |delegate|.
  void StartDiscovery(
      mojo::PendingRemote<mojom::BluetoothDiscoveryDelegate> delegate);

 protected:
  DiscoverySessionManager(
      AdapterStateController* adapter_state_controller,
      DiscoveredDevicesProvider* discovered_devices_provider);

  void NotifyDiscoveryStarted();
  void NotifyDiscoveryStoppedAndClearActiveClients();
  bool HasAtLeastOneDiscoveryClient() const;
  void NotifyDiscoveredDevicesListChanged();

  virtual bool IsDiscoverySessionActive() const = 0;

  // Derived classes can override this function to be notified when the first
  // client is added or the last client is removed. This callback can be used to
  // start or stop a discovery session.
  virtual void OnHasAtLeastOneDiscoveryClientChanged() {}

  // Derived classes must override this to provide a concrete implementation of
  // DevicePairingHandler.
  virtual std::unique_ptr<DevicePairingHandler> CreateDevicePairingHandler(
      AdapterStateController* adapter_state_controller,
      mojo::PendingReceiver<mojom::DevicePairingHandler> receiver) = 0;

 private:
  friend class DiscoverySessionManagerImplTest;

  // AdapterStateController::Observer:
  void OnAdapterStateChanged() override;

  // DiscoveredDevicesProvider::Observer:
  void OnDiscoveredDevicesListChanged() override;

  // Creates a new DevicePairingHandler for |id| and inserts it into
  // |id_to_pairing_handler_map_|. Returns the remote connected to the handler.
  mojo::PendingRemote<mojom::DevicePairingHandler>
  RegisterNewDevicePairingHandler(mojo::RemoteSetElementId id);

  bool IsBluetoothEnabled() const;
  void OnDelegateDisconnected(mojo::RemoteSetElementId id);

  // Flushes queued Mojo messages in unit tests.
  void FlushForTesting();

  base::flat_map<mojo::RemoteSetElementId,
                 std::unique_ptr<DevicePairingHandler>>
      id_to_pairing_handler_map_;

  raw_ptr<AdapterStateController> adapter_state_controller_;
  raw_ptr<DiscoveredDevicesProvider> discovered_devices_provider_;

  base::ScopedObservation<AdapterStateController,
                          AdapterStateController::Observer>
      adapter_state_controller_observation_{this};
  base::ScopedObservation<DiscoveredDevicesProvider,
                          DiscoveredDevicesProvider::Observer>
      discovered_devices_provider_observation_{this};

  mojo::RemoteSet<mojom::BluetoothDiscoveryDelegate> delegates_;

  base::WeakPtrFactory<DiscoverySessionManager> weak_ptr_factory_{this};
};

}  // namespace ash::bluetooth_config

#endif  // CHROMEOS_ASH_SERVICES_BLUETOOTH_CONFIG_DISCOVERY_SESSION_MANAGER_H_
