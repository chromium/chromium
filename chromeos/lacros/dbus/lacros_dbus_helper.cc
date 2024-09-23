// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/lacros/dbus/lacros_dbus_helper.h"

#include "base/feature_list.h"
#include "chromeos/dbus/dlp/dlp_client.h"
#include "chromeos/dbus/init/initialize_dbus_client.h"
#include "chromeos/dbus/ip_peripheral/ip_peripheral_service_client.h"
#include "chromeos/dbus/missive/missive_client.h"
#include "chromeos/dbus/permission_broker/permission_broker_client.h"
#include "chromeos/dbus/power/power_manager_client.h"
#include "chromeos/dbus/regmon/regmon_client.h"
#include "chromeos/dbus/tpm_manager/tpm_manager_client.h"
#include "chromeos/dbus/u2f/u2f_client.h"
#include "chromeos/lacros/dbus/lacros_dbus_thread_manager.h"
#include "device/bluetooth/dbus/bluez_dbus_manager.h"
#include "device/bluetooth/floss/floss_dbus_manager.h"
#include "device/bluetooth/floss/floss_features.h"

namespace chromeos {

void LacrosInitializeDBus() {
  // Unlike Ash, Lacros has no services that need paths, and therefore needs
  // not override paths like Ash does.

  // Initialize LacrosDBusThreadManager for the browser.
  LacrosDBusThreadManager::Initialize();

  // Initialize Chrome D-Bus clients.
  dbus::Bus* bus = LacrosDBusThreadManager::Get()->GetSystemBus();

  InitializeDBusClient<IpPeripheralServiceClient>(bus);
  InitializeDBusClient<PermissionBrokerClient>(bus);
  InitializeDBusClient<MissiveClient>(bus);
  InitializeDBusClient<chromeos::PowerManagerClient>(bus);
  InitializeDBusClient<TpmManagerClient>(bus);
  InitializeDBusClient<U2FClient>(bus);
  InitializeDBusClient<DlpClient>(bus);
  InitializeDBusClient<chromeos::RegmonClient>(bus);
}

void LacrosInitializeFeatureListDependentDBus() {
  dbus::Bus* bus = LacrosDBusThreadManager::Get()->GetSystemBus();
  if (floss::features::IsFlossEnabled()) {
    InitializeDBusClient<floss::FlossDBusManager>(bus);
  } else {
    InitializeDBusClient<bluez::BluezDBusManager>(bus);
  }
}

void LacrosShutdownDBus() {
  // Shut down D-Bus clients in reverse order of initialization.

  // FeatureList is no longer available at this stage. Relying on the actual
  // Bluetooth DBUS manager properties rather than feature flags.
  if (floss::FlossDBusManager::IsInitialized()) {
    floss::FlossDBusManager::Shutdown();
  }
  if (bluez::BluezDBusManager::IsInitialized()) {
    bluez::BluezDBusManager::Shutdown();
  }

  chromeos::RegmonClient::Shutdown();
  DlpClient::Shutdown();
  MissiveClient::Shutdown();
  PermissionBrokerClient::Shutdown();
  IpPeripheralServiceClient::Shutdown();
  LacrosDBusThreadManager::Shutdown();
}

}  // namespace chromeos
