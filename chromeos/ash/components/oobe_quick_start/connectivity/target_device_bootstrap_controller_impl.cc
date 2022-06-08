// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/oobe_quick_start/connectivity/target_device_bootstrap_controller_impl.h"

#include "base/callback.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"

namespace ash::quick_start {

// static
std::unique_ptr<TargetDeviceBootstrapController>
TargetDeviceBootstrapControllerImpl::Factory::Create() {
  if (test_factory_) {
    return test_factory_->CreateInstance();
  }

  return std::make_unique<TargetDeviceBootstrapControllerImpl>();
}

// static
void TargetDeviceBootstrapControllerImpl::Factory::SetFactoryForTesting(
    Factory* test_factory) {
  test_factory_ = test_factory;
}

TargetDeviceBootstrapControllerImpl::Factory*
    TargetDeviceBootstrapControllerImpl::Factory::test_factory_ = nullptr;

TargetDeviceBootstrapControllerImpl::TargetDeviceBootstrapControllerImpl() {
  GetBluetoothAdapter();
}

TargetDeviceBootstrapControllerImpl::~TargetDeviceBootstrapControllerImpl() {}

TargetDeviceBootstrapControllerImpl::FeatureSupportStatus
TargetDeviceBootstrapControllerImpl::GetFeatureSupportStatus() const {
  // TODO(b/234848503) Add unit test coverage for the kUndetermined case.
  if (!bluetooth_adapter_)
    return FeatureSupportStatus::kUndetermined;

  if (bluetooth_adapter_->IsPresent())
    return FeatureSupportStatus::kSupported;

  return FeatureSupportStatus::kNotSupported;
}

void TargetDeviceBootstrapControllerImpl::GetBluetoothAdapter() {
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
              &TargetDeviceBootstrapControllerImpl::OnGetBluetoothAdapter,
              weak_ptr_factory_.GetWeakPtr())));
}

void TargetDeviceBootstrapControllerImpl::OnGetBluetoothAdapter(
    scoped_refptr<device::BluetoothAdapter> adapter) {
  bluetooth_adapter_ = adapter;
}

}  // namespace ash::quick_start
