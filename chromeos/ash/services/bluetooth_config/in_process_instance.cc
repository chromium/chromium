// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/bluetooth_config/in_process_instance.h"

#include <optional>

#include "ash/constants/ash_features.h"
#include "base/check.h"
#include "base/functional/bind.h"
#include "chromeos/ash/services/bluetooth_config/cros_bluetooth_config.h"
#include "chromeos/ash/services/bluetooth_config/fast_pair_delegate.h"
#include "chromeos/ash/services/bluetooth_config/initializer_impl.h"
#include "components/device_event_log/device_event_log.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"

namespace ash::bluetooth_config {

namespace {

CrosBluetoothConfig* g_instance = nullptr;

// Flag indicating that Shutdown() has been called and g_instance should not be
// initialized.
bool g_is_shut_down = false;

// PrefServices that should be used by CrosBluetoothConfig once it has finished
// initializing.
PrefService* g_pending_logged_in_profile_prefs = nullptr;
PrefService* g_pending_device_prefs = nullptr;

FastPairDelegate* g_fast_pair_delegate = nullptr;

void OnBluetoothAdapter(
    std::optional<mojo::PendingReceiver<mojom::CrosBluetoothConfig>>
        pending_receiver,
    scoped_refptr<device::BluetoothAdapter> bluetooth_adapter) {
  if (g_is_shut_down)
    return;

  if (!g_instance) {
    BLUETOOTH_LOG(EVENT) << "Initializing CrosBluetoothConfig";
    InitializerImpl initializer;
    g_instance = new CrosBluetoothConfig(
        initializer, std::move(bluetooth_adapter), g_fast_pair_delegate);
    g_instance->SetPrefs(g_pending_logged_in_profile_prefs,
                         g_pending_device_prefs);
    g_pending_logged_in_profile_prefs = nullptr;
    g_pending_device_prefs = nullptr;
  }
  if (pending_receiver)
    g_instance->BindPendingReceiver(std::move(*pending_receiver));
}

}  // namespace

void Initialize(FastPairDelegate* delegate) {
  BLUETOOTH_LOG(EVENT) << "Beginning CrosBluetoothConfig initialization";
  CHECK(!g_instance);
  DCHECK_EQ(features::IsFastPairEnabled(), static_cast<bool>(delegate));

  g_fast_pair_delegate = delegate;

  device::BluetoothAdapterFactory::Get()->GetAdapter(
      base::BindOnce(&OnBluetoothAdapter, /*pending_receiver=*/std::nullopt));
}

void Shutdown() {
  BLUETOOTH_LOG(EVENT) << "Shutting down CrosBluetoothConfig";
  if (g_instance) {
    delete g_instance;
    g_instance = nullptr;
  } else {
    g_is_shut_down = true;
  }
  g_pending_logged_in_profile_prefs = nullptr;
  g_pending_device_prefs = nullptr;
}

void SetPrefs(PrefService* logged_in_profile_prefs, PrefService* device_prefs) {
  if (!g_instance) {
    // |g_instance| may not be initialized yet if we're still getting the
    // Bluetooth adapter. Save the prefs to be used once the instance has
    // initialized.
    g_pending_logged_in_profile_prefs = logged_in_profile_prefs;
    g_pending_device_prefs = device_prefs;
    return;
  }
  g_instance->SetPrefs(logged_in_profile_prefs, device_prefs);
}

void BindToInProcessInstance(
    mojo::PendingReceiver<mojom::CrosBluetoothConfig> pending_receiver) {
  BLUETOOTH_LOG(DEBUG) << "Binding to CrosBluetoothConfig";
  device::BluetoothAdapterFactory::Get()->GetAdapter(
      base::BindOnce(&OnBluetoothAdapter, std::move(pending_receiver)));
}

void OverrideInProcessInstanceForTesting(Initializer* initializer,
                                         FastPairDelegate* fast_pair_delegate) {
  if (g_instance) {
    delete g_instance;
    g_instance = nullptr;
  }

  if (!initializer)
    return;

  // Null BluetoothAdapter is used since |initializer| is expected to fake
  // Bluetooth functionality in tests.
  g_instance = new CrosBluetoothConfig(*initializer,
                                       /*bluetooth_adapter=*/nullptr,
                                       fast_pair_delegate);
}

}  // namespace ash::bluetooth_config
