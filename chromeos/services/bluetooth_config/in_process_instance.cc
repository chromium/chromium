// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/bluetooth_config/in_process_instance.h"

#include "ash/constants/ash_features.h"
#include "base/bind.h"
#include "base/check.h"
#include "base/no_destructor.h"
#include "chromeos/services/bluetooth_config/cros_bluetooth_config.h"
#include "chromeos/services/bluetooth_config/initializer_impl.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"

namespace chromeos {
namespace bluetooth_config {
namespace {

CrosBluetoothConfig* g_instance = nullptr;

void OnBluetoothAdapter(
    mojo::PendingReceiver<mojom::CrosBluetoothConfig> pending_receiver,
    scoped_refptr<device::BluetoothAdapter> bluetooth_adapter) {
  if (!g_instance) {
    InitializerImpl initializer;
    g_instance =
        new CrosBluetoothConfig(initializer, std::move(bluetooth_adapter));
  }
  g_instance->BindPendingReceiver(std::move(pending_receiver));
}

}  // namespace

void BindToInProcessInstance(
    mojo::PendingReceiver<mojom::CrosBluetoothConfig> pending_receiver) {
  CHECK(ash::features::IsBluetoothRevampEnabled());
  device::BluetoothAdapterFactory::Get()->GetAdapter(
      base::BindOnce(&OnBluetoothAdapter, std::move(pending_receiver)));
}

void OverrideInProcessInstanceForTesting(Initializer* initializer) {
  if (g_instance) {
    delete g_instance;
    g_instance = nullptr;
  }

  if (!initializer)
    return;

  // Null BluetoothAdapter is used since |initializer| is expected to fake
  // Bluetooth functionality in tests.
  g_instance = new CrosBluetoothConfig(*initializer,
                                       /*bluetooth_adapter=*/nullptr);
}

}  // namespace bluetooth_config
}  // namespace chromeos
