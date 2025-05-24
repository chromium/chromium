// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_DBUS_SHILL_FAKE_SHILL_THIRD_PARTY_VPN_DRIVER_CLIENT_H_
#define CHROMEOS_ASH_COMPONENTS_DBUS_SHILL_FAKE_SHILL_THIRD_PARTY_VPN_DRIVER_CLIENT_H_

#include <stdint.h>

#include <map>
#include <string>
#include <vector>

#include "base/component_export.h"
#include "base/memory/raw_ptr.h"
#include "chromeos/ash/components/dbus/shill/shill_third_party_vpn_driver_client.h"

namespace ash {

// A fake implementation of ShillThirdPartyVpnDriverClient.
// The client can generate fake DBus signals when
// ShillThirdPartyVpnDriverClient::TestInterface methods are called. The
// DBus methods are nops that only acknowledge the caller.
class COMPONENT_EXPORT(SHILL_CLIENT) FakeShillThirdPartyVpnDriverClient
    : public ShillThirdPartyVpnDriverClient,
      public ShillThirdPartyVpnDriverClient::TestInterface {
 public:
  FakeShillThirdPartyVpnDriverClient();

  FakeShillThirdPartyVpnDriverClient(
      const FakeShillThirdPartyVpnDriverClient&) = delete;
  FakeShillThirdPartyVpnDriverClient& operator=(
      const FakeShillThirdPartyVpnDriverClient&) = delete;

  ~FakeShillThirdPartyVpnDriverClient() override;

  // ShillThirdPartyVpnDriverClient overrides
  void AddShillThirdPartyVpnObserver(
      const std::string& object_path_value,
      ShillThirdPartyVpnObserver* observer) override;
  void RemoveShillThirdPartyVpnObserver(
      const std::string& object_path_value) override;
  void SetParameters(const std::string& object_path_value,
                     const base::Value::Dict& parameters,
                     StringCallback callback,
                     ErrorCallback error_callback) override;
  void UpdateConnectionState(const std::string& object_path_value,
                             const uint32_t connection_state,
                             base::OnceClosure callback,
                             ErrorCallback error_callback) override;
  void SendPacket(const std::string& object_path_value,
                  const std::vector<char>& ip_packet,
                  base::OnceClosure callback,
                  ErrorCallback error_callback) override;
  ShillThirdPartyVpnDriverClient::TestInterface* GetTestInterface() override;

  // ShillThirdPartyVpnDriverClient::TestInterface overrides
  void OnPacketReceived(const std::string& object_path_value,
                        const std::vector<char>& packet) override;
  void OnPlatformMessage(const std::string& object_path_value,
                         uint32_t message) override;

 private:
  using ObserverMap =
      std::map<std::string,
               raw_ptr<ShillThirdPartyVpnObserver, CtnExperimental>>;

  ObserverMap observer_map_;
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_DBUS_SHILL_FAKE_SHILL_THIRD_PARTY_VPN_DRIVER_CLIENT_H_
