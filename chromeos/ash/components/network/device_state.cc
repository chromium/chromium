// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/network/device_state.h"

#include <memory>
#include <string>

#include "ash/constants/ash_features.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/values.h"
#include "chromeos/ash/components/network/network_event_log.h"
#include "chromeos/ash/components/network/shill_property_util.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace ash {
namespace {

bool IpTypeMatchesIpConfigMethod(const std::string& type,
                                 const std::string& method) {
  if (type == method) {
    return true;
  }
  if (type == shill::kTypeIPv4) {
    return method == shill::kTypeDHCP;
  }
  return type == shill::kTypeIPv6 &&
         (method == shill::kTypeDHCP6 || method == shill::kTypeSLAAC);
}

}  // namespace

DeviceState::DeviceState(const std::string& path)
    : ManagedState(MANAGED_TYPE_DEVICE, path) {}

DeviceState::~DeviceState() = default;

bool DeviceState::PropertyChanged(const std::string& key,
                                  const base::Value& value) {
  // All property values get stored in |properties_|.
  properties_.Set(key, value.Clone());

  if (ManagedStatePropertyChanged(key, value))
    return true;
  if (key == shill::kAddressProperty) {
    return GetStringValue(key, value, &mac_address_);
  } else if (key == shill::kInterfaceProperty) {
    return GetStringValue(key, value, &interface_);
  } else if (key == shill::kScanningProperty) {
    return GetBooleanValue(key, value, &scanning_);
  } else if (key == shill::kSupportNetworkScanProperty) {
    return GetBooleanValue(key, value, &support_network_scan_);
  } else if (key == shill::kProviderRequiresRoamingProperty) {
    return GetBooleanValue(key, value, &provider_requires_roaming_);
  } else if (key == shill::kHomeProviderProperty) {
    if (!value.is_dict()) {
      operator_name_.clear();
      country_code_.clear();
      return true;
    }
    const base::Value::Dict& dict = value.GetDict();

    const std::string* operator_name = dict.FindString(shill::kOperatorNameKey);
    if (operator_name) {
      operator_name_ = *operator_name;
    }
    if (operator_name_.empty()) {
      const std::string* operator_code =
          dict.FindString(shill::kOperatorCodeKey);
      operator_name_ = operator_code ? *operator_code : std::string();
    }
    const std::string* country_code =
        dict.FindString(shill::kOperatorCountryKey);
    country_code_ = country_code ? *country_code : std::string();
  } else if (key == shill::kTechnologyFamilyProperty) {
    return GetStringValue(key, value, &technology_family_);
  } else if (key == shill::kFoundNetworksProperty) {
    if (!value.is_list()) {
      return false;
    }
    CellularScanResults parsed_results;
    if (!network_util::ParseCellularScanResults(value.GetList(),
                                                &parsed_results)) {
      return false;
    }
    scan_results_.swap(parsed_results);
    return true;
  } else if (key == shill::kSIMSlotInfoProperty) {
    if (!value.is_list()) {
      return false;
    }
    CellularSIMSlotInfos parsed_results;
    if (!network_util::ParseCellularSIMSlotInfo(value.GetList(),
                                                &parsed_results)) {
      return false;
    }
    sim_slot_infos_.swap(parsed_results);
    return true;
  } else if (key == shill::kSIMLockStatusProperty) {
    if (!value.is_dict()) {
      return false;
    }
    const base::Value::Dict& dict = value.GetDict();

    // Set default values for SIM properties.
    sim_lock_type_.erase();
    sim_retries_left_ = 0;
    sim_lock_enabled_ = false;

    const base::Value* out_value = nullptr;
    out_value = dict.Find(shill::kSIMLockTypeProperty);
    if (out_value) {
      GetStringValue(shill::kSIMLockTypeProperty, *out_value, &sim_lock_type_);
    }
    out_value = dict.Find(shill::kSIMLockRetriesLeftProperty);
    if (out_value) {
      GetIntegerValue(shill::kSIMLockRetriesLeftProperty, *out_value,
                      &sim_retries_left_);
    }
    out_value = dict.Find(shill::kSIMLockEnabledProperty);
    if (out_value) {
      GetBooleanValue(shill::kSIMLockEnabledProperty, *out_value,
                      &sim_lock_enabled_);
    }
    return true;
  } else if (key == shill::kMeidProperty) {
    return GetStringValue(key, value, &meid_);
  } else if (key == shill::kImeiProperty) {
    return GetStringValue(key, value, &imei_);
  } else if (key == shill::kIccidProperty) {
    return GetStringValue(key, value, &iccid_);
  } else if (key == shill::kMdnProperty) {
    return GetStringValue(key, value, &mdn_);
  } else if (key == shill::kSIMPresentProperty) {
    return GetBooleanValue(key, value, &sim_present_);
  } else if (key == shill::kCellularApnListProperty) {
    if (!value.is_list())
      return false;
    apn_list_ = value.GetList().Clone();
    return true;
  } else if (key == shill::kInhibitedProperty) {
    return GetBooleanValue(key, value, &inhibited_);
  } else if (key == shill::kEapAuthenticationCompletedProperty) {
    return GetBooleanValue(key, value, &eap_authentication_completed_);
  } else if (key == shill::kIPConfigsProperty) {
    // If kIPConfigsProperty changes, clear any previous ip_configs_.
    // ShillPropertyhandler will request the IPConfig objects which will trigger
    // calls to IPConfigPropertiesChanged.
    ip_configs_.clear();
    return false;  // No actual state change.
  } else if (key == shill::kLinkUpProperty) {
    return GetBooleanValue(key, value, &link_up_);
  } else if (key == shill::kDeviceBusTypeProperty) {
    return GetStringValue(key, value, &device_bus_type_);
  } else if (key == shill::kUsbEthernetMacAddressSourceProperty) {
    return GetStringValue(key, value, &mac_address_source_);
  } else if (key == shill::kFlashingProperty) {
    return GetBooleanValue(key, value, &flashing_);
  }
  return false;
}

bool DeviceState::IsActive() const {
  return true;
}

void DeviceState::IPConfigPropertiesChanged(const std::string& ip_config_path,
                                            base::Value::Dict properties) {
  NET_LOG(EVENT) << "IPConfig for: " << path()
                 << " Changed: " << ip_config_path;
  ip_configs_.Set(ip_config_path, std::move(properties));
}

std::string DeviceState::GetName() const {
  if (!operator_name_.empty())
    return operator_name_;
  return name();
}

DeviceState::CellularSIMSlotInfos DeviceState::GetSimSlotInfos() const {
  // If information was provided from Shill, return it directly.
  if (!sim_slot_infos_.empty())
    return sim_slot_infos_;

  // Non-cellular types do not have any SIM slots.
  if (type() != shill::kTypeCellular) {
    NET_LOG(ERROR) << "Attempted to fetch SIM slots for device of type "
                   << type() << ". Returning empty list.";
    return {};
  }

  // Some devices do not return SIMSlotInfo properties (see b/189874098). If the
  // list is currently empty, we assume that this is a single-pSIM device and
  // return one CellularSIMSlotInfo object representing the single pSIM.
  CellularSIMSlotInfo info;
  info.slot_id = 1;          // Slot numbers start at 1, not 0.
  info.eid = std::string();  // Empty EID implies a physical SIM slot.
  info.iccid = iccid();      // Copy ICCID property.
  info.primary = true;       // Only one slot, so it must be the primary one.

  return CellularSIMSlotInfos{info};
}

std::string DeviceState::GetIpAddressByType(const std::string& type) const {
  for (const auto iter : ip_configs_) {
    if (!iter.second.is_dict())
      continue;
    const base::Value::Dict& ip_config = iter.second.GetDict();
    const std::string* ip_config_method =
        ip_config.FindString(shill::kMethodProperty);
    if (!ip_config_method)
      continue;
    if (IpTypeMatchesIpConfigMethod(type, *ip_config_method)) {
      const std::string* address =
          ip_config.FindString(shill::kAddressProperty);
      if (!address)
        continue;
      return *address;
    }
  }
  return std::string();
}

bool DeviceState::IsSimAbsent() const {
  return technology_family_ != shill::kTechnologyFamilyCdma && !sim_present_;
}

bool DeviceState::IsSimLocked() const {
  if (technology_family_ == shill::kTechnologyFamilyCdma || !sim_present_)
    return false;
  if (sim_lock_type_ == shill::kSIMLockPin ||
      sim_lock_type_ == shill::kSIMLockPuk) {
    return true;
  }
  return sim_lock_type_ == shill::kSIMLockNetworkPin;
}

bool DeviceState::IsSimCarrierLocked() const {
  if (technology_family_ == shill::kTechnologyFamilyCdma || !sim_present_) {
    return false;
  }
  return sim_lock_type_ == shill::kSIMLockNetworkPin;
}

bool DeviceState::HasAPN(const std::string& access_point_name) const {
  for (const auto& apn : apn_list_) {
    // bogus empty entries in the list might have been converted to a list while
    // traveling over D-Bus, skip them rather than crashing below.
    if (!apn.is_dict())
      continue;

    const std::string* apn_name = apn.GetDict().FindString(shill::kApnProperty);
    if (apn_name && *apn_name == access_point_name) {
      return true;
    }
  }
  return false;
}

}  // namespace ash
