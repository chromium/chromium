// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/download/internal/background_service/scheduler/device_status_listener.h"
#include "base/functional/bind.h"
#include "build/build_config.h"

namespace download {

namespace {

// Converts |battery_power_status| to battery status.
BatteryStatus ToBatteryStatus(
    base::PowerStateObserver::BatteryPowerStatus battery_power_status) {
  switch (battery_power_status) {
    case base::PowerStateObserver::BatteryPowerStatus::kBatteryPower:
      return BatteryStatus::NOT_CHARGING;
    case base::PowerStateObserver::BatteryPowerStatus::kExternalPower:
      // TODO(339859756): We return CHARGING for kUnknown to preserve the old
      // behavior.
    case base::PowerStateObserver::BatteryPowerStatus::kUnknown:
      return BatteryStatus::CHARGING;
  }
}

// Converts a ConnectionType to NetworkStatus.
NetworkStatus ToNetworkStatus(network::mojom::ConnectionType type) {
  switch (type) {
    case network::mojom::ConnectionType::CONNECTION_ETHERNET:
    case network::mojom::ConnectionType::CONNECTION_WIFI:
      return NetworkStatus::UNMETERED;
    case network::mojom::ConnectionType::CONNECTION_2G:
    case network::mojom::ConnectionType::CONNECTION_3G:
    case network::mojom::ConnectionType::CONNECTION_4G:
    case network::mojom::ConnectionType::CONNECTION_5G:
      // TODO(crbug.com/40148439): 5G networks may be unmetered. Find a way to
      // detect this and make DeviceStatusListener aware of it.
      return NetworkStatus::METERED;
    case network::mojom::ConnectionType::CONNECTION_NONE:
    case network::mojom::ConnectionType::CONNECTION_BLUETOOTH:
      return NetworkStatus::DISCONNECTED;
    case network::mojom::ConnectionType::CONNECTION_UNKNOWN:
#if BUILDFLAG(IS_ANDROID)
      return NetworkStatus::DISCONNECTED;
#else
      return NetworkStatus::UNMETERED;
#endif
  }
  NOTREACHED_IN_MIGRATION();
  return NetworkStatus::DISCONNECTED;
}

}  // namespace

DeviceStatusListener::DeviceStatusListener(
    const base::TimeDelta& startup_delay,
    const base::TimeDelta& online_delay,
    std::unique_ptr<BatteryStatusListener> battery_listener,
    std::unique_ptr<NetworkStatusListener> network_listener)
    : network_listener_(std::move(network_listener)),
      observer_(nullptr),
      listening_(false),
      is_valid_state_(false),
      online_delay_(online_delay),
      battery_listener_(std::move(battery_listener)) {}

DeviceStatusListener::~DeviceStatusListener() {
  Stop();
}

const DeviceStatus& DeviceStatusListener::CurrentDeviceStatus() {
  DCHECK(battery_listener_);
  status_.battery_percentage = battery_listener_->GetBatteryPercentage();
  return status_;
}

void DeviceStatusListener::SetObserver(
    DeviceStatusListener::Observer* observer) {
  DCHECK(observer);
  observer_ = observer;
}

void DeviceStatusListener::Start(const base::TimeDelta& start_delay) {
  if (listening_ || !observer_)
    return;

  // Network stack may shake off all connections after getting the IP address,
  // use a delay to wait for potential network setup.
  timer_.Start(FROM_HERE, start_delay,
               base::BindOnce(&DeviceStatusListener::StartAfterDelay,
                              base::Unretained(this)));
}

void DeviceStatusListener::StartAfterDelay() {
  if (listening_ || !observer_)
    return;

  // Listen to battery status changes.
  DCHECK(battery_listener_);
  battery_listener_->Start(this);
  status_.battery_status =
      ToBatteryStatus(battery_listener_->GetBatteryPowerStatus());

  // Listen to network status changes.
  network_listener_->Start(this);

  status_.battery_status =
      ToBatteryStatus(battery_listener_->GetBatteryPowerStatus());
  status_.network_status =
      ToNetworkStatus(network_listener_->GetConnectionType());
  pending_network_status_ = status_.network_status;

  listening_ = true;
}

void DeviceStatusListener::Stop() {
  timer_.Stop();

  if (!listening_)
    return;

  battery_listener_->Stop();
  battery_listener_.reset();

  network_listener_->Stop();
  network_listener_.reset();

  status_ = DeviceStatus();
  listening_ = false;
  observer_ = nullptr;
}

void DeviceStatusListener::OnNetworkStatusReady(
    network::mojom::ConnectionType type) {
  status_.network_status = ToNetworkStatus(type);
  is_valid_state_ = true;
  NotifyStatusChange();
}

void DeviceStatusListener::OnNetworkChanged(
    network::mojom::ConnectionType type) {
  pending_network_status_ = ToNetworkStatus(type);

  if (pending_network_status_ == status_.network_status) {
    timer_.Stop();
    is_valid_state_ = true;
    return;
  }

  bool change_to_online =
      (status_.network_status == NetworkStatus::DISCONNECTED) &&
      (pending_network_status_ != NetworkStatus::DISCONNECTED);

  // It's unreliable to send requests immediately after the network becomes
  // online that the signal may not fully consider DHCP. Notify network change
  // to the observer after a delay.
  // Android network change listener still need this delay.
  if (change_to_online) {
    is_valid_state_ = false;
    timer_.Start(FROM_HERE, online_delay_,
                 base::BindOnce(&DeviceStatusListener::NotifyNetworkChange,
                                base::Unretained(this)));
  } else {
    timer_.Stop();
    NotifyNetworkChange();
  }
}

void DeviceStatusListener::OnPowerStateChange(
    base::PowerStateObserver::BatteryPowerStatus battery_power_status) {
  status_.battery_status = ToBatteryStatus(battery_power_status);
  NotifyStatusChange();
}

void DeviceStatusListener::NotifyStatusChange() {
  DCHECK(observer_);
  observer_->OnDeviceStatusChanged(status_);
}

void DeviceStatusListener::NotifyNetworkChange() {
  is_valid_state_ = true;
  if (pending_network_status_ == status_.network_status)
    return;

  status_.network_status = pending_network_status_;
  NotifyStatusChange();
}

}  // namespace download
