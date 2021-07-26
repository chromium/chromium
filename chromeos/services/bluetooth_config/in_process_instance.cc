// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/bluetooth_config/in_process_instance.h"

#include "ash/constants/ash_features.h"
#include "base/bind.h"
#include "base/check.h"
#include "base/no_destructor.h"
#include "chromeos/services/bluetooth_config/cros_bluetooth_config.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"

namespace chromeos {
namespace bluetooth_config {
namespace {

void OnBluetoothAdapter(
    mojo::PendingReceiver<mojom::CrosBluetoothConfig> pending_receiver,
    scoped_refptr<device::BluetoothAdapter> bluetooth_adapter) {
  static base::NoDestructor<CrosBluetoothConfig> cros_bluetooth_config(
      std::move(bluetooth_adapter));
  cros_bluetooth_config->BindPendingReceiver(std::move(pending_receiver));
}

}  // namespace

void BindToInProcessInstance(
    mojo::PendingReceiver<mojom::CrosBluetoothConfig> pending_receiver) {
  CHECK(ash::features::IsBluetoothRevampEnabled());
  device::BluetoothAdapterFactory::Get()->GetAdapter(
      base::BindOnce(&OnBluetoothAdapter, std::move(pending_receiver)));
}

}  // namespace bluetooth_config
}  // namespace chromeos
