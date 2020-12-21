// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/network/network_device_handler_impl.h"

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/system/sys_info.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/dbus/shill/shill_device_client.h"
#include "chromeos/dbus/shill/shill_ipconfig_client.h"
#include "chromeos/network/device_state.h"
#include "chromeos/network/network_event_log.h"
#include "chromeos/network/network_state_handler.h"
#include "dbus/object_path.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {

namespace {

std::string GetErrorNameForShillError(const std::string& shill_error_name) {
  if (shill_error_name == shill::kErrorResultFailure ||
      shill_error_name == shill::kErrorResultInvalidArguments) {
    return NetworkDeviceHandler::kErrorFailure;
  }
  if (shill_error_name == shill::kErrorResultNotSupported)
    return NetworkDeviceHandler::kErrorNotSupported;
  if (shill_error_name == shill::kErrorResultIncorrectPin)
    return NetworkDeviceHandler::kErrorIncorrectPin;
  if (shill_error_name == shill::kErrorResultPinBlocked)
    return NetworkDeviceHandler::kErrorPinBlocked;
  if (shill_error_name == shill::kErrorResultPinRequired)
    return NetworkDeviceHandler::kErrorPinRequired;
  if (shill_error_name == shill::kErrorResultNotFound)
    return NetworkDeviceHandler::kErrorDeviceMissing;
  return NetworkDeviceHandler::kErrorUnknown;
}

void GetPropertiesCallback(const std::string& device_path,
                           network_handler::ResultCallback callback,
                           base::Optional<base::Value> result) {
  if (!result) {
    NET_LOG(ERROR) << "GetProperties failed: " << NetworkPathId(device_path);
    std::move(callback).Run(device_path, base::nullopt);
    return;
  }
  std::move(callback).Run(device_path, std::move(result));
}

void InvokeErrorCallback(const std::string& device_path,
                         network_handler::ErrorCallback error_callback,
                         const std::string& error_name) {
  std::string error_msg = "Device Error: " + error_name;
  NET_LOG(ERROR) << error_msg << ": " << device_path;
  network_handler::RunErrorCallback(std::move(error_callback), device_path,
                                    error_name, error_msg);
}

void HandleShillCallFailure(const std::string& device_path,
                            network_handler::ErrorCallback error_callback,
                            const std::string& shill_error_name,
                            const std::string& shill_error_message) {
  network_handler::ShillErrorCallbackFunction(
      GetErrorNameForShillError(shill_error_name), device_path,
      std::move(error_callback), shill_error_name, shill_error_message);
}

void SetDevicePropertyInternal(const std::string& device_path,
                               const std::string& property_name,
                               const base::Value& value,
                               base::OnceClosure callback,
                               network_handler::ErrorCallback error_callback) {
  NET_LOG(USER) << "Device.SetProperty: " << property_name << " = " << value;
  ShillDeviceClient::Get()->SetProperty(
      dbus::ObjectPath(device_path), property_name, value, std::move(callback),
      base::BindOnce(&HandleShillCallFailure, device_path,
                     std::move(error_callback)));
}

std::unique_ptr<base::DictionaryValue> GetErrorData(const std::string& name) {
  auto error_data = std::make_unique<base::DictionaryValue>();
  error_data->SetString(network_handler::kErrorName, name);
  return error_data;
}

}  // namespace

NetworkDeviceHandlerImpl::NetworkDeviceHandlerImpl() = default;

NetworkDeviceHandlerImpl::~NetworkDeviceHandlerImpl() {
  if (network_state_handler_)
    network_state_handler_->RemoveObserver(this, FROM_HERE);
}

void NetworkDeviceHandlerImpl::GetDeviceProperties(
    const std::string& device_path,
    network_handler::ResultCallback callback) const {
  ShillDeviceClient::Get()->GetProperties(
      dbus::ObjectPath(device_path),
      base::BindOnce(&GetPropertiesCallback, device_path, std::move(callback)));
}

void NetworkDeviceHandlerImpl::SetDeviceProperty(
    const std::string& device_path,
    const std::string& property_name,
    const base::Value& value,
    base::OnceClosure callback,
    network_handler::ErrorCallback error_callback) {
  const char* const property_blocked[] = {
      // Must only be changed by policy/owner through.
      shill::kCellularAllowRoamingProperty};

  for (size_t i = 0; i < base::size(property_blocked); ++i) {
    if (property_name == property_blocked[i]) {
      InvokeErrorCallback(
          device_path, std::move(error_callback),
          "SetDeviceProperty called on blocked property " + property_name);
      return;
    }
  }

  SetDevicePropertyInternal(device_path, property_name, value,
                            std::move(callback), std::move(error_callback));
}

void NetworkDeviceHandlerImpl::RegisterCellularNetwork(
    const std::string& device_path,
    const std::string& network_id,
    base::OnceClosure callback,
    network_handler::ErrorCallback error_callback) {
  NET_LOG(USER) << "Device.RegisterCellularNetwork: " << device_path
                << " Id: " << network_id;
  ShillDeviceClient::Get()->Register(
      dbus::ObjectPath(device_path), network_id, std::move(callback),
      base::BindOnce(&HandleShillCallFailure, device_path,
                     std::move(error_callback)));
}

void NetworkDeviceHandlerImpl::RequirePin(
    const std::string& device_path,
    bool require_pin,
    const std::string& pin,
    base::OnceClosure callback,
    network_handler::ErrorCallback error_callback) {
  NET_LOG(USER) << "Device.RequirePin: " << device_path << ": " << require_pin;
  ShillDeviceClient::Get()->RequirePin(
      dbus::ObjectPath(device_path), pin, require_pin, std::move(callback),
      base::BindOnce(&HandleShillCallFailure, device_path,
                     std::move(error_callback)));
}

void NetworkDeviceHandlerImpl::EnterPin(
    const std::string& device_path,
    const std::string& pin,
    base::OnceClosure callback,
    network_handler::ErrorCallback error_callback) {
  NET_LOG(USER) << "Device.EnterPin: " << device_path;
  ShillDeviceClient::Get()->EnterPin(
      dbus::ObjectPath(device_path), pin, std::move(callback),
      base::BindOnce(&HandleShillCallFailure, device_path,
                     std::move(error_callback)));
}

void NetworkDeviceHandlerImpl::UnblockPin(
    const std::string& device_path,
    const std::string& puk,
    const std::string& new_pin,
    base::OnceClosure callback,
    network_handler::ErrorCallback error_callback) {
  NET_LOG(USER) << "Device.UnblockPin: " << device_path;
  ShillDeviceClient::Get()->UnblockPin(
      dbus::ObjectPath(device_path), puk, new_pin, std::move(callback),
      base::BindOnce(&HandleShillCallFailure, device_path,
                     std::move(error_callback)));
}

void NetworkDeviceHandlerImpl::ChangePin(
    const std::string& device_path,
    const std::string& old_pin,
    const std::string& new_pin,
    base::OnceClosure callback,
    network_handler::ErrorCallback error_callback) {
  NET_LOG(USER) << "Device.ChangePin: " << device_path;
  ShillDeviceClient::Get()->ChangePin(
      dbus::ObjectPath(device_path), old_pin, new_pin, std::move(callback),
      base::BindOnce(&HandleShillCallFailure, device_path,
                     std::move(error_callback)));
}

void NetworkDeviceHandlerImpl::SetCellularAllowRoaming(
    const bool allow_roaming) {
  cellular_allow_roaming_ = allow_roaming;
  ApplyCellularAllowRoamingToShill();
}

void NetworkDeviceHandlerImpl::SetMACAddressRandomizationEnabled(
    const bool enabled) {
  mac_addr_randomization_enabled_ = enabled;
  ApplyMACAddressRandomizationToShill();
}

void NetworkDeviceHandlerImpl::SetUsbEthernetMacAddressSource(
    const std::string& source) {
  if (source == usb_ethernet_mac_address_source_) {
    return;
  }

  usb_ethernet_mac_address_source_ = source;
  mac_address_change_not_supported_.clear();
  ApplyUsbEthernetMacAddressSourceToShill();
}

void NetworkDeviceHandlerImpl::AddWifiWakeOnPacketConnection(
    const net::IPEndPoint& ip_endpoint,
    base::OnceClosure callback,
    network_handler::ErrorCallback error_callback) {
  const DeviceState* device_state = GetWifiDeviceState();
  if (!device_state) {
    if (error_callback) {
      std::move(error_callback)
          .Run(kErrorDeviceMissing, GetErrorData(kErrorDeviceMissing));
    }
    return;
  }

  NET_LOG(USER) << "Device.AddWakeOnWifi: " << device_state->path();
  ShillDeviceClient::Get()->AddWakeOnPacketConnection(
      dbus::ObjectPath(device_state->path()), ip_endpoint, std::move(callback),
      base::BindOnce(&HandleShillCallFailure, device_state->path(),
                     std::move(error_callback)));
}

void NetworkDeviceHandlerImpl::AddWifiWakeOnPacketOfTypes(
    const std::vector<std::string>& types,
    base::OnceClosure callback,
    network_handler::ErrorCallback error_callback) {
  const DeviceState* device_state = GetWifiDeviceState();
  if (!device_state) {
    if (error_callback) {
      std::move(error_callback)
          .Run(kErrorDeviceMissing, GetErrorData(kErrorDeviceMissing));
    }
    return;
  }

  NET_LOG(USER) << "Device.AddWifiWakeOnPacketOfTypes: " << device_state->path()
                << " Types: " << base::JoinString(types, " ");
  ShillDeviceClient::Get()->AddWakeOnPacketOfTypes(
      dbus::ObjectPath(device_state->path()), types, std::move(callback),
      base::BindOnce(&HandleShillCallFailure, device_state->path(),
                     std::move(error_callback)));
}

void NetworkDeviceHandlerImpl::RemoveWifiWakeOnPacketConnection(
    const net::IPEndPoint& ip_endpoint,
    base::OnceClosure callback,
    network_handler::ErrorCallback error_callback) {
  const DeviceState* device_state = GetWifiDeviceState();
  if (!device_state) {
    if (error_callback) {
      std::move(error_callback)
          .Run(kErrorDeviceMissing, GetErrorData(kErrorDeviceMissing));
    }
    return;
  }

  NET_LOG(USER) << "Device.RemoveWakeOnWifi: " << device_state->path();
  ShillDeviceClient::Get()->RemoveWakeOnPacketConnection(
      dbus::ObjectPath(device_state->path()), ip_endpoint, std::move(callback),
      base::BindOnce(&HandleShillCallFailure, device_state->path(),
                     std::move(error_callback)));
}

void NetworkDeviceHandlerImpl::RemoveWifiWakeOnPacketOfTypes(
    const std::vector<std::string>& types,
    base::OnceClosure callback,
    network_handler::ErrorCallback error_callback) {
  const DeviceState* device_state = GetWifiDeviceState();
  if (!device_state) {
    if (error_callback) {
      std::move(error_callback)
          .Run(kErrorDeviceMissing, GetErrorData(kErrorDeviceMissing));
    }
    return;
  }

  NET_LOG(USER) << "Device.RemoveWifiWakeOnPacketOfTypes: "
                << device_state->path()
                << " Types: " << base::JoinString(types, " ");
  ShillDeviceClient::Get()->RemoveWakeOnPacketOfTypes(
      dbus::ObjectPath(device_state->path()), types, std::move(callback),
      base::BindOnce(&HandleShillCallFailure, device_state->path(),
                     std::move(error_callback)));
}

void NetworkDeviceHandlerImpl::RemoveAllWifiWakeOnPacketConnections(
    base::OnceClosure callback,
    network_handler::ErrorCallback error_callback) {
  const DeviceState* device_state = GetWifiDeviceState();
  if (!device_state) {
    if (error_callback) {
      std::move(error_callback)
          .Run(kErrorDeviceMissing, GetErrorData(kErrorDeviceMissing));
    }
    return;
  }

  NET_LOG(USER) << "Device.RemoveAllWakeOnWifi: " << device_state->path();
  ShillDeviceClient::Get()->RemoveAllWakeOnPacketConnections(
      dbus::ObjectPath(device_state->path()), std::move(callback),
      base::BindOnce(&HandleShillCallFailure, device_state->path(),
                     std::move(error_callback)));
}

void NetworkDeviceHandlerImpl::DeviceListChanged() {
  ApplyCellularAllowRoamingToShill();
  ApplyMACAddressRandomizationToShill();
  ApplyUsbEthernetMacAddressSourceToShill();
  ApplyUseAttachApnToShill();
}

void NetworkDeviceHandlerImpl::DevicePropertiesUpdated(
    const DeviceState* device) {
  ApplyUsbEthernetMacAddressSourceToShill();
}

void NetworkDeviceHandlerImpl::Init(
    NetworkStateHandler* network_state_handler) {
  DCHECK(network_state_handler);
  network_state_handler_ = network_state_handler;
  network_state_handler_->AddObserver(this, FROM_HERE);
}

void NetworkDeviceHandlerImpl::ApplyCellularAllowRoamingToShill() {
  NetworkStateHandler::DeviceStateList list;
  network_state_handler_->GetDeviceListByType(NetworkTypePattern::Cellular(),
                                              &list);
  if (list.empty()) {
    NET_LOG(DEBUG) << "No cellular device available. Roaming is only supported "
                      "by cellular devices.";
    return;
  }
  for (NetworkStateHandler::DeviceStateList::const_iterator it = list.begin();
       it != list.end(); ++it) {
    const DeviceState* device_state = *it;
    bool current_allow_roaming = device_state->allow_roaming();

    // If roaming is required by the provider, always try to set to true.
    bool new_device_value =
        device_state->provider_requires_roaming() || cellular_allow_roaming_;

    // Only set the value if the current value is different from
    // |new_device_value|.
    if (new_device_value == current_allow_roaming)
      continue;

    SetDevicePropertyInternal(device_state->path(),
                              shill::kCellularAllowRoamingProperty,
                              base::Value(new_device_value), base::DoNothing(),
                              network_handler::ErrorCallback());
  }
}

void NetworkDeviceHandlerImpl::ApplyMACAddressRandomizationToShill() {
  const DeviceState* device_state = GetWifiDeviceState();
  if (!device_state) {
    // We'll need to ask if this is supported when we find a Wi-Fi
    // device.
    mac_addr_randomization_supported_ =
        MACAddressRandomizationSupport::NOT_REQUESTED;
    return;
  }

  switch (mac_addr_randomization_supported_) {
    case MACAddressRandomizationSupport::NOT_REQUESTED:
      GetDeviceProperties(
          device_state->path(),
          base::BindOnce(
              &NetworkDeviceHandlerImpl::HandleMACAddressRandomization,
              weak_ptr_factory_.GetWeakPtr()));
      return;
    case MACAddressRandomizationSupport::SUPPORTED:
      SetDevicePropertyInternal(
          device_state->path(), shill::kMacAddressRandomizationEnabledProperty,
          base::Value(mac_addr_randomization_enabled_), base::DoNothing(),
          network_handler::ErrorCallback());
      return;
    case MACAddressRandomizationSupport::UNSUPPORTED:
      return;
  }
}

void NetworkDeviceHandlerImpl::ApplyUsbEthernetMacAddressSourceToShill() {
  // Do nothing else if MAC address source is not specified yet.
  if (usb_ethernet_mac_address_source_.empty()) {
    NET_LOG(DEBUG) << "Empty USB Ethernet MAC address source.";
    return;
  }

  UpdatePrimaryEnabledUsbEthernetDevice();
  ResetMacAddressSourceForSecondaryUsbEthernetDevices();

  const DeviceState* primary_enabled_usb_ethernet_device_state =
      network_state_handler_->GetDeviceState(
          primary_enabled_usb_ethernet_device_path_);

  // Do nothing else if device path is empty or device state is nullptr or
  // device MAC address source property equals to needed value.
  if (primary_enabled_usb_ethernet_device_path_.empty() ||
      !primary_enabled_usb_ethernet_device_state ||
      primary_enabled_usb_ethernet_device_state->mac_address_source() ==
          usb_ethernet_mac_address_source_) {
    return;
  }

  ShillDeviceClient::Get()->SetUsbEthernetMacAddressSource(
      dbus::ObjectPath(primary_enabled_usb_ethernet_device_path_),
      usb_ethernet_mac_address_source_, base::DoNothing(),
      base::BindOnce(
          &NetworkDeviceHandlerImpl::OnSetUsbEthernetMacAddressSourceError,
          weak_ptr_factory_.GetWeakPtr(),
          primary_enabled_usb_ethernet_device_path_,
          primary_enabled_usb_ethernet_device_state->mac_address(),
          usb_ethernet_mac_address_source_, network_handler::ErrorCallback()));
}

void NetworkDeviceHandlerImpl::ApplyUseAttachApnToShill() {
  NetworkStateHandler::DeviceStateList list;
  bool use_attach_apn =
      base::FeatureList::IsEnabled(chromeos::features::kCellularUseAttachApn);
  network_state_handler_->GetDeviceListByType(NetworkTypePattern::Cellular(),
                                              &list);
  if (list.empty()) {
    NET_LOG(DEBUG) << "No cellular device available.";
    return;
  }
  for (NetworkStateHandler::DeviceStateList::const_iterator it = list.begin();
       it != list.end(); ++it) {
    const DeviceState* device_state = *it;

    SetDevicePropertyInternal(device_state->path(),
                              shill::kUseAttachAPNProperty,
                              base::Value(use_attach_apn), base::DoNothing(),
                              network_handler::ErrorCallback());
  }
}

void NetworkDeviceHandlerImpl::OnSetUsbEthernetMacAddressSourceError(
    const std::string& device_path,
    const std::string& device_mac_address,
    const std::string& mac_address_source,
    network_handler::ErrorCallback error_callback,
    const std::string& shill_error_name,
    const std::string& shill_error_message) {
  HandleShillCallFailure(device_path, std::move(error_callback),
                         shill_error_name, shill_error_message);
  if (shill_error_name == shill::kErrorResultNotSupported &&
      mac_address_source == usb_ethernet_mac_address_source_) {
    mac_address_change_not_supported_.insert(device_mac_address);
    ApplyUsbEthernetMacAddressSourceToShill();
  }
}

bool NetworkDeviceHandlerImpl::IsUsbEnabledDevice(
    const DeviceState* device_state) const {
  return device_state && device_state->link_up() &&
         device_state->Matches(NetworkTypePattern::Ethernet()) &&
         device_state->device_bus_type() == shill::kDeviceBusTypeUsb &&
         mac_address_change_not_supported_.find(device_state->mac_address()) ==
             mac_address_change_not_supported_.end();
}

void NetworkDeviceHandlerImpl::UpdatePrimaryEnabledUsbEthernetDevice() {
  NetworkStateHandler::DeviceStateList device_state_list;
  network_state_handler_->GetDeviceListByType(NetworkTypePattern::Ethernet(),
                                              &device_state_list);

  // Try to avoid situation when both PCI and USB Ethernet devices are enabled
  // and have the same MAC address. In this situation we will change back USB
  // Ethernet MAC address.
  if (usb_ethernet_mac_address_source_ ==
      shill::kUsbEthernetMacAddressSourceBuiltinAdapterMac) {
    for (const auto* device_state : device_state_list) {
      if (device_state && device_state->link_up() &&
          device_state->device_bus_type() == shill::kDeviceBusTypePci) {
        primary_enabled_usb_ethernet_device_path_ = "";
        return;
      }
    }
  }

  // Nothing change, primary USB Ethernet device still enabled.
  // Note that if |primary_enabled_usb_ethernet_device_path_| is empty, this
  // will be IsUsbEnabledDevice(nullptr) which returns false.
  if (IsUsbEnabledDevice(network_state_handler_->GetDeviceState(
          primary_enabled_usb_ethernet_device_path_))) {
    return;
  }

  // Reset primary enabled USB Ethernet device since it isn't enabled anymore.
  primary_enabled_usb_ethernet_device_path_ = "";

  // Give the priority to USB Ethernet device which already has the required MAC
  // address source property. It can happen after Chrome crashes, when shill
  // devices have some properties and Chrome does not know which device was
  // the primary USB Ethernet before the crash.
  for (const auto* device_state : device_state_list) {
    if (IsUsbEnabledDevice(device_state) && device_state &&
        device_state->mac_address_source() ==
            usb_ethernet_mac_address_source_) {
      primary_enabled_usb_ethernet_device_path_ = device_state->path();
      return;
    }
  }

  for (const auto* device_state : device_state_list) {
    if (IsUsbEnabledDevice(device_state)) {
      primary_enabled_usb_ethernet_device_path_ = device_state->path();
      return;
    }
  }
}

void NetworkDeviceHandlerImpl::
    ResetMacAddressSourceForSecondaryUsbEthernetDevices() const {
  NetworkStateHandler::DeviceStateList device_state_list;
  network_state_handler_->GetDeviceListByType(NetworkTypePattern::Ethernet(),
                                              &device_state_list);

  for (const auto* device_state : device_state_list) {
    if (!device_state ||
        device_state->path() == primary_enabled_usb_ethernet_device_path_ ||
        device_state->mac_address_source().empty() ||
        device_state->mac_address_source() ==
            shill::kUsbEthernetMacAddressSourceUsbAdapterMac) {
      continue;
    }
    ShillDeviceClient::Get()->SetUsbEthernetMacAddressSource(
        dbus::ObjectPath(device_state->path()),
        shill::kUsbEthernetMacAddressSourceUsbAdapterMac, base::DoNothing(),
        base::BindOnce(&HandleShillCallFailure, device_state->path(),
                       network_handler::ErrorCallback()));
  }
}

void NetworkDeviceHandlerImpl::HandleMACAddressRandomization(
    const std::string& device_path,
    base::Optional<base::Value> properties) {
  if (!properties)
    return;
  base::Optional<bool> supported =
      properties->FindBoolKey(shill::kMacAddressRandomizationSupportedProperty);
  if (!supported.has_value()) {
    if (base::SysInfo::IsRunningOnChromeOS()) {
      NET_LOG(ERROR) << "Failed to determine if device " << device_path
                     << " supports MAC address randomization";
    }
    return;
  }

  // Try to set MAC address randomization if it's supported.
  if (*supported) {
    mac_addr_randomization_supported_ =
        MACAddressRandomizationSupport::SUPPORTED;
    ApplyMACAddressRandomizationToShill();
  } else {
    mac_addr_randomization_supported_ =
        MACAddressRandomizationSupport::UNSUPPORTED;
  }
}

const DeviceState* NetworkDeviceHandlerImpl::GetWifiDeviceState() {
  return network_state_handler_->GetDeviceStateByType(
      NetworkTypePattern::WiFi());
}

}  // namespace chromeos
