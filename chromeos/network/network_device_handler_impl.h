// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_NETWORK_NETWORK_DEVICE_HANDLER_IMPL_H_
#define CHROMEOS_NETWORK_NETWORK_DEVICE_HANDLER_IMPL_H_

#include <string>
#include <unordered_set>
#include <vector>

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/component_export.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/network/network_device_handler.h"
#include "chromeos/network/network_handler.h"
#include "chromeos/network/network_handler_callbacks.h"
#include "chromeos/network/network_state_handler_observer.h"

namespace chromeos {

class NetworkStateHandler;

class COMPONENT_EXPORT(CHROMEOS_NETWORK) NetworkDeviceHandlerImpl
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

  void RegisterCellularNetwork(
      const std::string& device_path,
      const std::string& network_id,
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

  void SetUsbEthernetMacAddressSource(const std::string& source) override;

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
  void DevicePropertiesUpdated(const DeviceState* device) override;

 private:
  friend class NetworkHandler;
  friend class NetworkDeviceHandler;
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

  // Applies the current value of |cellular_allow_roaming_| to all existing
  // cellular devices of Shill.
  void ApplyCellularAllowRoamingToShill();

  // Applies the current value of |mac_addr_randomization_enabled_| to wifi
  // devices.
  void ApplyMACAddressRandomizationToShill();

  // Applies the current value of |usb_ethernet_mac_address_source_| to primary
  // enabled USB Ethernet device. Does nothing if MAC address source is not
  // specified yet.
  void ApplyUsbEthernetMacAddressSourceToShill();

  // Callback to be called on MAC address source change request failure.
  // The request was called on device with |device_path| path and
  // |device_mac_address| MAC address to change MAC address source to the new
  // |mac_address_source| value.
  void OnSetUsbEthernetMacAddressSourceError(
      const std::string& device_path,
      const std::string& device_mac_address,
      const std::string& mac_address_source,
      const network_handler::ErrorCallback& error_callback,
      const std::string& shill_error_name,
      const std::string& shill_error_message);

  // Checks whether Device is enabled USB Ethernet adapter.
  bool IsUsbEnabledDevice(const DeviceState* device_state) const;

  // Updates the primary enabled USB Ethernet device path.
  void UpdatePrimaryEnabledUsbEthernetDevice();

  // Resets MAC address source property for secondary USB Ethernet devices.
  void ResetMacAddressSourceForSecondaryUsbEthernetDevices() const;

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

  std::string usb_ethernet_mac_address_source_;
  bool usb_ethernet_mac_address_source_needs_update_ = false;
  std::string primary_enabled_usb_ethernet_device_path_;
  // Set of device's MAC addresses that do not support MAC address source change
  // to |usb_ethernet_mac_address_source_|. Use MAC address as unique device
  // identifier, because link name can change.
  std::unordered_set<std::string> mac_address_change_not_supported_;

  base::WeakPtrFactory<NetworkDeviceHandlerImpl> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(NetworkDeviceHandlerImpl);
};

}  // namespace chromeos

#endif  // CHROMEOS_NETWORK_NETWORK_DEVICE_HANDLER_IMPL_H_
