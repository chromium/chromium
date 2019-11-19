// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_SHILL_SHILL_THIRD_PARTY_VPN_DRIVER_CLIENT_H_
#define CHROMEOS_DBUS_SHILL_SHILL_THIRD_PARTY_VPN_DRIVER_CLIENT_H_

#include <stdint.h>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/component_export.h"
#include "base/macros.h"
#include "chromeos/dbus/shill/shill_client_helper.h"

namespace base {
class DictionaryValue;
}  // namespace base

namespace dbus {
class Bus;
}  // namespace dbus

namespace chromeos {

class ShillThirdPartyVpnObserver;

// ShillThirdPartyVpnDriverClient is used to communicate with the Shill
// ThirdPartyVpnDriver service.
// All methods should be called from the origin thread which initializes the
// DBusThreadManager instance.
class COMPONENT_EXPORT(SHILL_CLIENT) ShillThirdPartyVpnDriverClient {
 public:
  class TestInterface {
   public:
    virtual void OnPacketReceived(const std::string& object_path_value,
                                  const std::vector<char>& packet) = 0;
    virtual void OnPlatformMessage(const std::string& object_path_value,
                                   uint32_t message) = 0;

   protected:
    virtual ~TestInterface() {}
  };

  // Creates and initializes the global instance. |bus| must not be null.
  static void Initialize(dbus::Bus* bus);

  // Creates the global instance with a fake implementation if not already
  // created (e.g. in a browser test setup), otherwise does nothing.
  static void InitializeFake();

  // Destroys the global instance which must have been initialized.
  static void Shutdown();

  // Returns the global instance if initialized. May return null.
  static ShillThirdPartyVpnDriverClient* Get();

  // Adds an |observer| for the third party vpn driver at |object_path_value|.
  virtual void AddShillThirdPartyVpnObserver(
      const std::string& object_path_value,
      ShillThirdPartyVpnObserver* observer) = 0;

  // Removes an |observer| for the third party vpn driver at
  // |object_path_value|.
  virtual void RemoveShillThirdPartyVpnObserver(
      const std::string& object_path_value) = 0;

  // Calls SetParameters method.
  // |callback| is called after the method call succeeds.
  virtual void SetParameters(
      const std::string& object_path_value,
      const base::DictionaryValue& parameters,
      const ShillClientHelper::StringCallback& callback,
      const ShillClientHelper::ErrorCallback& error_callback) = 0;

  // Calls UpdateConnectionState method.
  // |callback| is called after the method call succeeds.
  virtual void UpdateConnectionState(
      const std::string& object_path_value,
      const uint32_t connection_state,
      const base::Closure& callback,
      const ShillClientHelper::ErrorCallback& error_callback) = 0;

  // Calls SendPacket method.
  // |callback| is called after the method call succeeds.
  virtual void SendPacket(
      const std::string& object_path_value,
      const std::vector<char>& ip_packet,
      const base::Closure& callback,
      const ShillClientHelper::ErrorCallback& error_callback) = 0;

  // Returns an interface for testing (stub only), or returns nullptr.
  virtual ShillThirdPartyVpnDriverClient::TestInterface* GetTestInterface() = 0;

 protected:
  friend class ShillThirdPartyVpnDriverClientTest;

  // Initialize/Shutdown should be used instead.
  ShillThirdPartyVpnDriverClient();
  virtual ~ShillThirdPartyVpnDriverClient();

 private:
  DISALLOW_COPY_AND_ASSIGN(ShillThirdPartyVpnDriverClient);
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_SHILL_SHILL_THIRD_PARTY_VPN_DRIVER_CLIENT_H_
