// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_SHILL_FAKE_SHILL_THIRD_PARTY_VPN_DRIVER_CLIENT_H_
#define CHROMEOS_DBUS_SHILL_FAKE_SHILL_THIRD_PARTY_VPN_DRIVER_CLIENT_H_

#include <stdint.h>

#include <map>
#include <string>
#include <vector>

#include "base/component_export.h"
#include "base/macros.h"
#include "chromeos/dbus/shill/shill_third_party_vpn_driver_client.h"

namespace chromeos {

// A fake implementation of ShillThirdPartyVpnDriverClient.
// The client can generate fake DBus signals when
// ShillThirdPartyVpnDriverClient::TestInterface methods are called. The
// DBus methods are nops that only acknowledge the caller.
class COMPONENT_EXPORT(SHILL_CLIENT) FakeShillThirdPartyVpnDriverClient
    : public ShillThirdPartyVpnDriverClient,
      public ShillThirdPartyVpnDriverClient::TestInterface {
 public:
  FakeShillThirdPartyVpnDriverClient();
  ~FakeShillThirdPartyVpnDriverClient() override;

  // ShillThirdPartyVpnDriverClient overrides
  void AddShillThirdPartyVpnObserver(
      const std::string& object_path_value,
      ShillThirdPartyVpnObserver* observer) override;
  void RemoveShillThirdPartyVpnObserver(
      const std::string& object_path_value) override;
  void SetParameters(
      const std::string& object_path_value,
      const base::DictionaryValue& parameters,
      const ShillClientHelper::StringCallback& callback,
      const ShillClientHelper::ErrorCallback& error_callback) override;
  void UpdateConnectionState(
      const std::string& object_path_value,
      const uint32_t connection_state,
      const base::Closure& callback,
      const ShillClientHelper::ErrorCallback& error_callback) override;
  void SendPacket(
      const std::string& object_path_value,
      const std::vector<char>& ip_packet,
      const base::Closure& callback,
      const ShillClientHelper::ErrorCallback& error_callback) override;
  ShillThirdPartyVpnDriverClient::TestInterface* GetTestInterface() override;

  // ShillThirdPartyVpnDriverClient::TestInterface overrides
  void OnPacketReceived(const std::string& object_path_value,
                        const std::vector<char>& packet) override;
  void OnPlatformMessage(const std::string& object_path_value,
                         uint32_t message) override;

 private:
  using ObserverMap = std::map<std::string, ShillThirdPartyVpnObserver*>;

  ObserverMap observer_map_;

  DISALLOW_COPY_AND_ASSIGN(FakeShillThirdPartyVpnDriverClient);
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_SHILL_FAKE_SHILL_THIRD_PARTY_VPN_DRIVER_CLIENT_H_
