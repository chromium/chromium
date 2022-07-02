// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/oobe_quick_start/connectivity/target_device_connection_broker_impl.h"

#include <memory>

#include "base/bind.h"
#include "base/callback.h"
#include "base/logging.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "chromeos/ash/components/oobe_quick_start/connectivity/fast_pair_advertiser.h"
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
  // TODO(b/234655072): Notify client about incoming connections on the started
  // advertisement via ConnectionLifecycleListener.
  CHECK(GetFeatureSupportStatus() == FeatureSupportStatus::kSupported);

  if (!bluetooth_adapter_->IsPowered()) {
    LOG(ERROR) << __func__
               << " failed to start advertising because the bluetooth adapter "
                  "is not powered.";
    std::move(on_start_advertising_callback).Run(/*success=*/false);
    return;
  }

  fast_pair_advertiser_ =
      FastPairAdvertiser::Factory::Create(bluetooth_adapter_);
  auto [success_callback, failure_callback] =
      base::SplitOnceCallback(std::move(on_start_advertising_callback));

  fast_pair_advertiser_->StartAdvertising(
      base::BindOnce(std::move(success_callback), /*success=*/true),
      base::BindOnce(
          &TargetDeviceConnectionBrokerImpl::OnStartFastPairAdvertisingError,
          weak_ptr_factory_.GetWeakPtr(), std::move(failure_callback)));
}

void TargetDeviceConnectionBrokerImpl::OnStartFastPairAdvertisingError(
    ResultCallback callback) {
  fast_pair_advertiser_.reset();
  std::move(callback).Run(/*success=*/false);
}

void TargetDeviceConnectionBrokerImpl::StopAdvertising(
    base::OnceClosure on_stop_advertising_callback) {
  if (!fast_pair_advertiser_) {
    VLOG(1) << __func__ << " Not currently advertising, ignoring.";
    std::move(on_stop_advertising_callback).Run();
    return;
  }

  fast_pair_advertiser_->StopAdvertising(base::BindOnce(
      &TargetDeviceConnectionBrokerImpl::OnStopFastPairAdvertising,
      weak_ptr_factory_.GetWeakPtr(), std::move(on_stop_advertising_callback)));
}

void TargetDeviceConnectionBrokerImpl::OnStopFastPairAdvertising(
    base::OnceClosure callback) {
  fast_pair_advertiser_.reset();
  std::move(callback).Run();
}

}  // namespace ash::quick_start
