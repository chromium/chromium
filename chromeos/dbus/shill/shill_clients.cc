// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/shill/shill_clients.h"

#include "chromeos/dbus/shill/modem_messaging_client.h"
#include "chromeos/dbus/shill/shill_device_client.h"
#include "chromeos/dbus/shill/shill_ipconfig_client.h"
#include "chromeos/dbus/shill/shill_manager_client.h"
#include "chromeos/dbus/shill/shill_profile_client.h"
#include "chromeos/dbus/shill/shill_service_client.h"
#include "chromeos/dbus/shill/shill_third_party_vpn_driver_client.h"
#include "chromeos/dbus/shill/sms_client.h"

namespace chromeos {
namespace shill_clients {

void Initialize(dbus::Bus* system_bus) {
#if !defined(USE_REAL_DBUS_CLIENTS)
  if (!system_bus)
    return InitializeFakes();
#endif
  DCHECK(system_bus);
  ModemMessagingClient::Initialize(system_bus);
  SMSClient::Initialize(system_bus);
  ShillDeviceClient::Initialize(system_bus);
  ShillIPConfigClient::Initialize(system_bus);
  ShillManagerClient::Initialize(system_bus);
  ShillProfileClient::Initialize(system_bus);
  ShillServiceClient::Initialize(system_bus);
  ShillThirdPartyVpnDriverClient::Initialize(system_bus);
}

void InitializeFakes() {
  ModemMessagingClient::InitializeFake();
  SMSClient::InitializeFake();
  ShillDeviceClient::InitializeFake();
  ShillIPConfigClient::InitializeFake();
  ShillManagerClient::InitializeFake();
  ShillProfileClient::InitializeFake();
  ShillServiceClient::InitializeFake();
  ShillThirdPartyVpnDriverClient::InitializeFake();

  ShillManagerClient::Get()->GetTestInterface()->SetupDefaultEnvironment();
}

void Shutdown() {
  ShillThirdPartyVpnDriverClient::Shutdown();
  ShillServiceClient::Shutdown();
  ShillProfileClient::Shutdown();
  ShillManagerClient::Shutdown();
  ShillIPConfigClient::Shutdown();
  ShillDeviceClient::Shutdown();
  SMSClient::Shutdown();
  ModemMessagingClient::Shutdown();
}

}  // namespace shill_clients
}  // namespace chromeos
