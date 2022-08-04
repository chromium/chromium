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

void TargetDeviceConnectionBrokerImpl::BluetoothAdapterFactoryWrapper::
    GetAdapter(device::BluetoothAdapterFactory::AdapterCallback callback) {
  if (bluetooth_adapter_factory_wrapper_for_testing_) {
    bluetooth_adapter_factory_wrapper_for_testing_->GetAdapterImpl(
        std::move(callback));
    return;
  }

  device::BluetoothAdapterFactory* adapter_factory =
      device::BluetoothAdapterFactory::Get();

  // Bluetooth is always supported on the ChromeOS platform.
  DCHECK(adapter_factory->IsBluetoothSupported());

  adapter_factory->GetAdapter(std::move(callback));
}

TargetDeviceConnectionBrokerImpl::BluetoothAdapterFactoryWrapper*
    TargetDeviceConnectionBrokerImpl::BluetoothAdapterFactoryWrapper::
        bluetooth_adapter_factory_wrapper_for_testing_ = nullptr;

TargetDeviceConnectionBrokerImpl::TargetDeviceConnectionBrokerImpl() {
  GetBluetoothAdapter();
}

TargetDeviceConnectionBrokerImpl::~TargetDeviceConnectionBrokerImpl() {}

TargetDeviceConnectionBrokerImpl::FeatureSupportStatus
TargetDeviceConnectionBrokerImpl::GetFeatureSupportStatus() const {
  if (!bluetooth_adapter_)
    return FeatureSupportStatus::kUndetermined;

  if (bluetooth_adapter_->IsPresent())
    return FeatureSupportStatus::kSupported;

  return FeatureSupportStatus::kNotSupported;
}

void TargetDeviceConnectionBrokerImpl::GetBluetoothAdapter() {
  // Because this will be called from the constructor, GetAdapter() may call
  // OnGetBluetoothAdapter() immediately which can cause problems during tests
  // since the class is not fully constructed yet.
  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(
          &BluetoothAdapterFactoryWrapper::GetAdapter,
          base::BindOnce(
              &TargetDeviceConnectionBrokerImpl::OnGetBluetoothAdapter,
              weak_ptr_factory_.GetWeakPtr())));
}

void TargetDeviceConnectionBrokerImpl::OnGetBluetoothAdapter(
    scoped_refptr<device::BluetoothAdapter> adapter) {
  bluetooth_adapter_ = adapter;

  if (deferred_start_advertising_callback_) {
    std::move(deferred_start_advertising_callback_).Run();
  }
}

void TargetDeviceConnectionBrokerImpl::StartAdvertising(
    ConnectionLifecycleListener* listener,
    ResultCallback on_start_advertising_callback) {
  // TODO(b/234655072): Notify client about incoming connections on the started
  // advertisement via ConnectionLifecycleListener.
  if (GetFeatureSupportStatus() == FeatureSupportStatus::kUndetermined) {
    deferred_start_advertising_callback_ =
        base::BindOnce(&TargetDeviceConnectionBroker::StartAdvertising,
                       weak_ptr_factory_.GetWeakPtr(), listener,
                       std::move(on_start_advertising_callback));
    return;
  }

  if (GetFeatureSupportStatus() == FeatureSupportStatus::kNotSupported) {
    LOG(ERROR)
        << __func__
        << " failed to start advertising because the feature is not supported.";
    std::move(on_start_advertising_callback).Run(/*success=*/false);
    return;
  }

  DCHECK(GetFeatureSupportStatus() == FeatureSupportStatus::kSupported);

  if (!bluetooth_adapter_->IsPowered()) {
    LOG(ERROR) << __func__
               << " failed to start advertising because the bluetooth adapter "
                  "is not powered.";
    std::move(on_start_advertising_callback).Run(/*success=*/false);
    return;
  }

  if (!random_session_id_) {
    random_session_id_ = base::UnguessableToken::Create();
  }

  fast_pair_advertiser_ =
      FastPairAdvertiser::Factory::Create(bluetooth_adapter_);
  auto [success_callback, failure_callback] =
      base::SplitOnceCallback(std::move(on_start_advertising_callback));

  fast_pair_advertiser_->StartAdvertising(
      base::BindOnce(std::move(success_callback), /*success=*/true),
      base::BindOnce(
          &TargetDeviceConnectionBrokerImpl::OnStartFastPairAdvertisingError,
          weak_ptr_factory_.GetWeakPtr(), std::move(failure_callback)),
      random_session_id_);
}

void TargetDeviceConnectionBrokerImpl::OnStartFastPairAdvertisingError(
    ResultCallback callback) {
  fast_pair_advertiser_.reset();
  std::move(callback).Run(/*success=*/false);
}

void TargetDeviceConnectionBrokerImpl::StopAdvertising(
    base::OnceClosure on_stop_advertising_callback) {
  if (deferred_start_advertising_callback_) {
    deferred_start_advertising_callback_.Reset();
  }

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
