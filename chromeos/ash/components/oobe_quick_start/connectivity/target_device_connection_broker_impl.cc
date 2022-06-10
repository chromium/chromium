// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/oobe_quick_start/connectivity/target_device_connection_broker_impl.h"

#include <memory>

#include "base/callback.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"

namespace ash::quick_start {

TargetDeviceConnectionBrokerImpl::TargetDeviceConnectionBrokerImpl() {
  GetBluetoothAdapter();
}

TargetDeviceConnectionBrokerImpl::~TargetDeviceConnectionBrokerImpl() {}

TargetDeviceConnectionBrokerImpl::FeatureSupportStatus
TargetDeviceConnectionBrokerImpl::GetFeatureSupportStatus() const {
  // TODO(b/234848503) Add unit test coverage for the kUndetermined case.
  if (!bluetooth_adapter_)
    return FeatureSupportStatus::kUndetermined;

  if (bluetooth_adapter_->IsPresent())
    return FeatureSupportStatus::kSupported;

  return FeatureSupportStatus::kNotSupported;
}

void TargetDeviceConnectionBrokerImpl::GetBluetoothAdapter() {
  auto* adapter_factory = device::BluetoothAdapterFactory::Get();

  // Bluetooth is always supported on the ChromeOS platform.
  DCHECK(adapter_factory->IsBluetoothSupported());

  // Because this will be called from the constructor, GetAdapter() may call
  // OnGetBluetoothAdapter() immediately which can cause problems during tests
  // since the class is not fully constructed yet.
  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(
          &device::BluetoothAdapterFactory::GetAdapter,
          base::Unretained(adapter_factory),
          base::BindOnce(
              &TargetDeviceConnectionBrokerImpl::OnGetBluetoothAdapter,
              weak_ptr_factory_.GetWeakPtr())));
}

void TargetDeviceConnectionBrokerImpl::OnGetBluetoothAdapter(
    scoped_refptr<device::BluetoothAdapter> adapter) {
  bluetooth_adapter_ = adapter;
}

void TargetDeviceConnectionBrokerImpl::StartAdvertising(
    ConnectionLifecycleListener* listener,
    ResultCallback on_start_advertising_callback) {
  // TODO(b/234655072): Implement StartAdvertising
  std::move(on_start_advertising_callback).Run(true);
}

void TargetDeviceConnectionBrokerImpl::StopAdvertising(
    ResultCallback on_stop_advertising_callback) {
  // TODO(b/234655072): Implement StopAdvertising
  std::move(on_stop_advertising_callback).Run(true);
}

}  // namespace ash::quick_start
