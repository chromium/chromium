// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/network/network_device_handler_impl.h"

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <utility>
#include <vector>

#include "ash/constants/ash_features.h"
#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/feature_list.h"
#include "base/location.h"
#include "base/strings/string_util.h"
#include "base/system/sys_info.h"
#include "base/task/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chromeos/dbus/shill/shill_device_client.h"
#include "chromeos/dbus/shill/shill_ipconfig_client.h"
#include "chromeos/network/cellular_metrics_logger.h"
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
                           absl::optional<base::Value> result) {
  if (!result) {
    NET_LOG(ERROR) << "GetProperties failed: " << NetworkPathId(device_path);
    std::move(callback).Run(device_path, absl::nullopt);
    return;
  }
  std::move(callback).Run(device_path, std::move(result));
}

void InvokeErrorCallback(const std::string& device_path,
                         network_handler::ErrorCallback error_callback,
                         const std::string& error_name) {
  NET_LOG(ERROR) << "Device Error: " << error_name << ": " << device_path;
  network_handler::RunErrorCallback(std::move(error_callback), error_name);
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

void HandleSimPinOperationSuccess(
    const CellularMetricsLogger::SimPinOperation& pin_operation,
    base::OnceClosure callback) {
  CellularMetricsLogger::RecordSimPinOperationResult(pin_operation);
  std::move(callback).Run();
}

void HandleSimPinOperationFailure(
    const CellularMetricsLogger::SimPinOperation& pin_operation,
    const std::string& device_path,
    network_handler::ErrorCallback error_callback,
    const std::string& shill_error_name,
    const std::string& shill_error_message) {
  CellularMetricsLogger::RecordSimPinOperationResult(pin_operation,
                                                     shill_error_message);
  HandleShillCallFailure(device_path, std::move(error_callback),
                         shill_error_name, shill_error_message);
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
  const char* const blocked_properties[] = {
      // Must only be changed by policy/owner through
      // NetworkConfigurationUpdater.
      shill::kCellularPolicyAllowRoamingProperty};

  for (size_t i = 0; i < std::size(blocked_properties); ++i) {
    if (property_name == blocked_properties[i]) {
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
      dbus::ObjectPath(device_path), pin, require_pin,
      base::BindOnce(&HandleSimPinOperationSuccess,
                     CellularMetricsLogger::SimPinOperation::kLock,
                     std::move(callback)),
      base::BindOnce(&HandleSimPinOperationFailure,
                     CellularMetricsLogger::SimPinOperation::kLock, device_path,
                     std::move(error_callback)));
}

void NetworkDeviceHandlerImpl::EnterPin(
    const std::string& device_path,
    const std::string& pin,
    base::OnceClosure callback,
    network_handler::ErrorCallback error_callback) {
  NET_LOG(USER) << "Device.EnterPin: " << device_path;
  ShillDeviceClient::Get()->EnterPin(
      dbus::ObjectPath(device_path), pin,
      base::BindOnce(&HandleSimPinOperationSuccess,
                     CellularMetricsLogger::SimPinOperation::kUnlock,
                     std::move(callback)),
      base::BindOnce(&HandleSimPinOperationFailure,
                     CellularMetricsLogger::SimPinOperation::kUnlock,
                     device_path, std::move(error_callback)));
}

void NetworkDeviceHandlerImpl::UnblockPin(
    const std::string& device_path,
    const std::string& puk,
    const std::string& new_pin,
    base::OnceClosure callback,
    network_handler::ErrorCallback error_callback) {
  NET_LOG(USER) << "Device.UnblockPin: " << device_path;
  ShillDeviceClient::Get()->UnblockPin(
      dbus::ObjectPath(device_path), puk, new_pin,
      base::BindOnce(&HandleSimPinOperationSuccess,
                     CellularMetricsLogger::SimPinOperation::kUnblock,
                     std::move(callback)),
      base::BindOnce(&HandleSimPinOperationFailure,
                     CellularMetricsLogger::SimPinOperation::kUnblock,
                     device_path, std::move(error_callback)));
}

void NetworkDeviceHandlerImpl::ChangePin(
    const std::string& device_path,
    const std::string& old_pin,
    const std::string& new_pin,
    base::OnceClosure callback,
    network_handler::ErrorCallback error_callback) {
  NET_LOG(USER) << "Device.ChangePin: " << device_path;
  ShillDeviceClient::Get()->ChangePin(
      dbus::ObjectPath(device_path), old_pin, new_pin,
      base::BindOnce(&HandleSimPinOperationSuccess,
                     CellularMetricsLogger::SimPinOperation::kChange,
                     std::move(callback)),
      base::BindOnce(&HandleSimPinOperationFailure,
                     CellularMetricsLogger::SimPinOperation::kChange,
                     device_path, std::move(error_callback)));
}

void NetworkDeviceHandlerImpl::SetCellularPolicyAllowRoaming(
    const bool policy_allow_roaming) {
  cellular_policy_allow_roaming_ = policy_allow_roaming;
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

void NetworkDeviceHandlerImpl::DeviceListChanged() {
  ApplyCellularAllowRoamingToShill();
  ApplyMACAddressRandomizationToShill();
  ApplyUsbEthernetMacAddressSourceToShill();
  ApplyUseAttachApnToShill();
  ApplyWakeOnWifiAllowedToShill();
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
    SetDevicePropertyInternal(
        (*it)->path(), shill::kCellularPolicyAllowRoamingProperty,
        base::Value(cellular_policy_allow_roaming_), base::DoNothing(),
        network_handler::ErrorCallback());
  }
}

void NetworkDeviceHandlerImpl::ApplyWifiFeatureToShillIfSupported(
    std::string enable_property_name,
    bool enabled,
    std::string support_property_name,
    WifiFeatureSupport* supported) {
  const DeviceState* device_state = GetWifiDeviceState();
  if (!device_state) {
    *supported = WifiFeatureSupport::NOT_REQUESTED;
    return;
  }
  switch (*supported) {
    case WifiFeatureSupport::NOT_REQUESTED:
      GetDeviceProperties(
          device_state->path(),
          base::BindOnce(
              &NetworkDeviceHandlerImpl::HandleWifiFeatureSupportedProperty,
              weak_ptr_factory_.GetWeakPtr(), std::move(enable_property_name),
              enabled, std::move(support_property_name), supported));
      return;
    case WifiFeatureSupport::SUPPORTED:
      SetDevicePropertyInternal(device_state->path(), enable_property_name,
                                base::Value(enabled), base::DoNothing(),
                                network_handler::ErrorCallback());
      return;
    case WifiFeatureSupport::UNSUPPORTED:
      return;
  }
}

void NetworkDeviceHandlerImpl::HandleWifiFeatureSupportedProperty(
    std::string enable_property_name,
    bool enabled,
    std::string support_property_name,
    WifiFeatureSupport* feature_support_to_set,
    const std::string& device_path,
    absl::optional<base::Value> properties) {
  if (!properties) {
    return;
  }
  absl::optional<bool> supported_val =
      properties->FindBoolKey(support_property_name);
  if (!supported_val.has_value()) {
    if (base::SysInfo::IsRunningOnChromeOS()) {
      NET_LOG(ERROR) << "Failed to get support property "
                     << support_property_name << " from device " << device_path;
    }
    return;
  }

  // Try to set MAC address randomization if it's supported.
  if (*supported_val) {
    *feature_support_to_set = WifiFeatureSupport::SUPPORTED;
    ApplyWifiFeatureToShillIfSupported(std::move(enable_property_name), enabled,
                                       std::move(support_property_name),
                                       feature_support_to_set);
  } else {
    *feature_support_to_set = WifiFeatureSupport::UNSUPPORTED;
  }
}

void NetworkDeviceHandlerImpl::ApplyMACAddressRandomizationToShill() {
  ApplyWifiFeatureToShillIfSupported(
      shill::kMacAddressRandomizationEnabledProperty,
      mac_addr_randomization_enabled_,
      shill::kMacAddressRandomizationSupportedProperty,
      &mac_addr_randomization_supported_);
}

void NetworkDeviceHandlerImpl::ApplyWakeOnWifiAllowedToShill() {
  // Get the setting from feature flags.
  wake_on_wifi_allowed_ =
      base::FeatureList::IsEnabled(chromeos::features::kWakeOnWifiAllowed);
  ApplyWifiFeatureToShillIfSupported(
      shill::kWakeOnWiFiAllowedProperty, wake_on_wifi_allowed_,
      shill::kWakeOnWiFiSupportedProperty, &wake_on_wifi_supported_);
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
  network_state_handler_->GetDeviceListByType(NetworkTypePattern::Cellular(),
                                              &list);
  if (list.empty()) {
    NET_LOG(DEBUG) << "No cellular device available.";
    return;
  }
  for (NetworkStateHandler::DeviceStateList::const_iterator it = list.begin();
       it != list.end(); ++it) {
    const DeviceState* device_state = *it;

    SetDevicePropertyInternal(
        device_state->path(), shill::kUseAttachAPNProperty,
        base::Value(features::ShouldUseAttachApn()), base::DoNothing(),
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

const DeviceState* NetworkDeviceHandlerImpl::GetWifiDeviceState() {
  return network_state_handler_->GetDeviceStateByType(
      NetworkTypePattern::WiFi());
}

}  // namespace chromeos
