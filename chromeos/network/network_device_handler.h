// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_NETWORK_NETWORK_DEVICE_HANDLER_H_
#define CHROMEOS_NETWORK_NETWORK_DEVICE_HANDLER_H_

#include <string>
#include <vector>

#include "base/callback.h"
#include "base/component_export.h"
#include "base/macros.h"
#include "chromeos/network/network_handler_callbacks.h"

namespace base {
class Value;
}

namespace net {
class IPEndPoint;
}

namespace chromeos {

class NetworkStateHandler;

// The NetworkDeviceHandler class allows making device specific requests on a
// ChromeOS network device. All calls are asynchronous and interact with the
// Shill device API. No calls will block on DBus calls.
//
// This is owned and its lifetime managed by the Chrome startup code. It's
// basically a singleton, but with explicit lifetime management.
//
// Note on callbacks: Because all the functions here are meant to be
// asynchronous, they take a |callback| of some type, and an |error_callback|.
// When the operation succeeds, |callback| will be called, and when it doesn't,
// |error_callback| will be called with information about the error, including a
// symbolic name for the error and often some error message that is suitable for
// logging. None of the error message text is meant for user consumption.
class COMPONENT_EXPORT(CHROMEOS_NETWORK) NetworkDeviceHandler {
 public:
  // Constants for |error_name| from |error_callback|.
  static const char kErrorDeviceMissing[];
  static const char kErrorFailure[];
  static const char kErrorIncorrectPin[];
  static const char kErrorNotFound[];
  static const char kErrorNotSupported[];
  static const char kErrorPinBlocked[];
  static const char kErrorPinRequired[];
  static const char kErrorTimeout[];
  static const char kErrorUnknown[];

  NetworkDeviceHandler();
  virtual ~NetworkDeviceHandler();

  // Gets the properties of the device with id |device_path|. See note on
  // |callback| and |error_callback|, in class description above.
  virtual void GetDeviceProperties(
      const std::string& device_path,
      const network_handler::DictionaryResultCallback& callback,
      const network_handler::ErrorCallback& error_callback) const = 0;

  // Sets the value of property |name| on device with id |device_path| to
  // |value|. This function provides a generic setter to be used by the UI or
  // network API and doesn't allow changes to protected settings like cellular
  // roaming.
  virtual void SetDeviceProperty(
      const std::string& device_path,
      const std::string& property_name,
      const base::Value& value,
      const base::Closure& callback,
      const network_handler::ErrorCallback& error_callback) = 0;

  // Tells the device specified by |device_path| to register to the cellular
  // network with id |network_id|. If |network_id| is empty then registration
  // will proceed in automatic mode, which will cause the modem to register
  // with the home network.
  // This call is only available on cellular devices and will fail with
  // Error.NotSupported on all other technologies.
  virtual void RegisterCellularNetwork(
      const std::string& device_path,
      const std::string& network_id,
      const base::Closure& callback,
      const network_handler::ErrorCallback& error_callback) = 0;

  // SIM PIN/PUK methods

  // Tells the device whether or not a SIM PIN lock should be enforced by
  // the device referenced by |device_path|. If |require_pin| is true, a PIN
  // code (specified in |pin|) will be required before the next time the device
  // can be enabled. If |require_pin| is false, the existing requirement will
  // be lifted.
  //
  // See note on |callback| and |error_callback| in the class description
  // above. The operation will fail if:
  //    - Device |device_path| could not be found.
  //    - The PIN requirement status already matches |require_pin|.
  //    - |pin| doesn't match the PIN code currently stored by the SIM.
  //    - No SIM exists on the device.
  //
  // This method applies to Cellular devices only. The call will fail with a
  // "not-supported" error if called on a non-cellular device.
  virtual void RequirePin(
      const std::string& device_path,
      bool require_pin,
      const std::string& pin,
      const base::Closure& callback,
      const network_handler::ErrorCallback& error_callback) = 0;

  // Sends the PIN code |pin| to the device |device_path|.
  //
  // See note on |callback| and |error_callback| in the class description
  // above. The operation will fail if:
  //    - Device |device_path| could not be found.
  //    - |pin| is incorrect.
  //    - The SIM is blocked.
  //
  // This method applies to Cellular devices only. The call will fail with a
  // "not-supported" error if called on a non-cellular device.
  virtual void EnterPin(
      const std::string& device_path,
      const std::string& pin,
      const base::Closure& callback,
      const network_handler::ErrorCallback& error_callback) = 0;

  // Sends the PUK code |puk| to the SIM to unblock a blocked SIM. On success,
  // the SIM will be unblocked and its PIN code will be set to |pin|.
  //
  // See note on |callback| and |error_callback| in the class description
  // above. The operation will fail if:
  //    - Device |device_path| could not be found.
  //    - |puk| is incorrect.
  //
  // This method applies to Cellular devices only. The call will fail with a
  // "not-supported" error if called on a non-cellular device.
  virtual void UnblockPin(
      const std::string& device_path,
      const std::string& puk,
      const std::string& new_pin,
      const base::Closure& callback,
      const network_handler::ErrorCallback& error_callback) = 0;

  // Tells the device to change the PIN code used to unlock a locked SIM card.
  //
  // See note on |callback| and |error_callback| in the class description
  // above. The operation will fail if:
  //    - Device |device_path| could not be found.
  //    - |old_pin| does not match the current PIN on the device.
  //    - The SIM is locked.
  //    - The SIM is blocked.
  //
  // This method applies to Cellular devices only. The call will fail with a
  // "not-supported" error if called on a non-cellular device.
  virtual void ChangePin(
      const std::string& device_path,
      const std::string& old_pin,
      const std::string& new_pin,
      const base::Closure& callback,
      const network_handler::ErrorCallback& error_callback) = 0;

  // Enables/disables roaming of all cellular devices. This happens
  // asychronously in the background and applies also to devices which become
  // available in the future.
  virtual void SetCellularAllowRoaming(bool allow_roaming) = 0;

  // Sets up MAC address randomization if available. This applies to devices
  // which become available in the future.
  virtual void SetMACAddressRandomizationEnabled(bool enabled) = 0;

  // Sets up USB Ethernet MAC address source. This applies to primary enabled
  // USB Ethernet device.
  virtual void SetUsbEthernetMacAddressSource(const std::string& source) = 0;

  // Attempts to enable or disable TDLS for the specified IP or MAC address for
  // the active wifi device.
  virtual void SetWifiTDLSEnabled(
      const std::string& ip_or_mac_address,
      bool enabled,
      const network_handler::StringResultCallback& callback,
      const network_handler::ErrorCallback& error_callback) = 0;

  // Returns the TDLS status for the specified IP or MAC address for
  // the active wifi device.
  virtual void GetWifiTDLSStatus(
      const std::string& ip_or_mac_address,
      const network_handler::StringResultCallback& callback,
      const network_handler::ErrorCallback& error_callback) = 0;

  // Adds |ip_endpoint| to the list of tcp connections that the wifi device
  // should monitor to wake the system from suspend.
  virtual void AddWifiWakeOnPacketConnection(
      const net::IPEndPoint& ip_endpoint,
      const base::Closure& callback,
      const network_handler::ErrorCallback& error_callback) = 0;

  // Adds |types| to the list of packet types that the device should monitor to
  // wake the system from suspend.
  virtual void AddWifiWakeOnPacketOfTypes(
      const std::vector<std::string>& types,
      const base::Closure& callback,
      const network_handler::ErrorCallback& error_callback) = 0;

  // Removes |ip_endpoint| from the list of tcp connections that the wifi device
  // should monitor to wake the system from suspend.
  virtual void RemoveWifiWakeOnPacketConnection(
      const net::IPEndPoint& ip_endpoint,
      const base::Closure& callback,
      const network_handler::ErrorCallback& error_callback) = 0;

  // Removes |types| from the list of packet types that the device should
  // monitor to wake the system from suspend.
  virtual void RemoveWifiWakeOnPacketOfTypes(
      const std::vector<std::string>& types,
      const base::Closure& callback,
      const network_handler::ErrorCallback& error_callback) = 0;

  // Clears the list of tcp connections that the wifi device should monitor to
  // wake the system from suspend.
  virtual void RemoveAllWifiWakeOnPacketConnections(
      const base::Closure& callback,
      const network_handler::ErrorCallback& error_callback) = 0;

  static std::unique_ptr<NetworkDeviceHandler> InitializeForTesting(
      NetworkStateHandler* network_state_handler);

 private:
  DISALLOW_COPY_AND_ASSIGN(NetworkDeviceHandler);
};

}  // namespace chromeos

#endif  // CHROMEOS_NETWORK_NETWORK_DEVICE_HANDLER_H_
