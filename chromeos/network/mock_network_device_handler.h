// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_NETWORK_MOCK_NETWORK_DEVICE_HANDLER_H_
#define CHROMEOS_NETWORK_MOCK_NETWORK_DEVICE_HANDLER_H_

#include <string>
#include <vector>

#include "base/callback.h"
#include "base/component_export.h"
#include "base/macros.h"
#include "base/values.h"
#include "chromeos/network/network_device_handler.h"
#include "chromeos/network/network_handler_callbacks.h"
#include "net/base/ip_endpoint.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace chromeos {

class COMPONENT_EXPORT(CHROMEOS_NETWORK) MockNetworkDeviceHandler
    : public NetworkDeviceHandler {
 public:
  MockNetworkDeviceHandler();
  virtual ~MockNetworkDeviceHandler();

  MOCK_CONST_METHOD3(
      GetDeviceProperties,
      void(const std::string& device_path,
           const network_handler::DictionaryResultCallback& callback,
           const network_handler::ErrorCallback& error_callback));

  MOCK_METHOD5(SetDeviceProperty,
               void(const std::string& device_path,
                    const std::string& property_name,
                    const base::Value& value,
                    const base::Closure& callback,
                    const network_handler::ErrorCallback& error_callback));

  MOCK_METHOD4(RegisterCellularNetwork,
               void(const std::string& device_path,
                    const std::string& network_id,
                    const base::Closure& callback,
                    const network_handler::ErrorCallback& error_callback));

  MOCK_METHOD5(RequirePin,
               void(const std::string& device_path,
                    bool require_pin,
                    const std::string& pin,
                    const base::Closure& callback,
                    const network_handler::ErrorCallback& error_callback));

  MOCK_METHOD4(EnterPin,
               void(const std::string& device_path,
                    const std::string& pin,
                    const base::Closure& callback,
                    const network_handler::ErrorCallback& error_callback));

  MOCK_METHOD5(UnblockPin,
               void(const std::string& device_path,
                    const std::string& puk,
                    const std::string& new_pin,
                    const base::Closure& callback,
                    const network_handler::ErrorCallback& error_callback));

  MOCK_METHOD5(ChangePin,
               void(const std::string& device_path,
                    const std::string& old_pin,
                    const std::string& new_pin,
                    const base::Closure& callback,
                    const network_handler::ErrorCallback& error_callback));

  MOCK_METHOD1(SetCellularAllowRoaming, void(bool allow_roaming));

  MOCK_METHOD1(SetMACAddressRandomizationEnabled, void(bool enabled));

  MOCK_METHOD1(SetUsbEthernetMacAddressSource,
               void(const std::string& enabled));

  MOCK_METHOD4(SetWifiTDLSEnabled,
               void(const std::string& ip_or_mac_address,
                    bool enabled,
                    const network_handler::StringResultCallback& callback,
                    const network_handler::ErrorCallback& error_callback));

  MOCK_METHOD3(GetWifiTDLSStatus,
               void(const std::string& ip_or_mac_address,
                    const network_handler::StringResultCallback& callback,
                    const network_handler::ErrorCallback& error_callback));

  MOCK_METHOD3(AddWifiWakeOnPacketConnection,
               void(const net::IPEndPoint& ip_endpoint,
                    const base::Closure& callback,
                    const network_handler::ErrorCallback& error_callback));

  MOCK_METHOD3(AddWifiWakeOnPacketOfTypes,
               void(const std::vector<std::string>& types,
                    const base::Closure& callback,
                    const network_handler::ErrorCallback& error_callback));

  MOCK_METHOD3(RemoveWifiWakeOnPacketOfTypes,
               void(const std::vector<std::string>& types,
                    const base::Closure& callback,
                    const network_handler::ErrorCallback& error_callback));

  MOCK_METHOD3(RemoveWifiWakeOnPacketConnection,
               void(const net::IPEndPoint& ip_endpoint,
                    const base::Closure& callback,
                    const network_handler::ErrorCallback& error_callback));

  MOCK_METHOD2(RemoveAllWifiWakeOnPacketConnections,
               void(const base::Closure& callback,
                    const network_handler::ErrorCallback& error_callback));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockNetworkDeviceHandler);
};

}  // namespace chromeos

#endif  // CHROMEOS_NETWORK_MOCK_NETWORK_DEVICE_HANDLER_H_
