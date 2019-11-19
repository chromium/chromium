// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/proximity_auth/proximity_monitor_impl.h"

#include <math.h>
#include <utility>

#include "base/bind.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "chromeos/components/multidevice/logging/logging.h"
#include "chromeos/components/proximity_auth/metrics.h"
#include "chromeos/services/secure_channel/public/cpp/client/client_channel.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"

namespace proximity_auth {

// The time to wait, in milliseconds, between proximity polling iterations.
const int kPollingTimeoutMs = 250;

// The weight of the most recent RSSI sample.
const double kRssiSampleWeight = 0.3;

// RSSI must be greater than this value to consider the host proximate, and
// allow unlock.
const int kRssiThreshold = -70;

ProximityMonitorImpl::ProximityMonitorImpl(
    chromeos::multidevice::RemoteDeviceRef remote_device,
    chromeos::secure_channel::ClientChannel* channel)
    : remote_device_(remote_device),
      channel_(channel),
      remote_device_is_in_proximity_(false),
      is_active_(false) {
  if (device::BluetoothAdapterFactory::IsBluetoothSupported()) {
    device::BluetoothAdapterFactory::GetAdapter(
        base::BindOnce(&ProximityMonitorImpl::OnAdapterInitialized,
                       weak_ptr_factory_.GetWeakPtr()));
  } else {
    PA_LOG(ERROR) << "[Proximity] Proximity monitoring unavailable: "
                  << "Bluetooth is unsupported on this platform.";
  }
}

ProximityMonitorImpl::~ProximityMonitorImpl() {}

void ProximityMonitorImpl::Start() {
  is_active_ = true;
  UpdatePollingState();
}

void ProximityMonitorImpl::Stop() {
  is_active_ = false;
  ClearProximityState();
  UpdatePollingState();
}

bool ProximityMonitorImpl::IsUnlockAllowed() const {
  return remote_device_is_in_proximity_;
}

void ProximityMonitorImpl::RecordProximityMetricsOnAuthSuccess() {
  double rssi_rolling_average = rssi_rolling_average_
                                    ? *rssi_rolling_average_
                                    : metrics::kUnknownProximityValue;

  std::string remote_device_model = metrics::kUnknownDeviceModel;
  chromeos::multidevice::RemoteDeviceRef remote_device = remote_device_;
  if (!remote_device.name().empty())
    remote_device_model = remote_device.name();

  metrics::RecordAuthProximityRollingRssi(round(rssi_rolling_average));
  metrics::RecordAuthProximityRemoteDeviceModelHash(remote_device_model);
}

void ProximityMonitorImpl::OnAdapterInitialized(
    scoped_refptr<device::BluetoothAdapter> adapter) {
  bluetooth_adapter_ = adapter;
  UpdatePollingState();
}

void ProximityMonitorImpl::UpdatePollingState() {
  if (ShouldPoll()) {
    // If there is a polling iteration already scheduled, wait for it.
    if (polling_weak_ptr_factory_.HasWeakPtrs())
      return;

    // Polling can re-entrantly call back into this method, so make sure to
    // schedule the next polling iteration prior to executing the current one.
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(
            &ProximityMonitorImpl::PerformScheduledUpdatePollingState,
            polling_weak_ptr_factory_.GetWeakPtr()),
        base::TimeDelta::FromMilliseconds(kPollingTimeoutMs));
    Poll();
  } else {
    polling_weak_ptr_factory_.InvalidateWeakPtrs();
    remote_device_is_in_proximity_ = false;
  }
}

void ProximityMonitorImpl::PerformScheduledUpdatePollingState() {
  polling_weak_ptr_factory_.InvalidateWeakPtrs();
  UpdatePollingState();
}

bool ProximityMonitorImpl::ShouldPoll() const {
  return is_active_ && bluetooth_adapter_;
}

void ProximityMonitorImpl::Poll() {
  DCHECK(ShouldPoll());

  if (channel_->is_disconnected()) {
    PA_LOG(ERROR) << "Channel is disconnected.";
    ClearProximityState();
    return;
  }

  channel_->GetConnectionMetadata(
      base::BindOnce(&ProximityMonitorImpl::OnGetConnectionMetadata,
                     weak_ptr_factory_.GetWeakPtr()));
}

void ProximityMonitorImpl::OnGetConnectionMetadata(
    chromeos::secure_channel::mojom::ConnectionMetadataPtr
        connection_metadata) {
  if (connection_metadata->bluetooth_connection_metadata)
    OnGetRssi(connection_metadata->bluetooth_connection_metadata->current_rssi);
  else
    OnGetRssi(base::nullopt);
}

void ProximityMonitorImpl::OnGetRssi(const base::Optional<int32_t>& rssi) {
  if (!is_active_) {
    PA_LOG(VERBOSE) << "Received RSSI after stopping.";
    return;
  }

  if (rssi) {
    AddSample(*rssi);
  } else {
    PA_LOG(WARNING) << "Received invalid RSSI value.";
    rssi_rolling_average_.reset();
    CheckForProximityStateChange();
  }
}

void ProximityMonitorImpl::ClearProximityState() {
  if (is_active_ && remote_device_is_in_proximity_)
    NotifyProximityStateChanged();

  remote_device_is_in_proximity_ = false;
  rssi_rolling_average_.reset();
}

void ProximityMonitorImpl::AddSample(int32_t rssi) {
  double weight = kRssiSampleWeight;
  if (!rssi_rolling_average_) {
    rssi_rolling_average_.reset(new double(rssi));
  } else {
    *rssi_rolling_average_ =
        weight * rssi + (1 - weight) * (*rssi_rolling_average_);
  }

  CheckForProximityStateChange();
}

void ProximityMonitorImpl::CheckForProximityStateChange() {
  bool is_now_in_proximity =
      rssi_rolling_average_ && *rssi_rolling_average_ > kRssiThreshold;

  if (rssi_rolling_average_ && !is_now_in_proximity) {
    PA_LOG(VERBOSE) << "Not in proximity. Rolling RSSI average: "
                    << *rssi_rolling_average_;
  }

  if (remote_device_is_in_proximity_ != is_now_in_proximity) {
    PA_LOG(VERBOSE) << "[Proximity] Updated proximity state: "
                    << (is_now_in_proximity ? "proximate" : "distant");
    remote_device_is_in_proximity_ = is_now_in_proximity;
    NotifyProximityStateChanged();
  }
}

}  // namespace proximity_auth
