// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOWNLOAD_INTERNAL_BACKGROUND_SERVICE_SCHEDULER_DEVICE_STATUS_LISTENER_H_
#define COMPONENTS_DOWNLOAD_INTERNAL_BACKGROUND_SERVICE_SCHEDULER_DEVICE_STATUS_LISTENER_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "components/download/internal/background_service/scheduler/battery_status_listener.h"
#include "components/download/internal/background_service/scheduler/device_status.h"
#include "components/download/network/network_status_listener.h"

namespace download {

// Listens to network and battery status change and notifies the observer.
class DeviceStatusListener : public NetworkStatusListener::Observer,
                             public BatteryStatusListener::Observer {
 public:
  class Observer {
   public:
    // Called when device status is changed.
    virtual void OnDeviceStatusChanged(const DeviceStatus& device_status) = 0;
  };

  explicit DeviceStatusListener(
      const base::TimeDelta& startup_delay,
      const base::TimeDelta& online_delay,
      std::unique_ptr<BatteryStatusListener> battery_listener,
      std::unique_ptr<NetworkStatusListener> network_listener);

  DeviceStatusListener(const DeviceStatusListener&) = delete;
  DeviceStatusListener& operator=(const DeviceStatusListener&) = delete;

  ~DeviceStatusListener() override;

  bool is_valid_state() { return is_valid_state_; }

  // Returns the current device status for download scheduling. May update
  // internal device status when called.
  const DeviceStatus& CurrentDeviceStatus();

  void SetObserver(DeviceStatusListener::Observer* observer);

  // Starts/stops to listen network and battery change events, virtual for
  // testing.
  virtual void Start(const base::TimeDelta& start_delay);

  virtual void Stop();

 protected:
  // NetworkStatusListener::Observer implementation. Visible for testing.
  void OnNetworkStatusReady(network::mojom::ConnectionType type) override;
  void OnNetworkChanged(network::mojom::ConnectionType type) override;

  // Used to listen to network connectivity changes.
  std::unique_ptr<NetworkStatusListener> network_listener_;

  // The current device status.
  DeviceStatus status_;

  // The observer that listens to device status change events.
  raw_ptr<Observer> observer_;

  // If device status listener is started.
  bool listening_;

  bool is_valid_state_;

 private:
  // Start after a delay to wait for potential network stack setup.
  void StartAfterDelay();

  // BatteryStatusListener::Observer implementation.
  void OnPowerStateChange(base::PowerStateObserver::BatteryPowerStatus
                              battery_power_status) override;

  // Notifies the observer about device status change.
  void NotifyStatusChange();

  // Called after a delay to notify the observer. See |delay_|.
  void NotifyNetworkChange();

  // Used to start the device listener or notify network change after a delay.
  base::OneShotTimer timer_;

  // The delay used when network status becomes online.
  base::TimeDelta online_delay_;

  // Pending network status used to update the current network status.
  NetworkStatus pending_network_status_ = NetworkStatus::DISCONNECTED;

  // Used to listen to battery status.
  std::unique_ptr<BatteryStatusListener> battery_listener_;
};

}  // namespace download

#endif  // COMPONENTS_DOWNLOAD_INTERNAL_BACKGROUND_SERVICE_SCHEDULER_DEVICE_STATUS_LISTENER_H_
