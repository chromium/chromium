// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_OOBE_QUICK_START_CONNECTIVITY_TARGET_DEVICE_CONNECTION_BROKER_IMPL_H_
#define CHROMEOS_ASH_COMPONENTS_OOBE_QUICK_START_CONNECTIVITY_TARGET_DEVICE_CONNECTION_BROKER_IMPL_H_

#include "base/memory/weak_ptr.h"
#include "chromeos/ash/components/oobe_quick_start/connectivity/target_device_connection_broker.h"

class FastPairAdvertiser;

namespace device {
class BluetoothAdapter;
}

namespace ash::quick_start {

class TargetDeviceConnectionBrokerImpl : public TargetDeviceConnectionBroker {
 public:
  using FeatureSupportStatus =
      TargetDeviceConnectionBroker::FeatureSupportStatus;
  using ResultCallback = TargetDeviceConnectionBroker::ResultCallback;

  TargetDeviceConnectionBrokerImpl();
  TargetDeviceConnectionBrokerImpl(TargetDeviceConnectionBrokerImpl&) = delete;
  TargetDeviceConnectionBrokerImpl& operator=(
      TargetDeviceConnectionBrokerImpl&) = delete;
  ~TargetDeviceConnectionBrokerImpl() override;

  // TargetDeviceConnectionBroker:
  FeatureSupportStatus GetFeatureSupportStatus() const override;
  void StartAdvertising(ConnectionLifecycleListener* listener,
                        ResultCallback on_start_advertising_callback) override;
  void StopAdvertising(base::OnceClosure on_stop_advertising_callback) override;

 private:
  void GetBluetoothAdapter();
  void OnGetBluetoothAdapter(scoped_refptr<device::BluetoothAdapter> adapter);
  void OnStartFastPairAdvertisingError(ResultCallback callback);
  void OnStopFastPairAdvertising(base::OnceClosure callback);

  scoped_refptr<device::BluetoothAdapter> bluetooth_adapter_;

  std::unique_ptr<FastPairAdvertiser> fast_pair_advertiser_;

  base::WeakPtrFactory<TargetDeviceConnectionBrokerImpl> weak_ptr_factory_{
      this};
};

}  // namespace ash::quick_start

#endif  // CHROMEOS_ASH_COMPONENTS_OOBE_QUICK_START_CONNECTIVITY_TARGET_DEVICE_CONNECTION_BROKER_IMPL_H_
