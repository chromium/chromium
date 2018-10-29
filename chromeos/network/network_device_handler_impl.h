// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_NETWORK_NETWORK_DEVICE_HANDLER_IMPL_H_
#define CHROMEOS_NETWORK_NETWORK_DEVICE_HANDLER_IMPL_H_

#include <map>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/chromeos_export.h"
#include "chromeos/network/network_device_handler.h"
#include "chromeos/network/network_handler.h"
#include "chromeos/network/network_handler_callbacks.h"
#include "chromeos/network/network_state_handler_observer.h"

namespace chromeos {

class NetworkStateHandler;

class CHROMEOS_EXPORT NetworkDeviceHandlerImpl
    : public NetworkDeviceHandler,
      public NetworkStateHandlerObserver {
 public:
  ~NetworkDeviceHandlerImpl() override;

  // NetworkDeviceHandler overrides
  void GetDeviceProperties(
      const std::string& device_path,
      const network_handler::DictionaryResultCallback& callback,
      const network_handler::ErrorCallback& error_callback) const override;

  void SetDeviceProperty(
      const std::string& device_path,
      const std::string& property_name,
      const base::Value& value,
      const base::Closure& callback,
      const network_handler::ErrorCallback& error_callback) override;

  void RequestRefreshIPConfigs(
      const std::string& device_path,
      const base::Closure& callback,
      const network_handler::ErrorCallback& error_callback) override;

  void RegisterCellularNetwork(
      const std::string& device_path,
      const std::string& network_id,
      const base::Closure& callback,
      const network_handler::ErrorCallback& error_callback) override;

  void SetCarrier(
      const std::string& device_path,
      const std::string& carrier,
      const base::Closure& callback,
      const network_handler::ErrorCallback& error_callback) override;

  void RequirePin(
      const std::string& device_path,
      bool require_pin,
      const std::string& pin,
      const base::Closure& callback,
      const network_handler::ErrorCallback& error_callback) override;

  void EnterPin(const std::string& device_path,
                const std::string& pin,
                const base::Closure& callback,
                const network_handler::ErrorCallback& error_callback) override;

  void UnblockPin(
      const std::string& device_path,
      const std::string& puk,
      const std::string& new_pin,
      const base::Closure& callback,
      const network_handler::ErrorCallback& error_callback) override;

  void ChangePin(const std::string& device_path,
                 const std::string& old_pin,
                 const std::string& new_pin,
                 const base::Closure& callback,
                 const network_handler::ErrorCallback& error_callback) override;

  void SetCellularAllowRoaming(bool allow_roaming) override;

  void SetMACAddressRandomizationEnabled(bool enabled) override;

  void SetWifiTDLSEnabled(
      const std::string& ip_or_mac_address,
      bool enabled,
      const network_handler::StringResultCallback& callback,
      const network_handler::ErrorCallback& error_callback) override;

  void GetWifiTDLSStatus(
      const std::string& ip_or_mac_address,
      const network_handler::StringResultCallback& callback,
      const network_handler::ErrorCallback& error_callback) override;

  void AddWifiWakeOnPacketConnection(
      const net::IPEndPoint& ip_endpoint,
      const base::Closure& callback,
      const network_handler::ErrorCallback& error_callback) override;

  void AddWifiWakeOnPacketOfTypes(
      const std::vector<std::string>& types,
      const base::Closure& callback,
      const network_handler::ErrorCallback& error_callback) override;

  void RemoveWifiWakeOnPacketConnection(
      const net::IPEndPoint& ip_endpoint,
      const base::Closure& callback,
      const network_handler::ErrorCallback& error_callback) override;

  void RemoveWifiWakeOnPacketOfTypes(
      const std::vector<std::string>& types,
      const base::Closure& callback,
      const network_handler::ErrorCallback& error_callback) override;

  void RemoveAllWifiWakeOnPacketConnections(
      const base::Closure& callback,
      const network_handler::ErrorCallback& error_callback) override;

  // NetworkStateHandlerObserver overrides
  void DeviceListChanged() override;

 private:
  friend class NetworkHandler;
  friend class NetworkDeviceHandlerTest;

  // When there's no Wi-Fi device or there is one but we haven't asked if
  // MAC address randomization is supported yet, the value of the member
  // |mac_addr_randomizaton_supported_| will be |NOT_REQUESTED|. When we
  // try to apply the |mac_addr_randomization_enabled_| value we will
  // check whether it is supported and change to one of the other two
  // values.
  enum class MACAddressRandomizationSupport {
    NOT_REQUESTED,
    SUPPORTED,
    UNSUPPORTED
  };

  NetworkDeviceHandlerImpl();

  void Init(NetworkStateHandler* network_state_handler);

  // Apply the current value of |cellular_allow_roaming_| to all existing
  // cellular devices of Shill.
  void ApplyCellularAllowRoamingToShill();

  // Apply the current value of |mac_addr_randomization_enabled_| to wifi
  // devices.
  void ApplyMACAddressRandomizationToShill();

  // Sets the value of |mac_addr_randomization_supported_| based on
  // whether shill thinks it is supported on the wifi device. If it is
  // supported, also apply |mac_addr_randomization_enabled_| to the
  // shill device.
  void HandleMACAddressRandomization(const std::string& device_path,
                                     const base::DictionaryValue& properties);

  // Get the DeviceState for the wifi device, if any.
  const DeviceState* GetWifiDeviceState(
      const network_handler::ErrorCallback& error_callback);

  NetworkStateHandler* network_state_handler_ = nullptr;
  bool cellular_allow_roaming_ = false;
  MACAddressRandomizationSupport mac_addr_randomization_supported_ =
      MACAddressRandomizationSupport::NOT_REQUESTED;
  bool mac_addr_randomization_enabled_ = false;
  base::WeakPtrFactory<NetworkDeviceHandlerImpl> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(NetworkDeviceHandlerImpl);
};

}  // namespace chromeos

#endif  // CHROMEOS_NETWORK_NETWORK_DEVICE_HANDLER_IMPL_H_
