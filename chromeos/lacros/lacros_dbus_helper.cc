// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/lacros/lacros_dbus_helper.h"

#include "chromeos/dbus/init/initialize_dbus_client.h"
#include "chromeos/dbus/permission_broker/permission_broker_client.h"
#include "chromeos/lacros/lacros_dbus_thread_manager.h"

namespace chromeos {

void LacrosInitializeDBus() {
  // Unlike Ash, Lacros has no services that need paths, and therefore needs
  // not override paths like Ash does.

  // Initialize LacrosDBusThreadManager for the browser.
  LacrosDBusThreadManager::Initialize();

  // Initialize Chrome D-Bus clients.
  dbus::Bus* bus = LacrosDBusThreadManager::Get()->GetSystemBus();

  InitializeDBusClient<PermissionBrokerClient>(bus);
}

void LacrosShutdownDBus() {
  // Shut down D-Bus clients in reverse order of initialization.
  PermissionBrokerClient::Shutdown();

  LacrosDBusThreadManager::Shutdown();
}

}  // namespace chromeos
