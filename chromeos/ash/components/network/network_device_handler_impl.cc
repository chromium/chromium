// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chromeos/ash/components/network/network_device_handler_impl.h"

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <utility>
#include <vector>

#include "ash/constants/ash_features.h"
#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/strings/string_util.h"
#include "base/system/sys_info.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chromeos/ash/components/dbus/shill/shill_device_client.h"
#include "chromeos/ash/components/dbus/shill/shill_ipconfig_client.h"
#include "chromeos/ash/components/network/cellular_metrics_logger.h"
#include "chromeos/ash/components/network/device_state.h"
#include "chromeos/ash/components/network/network_event_log.h"
#include "chromeos/ash/components/network/network_handler.h"
#include "chromeos/ash/components/network/network_state_handler.h"
#include "dbus/object_path.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace ash {

namespace {

const char kDefaultSimPin[] = "1111";

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
                           std::optional<base::Value::Dict> result) {
  if (!result) {
    NET_LOG(ERROR) << "GetProperties failed: " << NetworkPathId(device_path);
    std::move(callback).Run(device_path, std::nullopt);
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
    const bool allow_cellular_sim_lock,
    base::OnceClosure callback) {
  CellularMetricsLogger::RecordSimPinOperationResult(pin_operation,
                                                     allow_cellular_sim_lock);
  std::move(callback).Run();
}

void HandleSimPinOperationFailure(
    const CellularMetricsLogger::SimPinOperation& pin_operation,
    const bool allow_cellular_sim_lock,
    const std::string& device_path,
    network_handler::ErrorCallback error_callback,
    const std::string& shill_error_name,
    const std::string& shill_error_message) {
  CellularMetricsLogger::RecordSimPinOperationResult(
      pin_operation, allow_cellular_sim_lock, shill_error_name);
  HandleShillCallFailure(device_path, std::move(error_callback),
                         shill_error_name, shill_error_message);
}

}  // namespace

NetworkDeviceHandlerImpl::NetworkDeviceHandlerImpl() = default;

NetworkDeviceHandlerImpl::~NetworkDeviceHandlerImpl() = default;

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
  // Allow removal of the SIM PIN, but disallow requiring a SIM PIN.
  if (require_pin && !allow_cellular_sim_lock_) {
    std::move(error_callback).Run(NetworkDeviceHandler::kErrorBlockedByPolicy);
    return;
  }

  const CellularMetricsLogger::SimPinOperation pin_operation =
      require_pin ? CellularMetricsLogger::SimPinOperation::kRequireLock
                  : CellularMetricsLogger::SimPinOperation::kRemoveLock;

  NET_LOG(USER) << "Device.RequirePin: " << device_path << ": " << require_pin;
  ShillDeviceClient::Get()->RequirePin(
      dbus::ObjectPath(device_path), pin, require_pin,
      base::BindOnce(&HandleSimPinOperationSuccess, pin_operation,
                     allow_cellular_sim_lock_, std::move(callback)),
      base::BindOnce(&HandleSimPinOperationFailure, pin_operation,
                     allow_cellular_sim_lock_, device_path,
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
      base::BindOnce(&NetworkDeviceHandlerImpl::OnPinValidationSuccess,
                     weak_ptr_factory_.GetWeakPtr(), device_path, pin,
                     CellularMetricsLogger::SimPinOperation::kUnlock,
                     std::move(callback)),
      base::BindOnce(&HandleSimPinOperationFailure,
                     CellularMetricsLogger::SimPinOperation::kUnlock,
                     allow_cellular_sim_lock_, device_path,
                     std::move(error_callback)));
}

void NetworkDeviceHandlerImpl::UnblockPin(
    const std::string& device_path,
    const std::string& puk,
    const std::string& new_pin,
    base::OnceClosure callback,
    network_handler::ErrorCallback error_callback) {
  NET_LOG(USER) << "Device.UnblockPin: " << device_path;
  // A SIM PIN must be provided along with the PUK in order to PUK unblock a
  // SIM. If admins have blocked SIM PIN locking, a PIN still must be provided,
  // so th default SIM |kDefaultSimPin| is used.
  const std::string pin = allow_cellular_sim_lock_ ? new_pin : kDefaultSimPin;
  ShillDeviceClient::Get()->UnblockPin(
      dbus::ObjectPath(device_path), puk, pin,
      base::BindOnce(&NetworkDeviceHandlerImpl::OnPinValidationSuccess,
                     weak_ptr_factory_.GetWeakPtr(), device_path, pin,
                     CellularMetricsLogger::SimPinOperation::kUnblock,
                     std::move(callback)),
      base::BindOnce(&HandleSimPinOperationFailure,
                     CellularMetricsLogger::SimPinOperation::kUnblock,
                     allow_cellular_sim_lock_, device_path,
                     std::move(error_callback)));
}

void NetworkDeviceHandlerImpl::OnPinValidationSuccess(
    const std::string& device_path,
    const std::string& pin,
    const CellularMetricsLogger::SimPinOperation& pin_operation,
    base::OnceClosure callback) {
  if (allow_cellular_sim_lock_) {
    HandleSimPinOperationSuccess(pin_operation, allow_cellular_sim_lock_,
                                 std::move(callback));
    return;
  }

  // Disable the SIM PIN lock setting.
  RequirePin(device_path, false, pin,
             base::BindOnce(&HandleSimPinOperationSuccess, pin_operation,
                            allow_cellular_sim_lock_, std::move(callback)),
             base::DoNothing());
}

void NetworkDeviceHandlerImpl::ChangePin(
    const std::string& device_path,
    const std::string& old_pin,
    const std::string& new_pin,
    base::OnceClosure callback,
    network_handler::ErrorCallback error_callback) {
  if (!allow_cellular_sim_lock_) {
    std::move(error_callback).Run(NetworkDeviceHandler::kErrorBlockedByPolicy);
    return;
  }

  NET_LOG(USER) << "Device.ChangePin: " << device_path;
  ShillDeviceClient::Get()->ChangePin(
      dbus::ObjectPath(device_path), old_pin, new_pin,
      base::BindOnce(&HandleSimPinOperationSuccess,
                     CellularMetricsLogger::SimPinOperation::kChange,
                     allow_cellular_sim_lock_, std::move(callback)),
      base::BindOnce(&HandleSimPinOperationFailure,
                     CellularMetricsLogger::SimPinOperation::kChange,
                     allow_cellular_sim_lock_, device_path,
                     std::move(error_callback)));
}

void NetworkDeviceHandlerImpl::SetAllowCellularSimLock(
    bool allow_cellular_sim_lock) {
  allow_cellular_sim_lock_ = allow_cellular_sim_lock;
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
  network_state_handler_observer_.Observe(network_state_handler_.get());
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
    std::optional<base::Value::Dict> properties) {
  if (!properties) {
    return;
  }
  std::optional<bool> supported_val =
      properties->FindBool(support_property_name);
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
      base::FeatureList::IsEnabled(features::kWakeOnWifiAllowed);
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
         !base::Contains(mac_address_change_not_supported_,
                         device_state->mac_address());
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

}  // namespace ash
