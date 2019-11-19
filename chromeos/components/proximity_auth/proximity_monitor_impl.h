// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_PROXIMITY_AUTH_PROXIMITY_MONITOR_IMPL_H_
#define CHROMEOS_COMPONENTS_PROXIMITY_AUTH_PROXIMITY_MONITOR_IMPL_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "chromeos/components/multidevice/remote_device_ref.h"
#include "chromeos/components/proximity_auth/proximity_monitor.h"
#include "chromeos/services/secure_channel/public/mojom/secure_channel.mojom.h"
#include "device/bluetooth/bluetooth_device.h"

namespace chromeos {
namespace secure_channel {
class ClientChannel;
}  // namespace secure_channel
}  // namespace chromeos

namespace device {
class BluetoothAdapter;
}  // namespace device

namespace proximity_auth {

// The concrete implemenation of the proximity monitor interface.
class ProximityMonitorImpl : public ProximityMonitor {
 public:
  // The |connection| is not owned, and must outlive |this| instance.
  ProximityMonitorImpl(chromeos::multidevice::RemoteDeviceRef remote_device,
                       chromeos::secure_channel::ClientChannel* channel);
  ~ProximityMonitorImpl() override;

  // ProximityMonitor:
  void Start() override;
  void Stop() override;
  bool IsUnlockAllowed() const override;
  void RecordProximityMetricsOnAuthSuccess() override;

 private:
  // Callback for asynchronous initialization of the Bluetooth adpater.
  void OnAdapterInitialized(scoped_refptr<device::BluetoothAdapter> adapter);

  // Ensures that the app is periodically polling for the proximity status
  // between the remote and the local device iff it should be, based on the
  // current app state.
  void UpdatePollingState();

  // Performs a scheduled |UpdatePollingState()| operation. This method is
  // used to distinguish periodically scheduled calls to |UpdatePollingState()|
  // from event-driven calls, which should be handled differently.
  void PerformScheduledUpdatePollingState();

  // Returns |true| iff the app should be periodically polling for the proximity
  // status between the remote and the local device.
  bool ShouldPoll() const;

  // Polls the connection information.
  void Poll();

  void OnGetConnectionMetadata(
      chromeos::secure_channel::mojom::ConnectionMetadataPtr
          connection_metadata);
  void OnGetRssi(const base::Optional<int32_t>& rssi);

  // Resets the proximity state to |false|, and clears all member variables
  // tracking the proximity state.
  void ClearProximityState();

  // Updates the proximity state with a new sample of the current RSSI.
  void AddSample(int32_t rssi);

  // Checks whether the proximity state has changed based on the current
  // samples. Notifies observers on a change.
  void CheckForProximityStateChange();

  // Used to get the name of the remote device that ProximitMonitor is
  // communicating with, for metrics purposes.
  chromeos::multidevice::RemoteDeviceRef remote_device_;

  // Used to communicate with the remote device to gauge its proximity via RSSI
  // measurement.
  chromeos::secure_channel::ClientChannel* channel_;

  // The Bluetooth adapter that will be polled for connection info.
  scoped_refptr<device::BluetoothAdapter> bluetooth_adapter_;

  // Whether the remote device is currently in close proximity to the local
  // device.
  bool remote_device_is_in_proximity_;

  // Whether the proximity monitor is active, i.e. should possibly be scanning
  // for proximity to the remote device.
  bool is_active_;

  // The exponentailly weighted rolling average of the RSSI, used to smooth the
  // RSSI readings. Null if the monitor is inactive, has not recently observed
  // an RSSI reading, or the most recent connection info included an invalid
  // measurement.
  std::unique_ptr<double> rssi_rolling_average_;

  // Used to vend weak pointers for polling. Using a separate factory for these
  // weak pointers allows the weak pointers to be invalidated when polling
  // stops, which effectively cancels the scheduled tasks.
  base::WeakPtrFactory<ProximityMonitorImpl> polling_weak_ptr_factory_{this};

  // Used to vend all other weak pointers.
  base::WeakPtrFactory<ProximityMonitorImpl> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(ProximityMonitorImpl);
};

}  // namespace proximity_auth

#endif  // CHROMEOS_COMPONENTS_PROXIMITY_AUTH_PROXIMITY_MONITOR_IMPL_H_
