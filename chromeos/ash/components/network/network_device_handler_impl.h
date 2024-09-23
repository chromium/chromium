// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_NETWORK_NETWORK_DEVICE_HANDLER_IMPL_H_
#define CHROMEOS_ASH_COMPONENTS_NETWORK_NETWORK_DEVICE_HANDLER_IMPL_H_

#include <string>
#include <unordered_set>
#include <vector>

#include "base/compiler_specific.h"
#include "base/component_export.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "chromeos/ash/components/network/cellular_metrics_logger.h"
#include "chromeos/ash/components/network/network_device_handler.h"
#include "chromeos/ash/components/network/network_handler.h"
#include "chromeos/ash/components/network/network_handler_callbacks.h"
#include "chromeos/ash/components/network/network_state_handler.h"
#include "chromeos/ash/components/network/network_state_handler_observer.h"

namespace ash {

class NetworkStateHandler;

class COMPONENT_EXPORT(CHROMEOS_NETWORK) NetworkDeviceHandlerImpl
    : public NetworkDeviceHandler,
      public NetworkStateHandlerObserver {
 public:
  NetworkDeviceHandlerImpl(const NetworkDeviceHandlerImpl&) = delete;
  NetworkDeviceHandlerImpl& operator=(const NetworkDeviceHandlerImpl&) = delete;

  ~NetworkDeviceHandlerImpl() override;

  // NetworkDeviceHandler overrides
  void GetDeviceProperties(
      const std::string& device_path,
      network_handler::ResultCallback callback) const override;

  void SetDeviceProperty(
      const std::string& device_path,
      const std::string& property_name,
      const base::Value& value,
      base::OnceClosure callback,
      network_handler::ErrorCallback error_callback) override;

  void RegisterCellularNetwork(
      const std::string& device_path,
      const std::string& network_id,
      base::OnceClosure callback,
      network_handler::ErrorCallback error_callback) override;

  void RequirePin(const std::string& device_path,
                  bool require_pin,
                  const std::string& pin,
                  base::OnceClosure callback,
                  network_handler::ErrorCallback error_callback) override;

  void EnterPin(const std::string& device_path,
                const std::string& pin,
                base::OnceClosure callback,
                network_handler::ErrorCallback error_callback) override;

  void UnblockPin(const std::string& device_path,
                  const std::string& puk,
                  const std::string& new_pin,
                  base::OnceClosure callback,
                  network_handler::ErrorCallback error_callback) override;

  void ChangePin(const std::string& device_path,
                 const std::string& old_pin,
                 const std::string& new_pin,
                 base::OnceClosure callback,
                 network_handler::ErrorCallback error_callback) override;

  void SetAllowCellularSimLock(bool allow_cellular_sim_lock) override;

  void SetCellularPolicyAllowRoaming(bool policy_allow_roaming) override;

  void SetMACAddressRandomizationEnabled(bool enabled) override;

  void SetUsbEthernetMacAddressSource(const std::string& source) override;

  // NetworkStateHandlerObserver overrides
  void DeviceListChanged() override;
  void DevicePropertiesUpdated(const DeviceState* device) override;

 private:
  friend class NetworkHandler;
  friend class NetworkDeviceHandler;
  friend class NetworkDeviceHandlerTest;

  // Some WiFi feature enablement needs to check supported property before
  // setting. e.g. MAC address randomization, wake on WiFi.
  // When there's no Wi-Fi device or there is one but we haven't asked if
  // the feature is supported yet, the value of the member, e.g.
  // |mac_addr_randomizaton_supported_|, will be |NOT_REQUESTED|. When we
  // try to apply the value e.g. |mac_addr_randomization_enabled_|, we will
  // check whether it is supported and change to one of the other two
  // values.
  enum class WifiFeatureSupport { NOT_REQUESTED, SUPPORTED, UNSUPPORTED };

  NetworkDeviceHandlerImpl();

  void Init(NetworkStateHandler* network_state_handler);

  // Applies the current value of |cellular_policy_allow_roaming_| to all
  // existing cellular devices of Shill.
  void ApplyCellularAllowRoamingToShill();

  // Applies the current value of |mac_addr_randomization_enabled_| to wifi
  // devices.
  void ApplyMACAddressRandomizationToShill();

  // Applies the wake-on-wifi-allowed feature flag to WiFi devices.
  void ApplyWakeOnWifiAllowedToShill();

  // Applies the current value of |usb_ethernet_mac_address_source_| to primary
  // enabled USB Ethernet device. Does nothing if MAC address source is not
  // specified yet.
  void ApplyUsbEthernetMacAddressSourceToShill();

  // Utility function for applying enabled setting of WiFi features that needs
  // to check if the feature is supported first.
  // This function will update |supported| if it is still NOT_REQUESTED by
  // getting |support_property_name| property of the WiFi device. Then, if it
  // is supported, set |enable_property_name| property of the WiFi device to
  // |enabled|.
  void ApplyWifiFeatureToShillIfSupported(std::string enable_property_name,
                                          bool enabled,
                                          std::string support_property_name,
                                          WifiFeatureSupport* supported);

  // Callback function used by ApplyWifiFeatureToShillIfSupported to get shill
  // property when the supported property is NOT_REQUESTED. It will extract
  // |support_property_name| of GetProperties response and update
  // |feature_support_to_set|, then call ApplyWifiFeatureToShillIfSupported
  // again if the feature is supported.
  void HandleWifiFeatureSupportedProperty(
      std::string enable_property_name,
      bool enabled,
      std::string support_property_name,
      WifiFeatureSupport* feature_support_to_set,
      const std::string& device_path,
      std::optional<base::Value::Dict> properties);

  // Callback to be called on MAC address source change request failure.
  // The request was called on device with |device_path| path and
  // |device_mac_address| MAC address to change MAC address source to the new
  // |mac_address_source| value.
  void OnSetUsbEthernetMacAddressSourceError(
      const std::string& device_path,
      const std::string& device_mac_address,
      const std::string& mac_address_source,
      network_handler::ErrorCallback error_callback,
      const std::string& shill_error_name,
      const std::string& shill_error_message);

  // Checks whether Device is enabled USB Ethernet adapter.
  bool IsUsbEnabledDevice(const DeviceState* device_state) const;

  // Updates the primary enabled USB Ethernet device path.
  void UpdatePrimaryEnabledUsbEthernetDevice();

  // Resets MAC address source property for secondary USB Ethernet devices.
  void ResetMacAddressSourceForSecondaryUsbEthernetDevices() const;

  // On a successful SIM PIN unlock, or a successful SIM PUK unblock.
  void OnPinValidationSuccess(
      const std::string& device_path,
      const std::string& pin,
      const CellularMetricsLogger::SimPinOperation& pin_operation,
      base::OnceClosure callback);

  // Get the DeviceState for the wifi device, if any.
  const DeviceState* GetWifiDeviceState();

  raw_ptr<NetworkStateHandler> network_state_handler_ = nullptr;
  base::ScopedObservation<NetworkStateHandler, NetworkStateHandlerObserver>
      network_state_handler_observer_{this};
  bool allow_cellular_sim_lock_ = true;
  bool cellular_policy_allow_roaming_ = true;
  WifiFeatureSupport mac_addr_randomization_supported_ =
      WifiFeatureSupport::NOT_REQUESTED;
  bool mac_addr_randomization_enabled_ = false;
  WifiFeatureSupport wake_on_wifi_supported_ =
      WifiFeatureSupport::NOT_REQUESTED;
  bool wake_on_wifi_allowed_ = false;

  std::string usb_ethernet_mac_address_source_;
  std::string primary_enabled_usb_ethernet_device_path_;
  // Set of device's MAC addresses that do not support MAC address source change
  // to |usb_ethernet_mac_address_source_|. Use MAC address as unique device
  // identifier, because link name can change.
  std::unordered_set<std::string> mac_address_change_not_supported_;

  base::WeakPtrFactory<NetworkDeviceHandlerImpl> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_NETWORK_NETWORK_DEVICE_HANDLER_IMPL_H_
