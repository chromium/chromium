// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_BLUETOOTH_CONFIG_IN_PROCESS_INSTANCE_H_
#define CHROMEOS_ASH_SERVICES_BLUETOOTH_CONFIG_IN_PROCESS_INSTANCE_H_

#include "base/component_export.h"
#include "chromeos/ash/services/bluetooth_config/public/mojom/cros_bluetooth_config.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"

class PrefService;

namespace ash::bluetooth_config {

class FastPairDelegate;
class Initializer;

COMPONENT_EXPORT(IN_PROCESS_BLUETOOTH_CONFIG)
void Initialize(FastPairDelegate* fast_pair_delegate);

COMPONENT_EXPORT(IN_PROCESS_BLUETOOTH_CONFIG)
void Shutdown();

// Updates the PrefServices used by CrosBluetoothConfig.
COMPONENT_EXPORT(IN_PROCESS_BLUETOOTH_CONFIG)
void SetPrefs(PrefService* logged_in_profile_prefs, PrefService* device_prefs);

// Binds to an instance of CrosBluetoothConfig from within the browser process.
COMPONENT_EXPORT(IN_PROCESS_BLUETOOTH_CONFIG)
void BindToInProcessInstance(
    mojo::PendingReceiver<mojom::CrosBluetoothConfig> pending_receiver);

// Overrides the in-process instance for testing purposes. To reverse this
// override, call this function, passing null for |initializer|.
COMPONENT_EXPORT(IN_PROCESS_BLUETOOTH_CONFIG)
void OverrideInProcessInstanceForTesting(
    Initializer* initializer,
    FastPairDelegate* fast_pair_delegate = nullptr);

}  // namespace ash::bluetooth_config

#endif  // CHROMEOS_ASH_SERVICES_BLUETOOTH_CONFIG_IN_PROCESS_INSTANCE_H_
