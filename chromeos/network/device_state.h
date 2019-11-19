// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_NETWORK_DEVICE_STATE_H_
#define CHROMEOS_NETWORK_DEVICE_STATE_H_

#include <stdint.h>

#include "base/macros.h"
#include "base/values.h"
#include "chromeos/network/managed_state.h"
#include "chromeos/network/network_util.h"

namespace chromeos {

// Simple class to provide device state information. Similar to NetworkState;
// see network_state.h for usage guidelines.
class COMPONENT_EXPORT(CHROMEOS_NETWORK) DeviceState : public ManagedState {
 public:
  typedef std::vector<CellularScanResult> CellularScanResults;

  explicit DeviceState(const std::string& path);
  ~DeviceState() override;

  // ManagedState overrides
  bool PropertyChanged(const std::string& key,
                       const base::Value& value) override;

  void IPConfigPropertiesChanged(const std::string& ip_config_path,
                                 const base::Value& properties);

  // Accessors
  const std::string& mac_address() const { return mac_address_; }
  const std::string& interface() const { return interface_; }
  bool scanning() const { return scanning_; }
  void set_scanning(bool scanning) { scanning_ = scanning; }

  // Cellular specific accessors
  const std::string& operator_name() const { return operator_name_; }
  const std::string& country_code() const { return country_code_; }
  bool allow_roaming() const { return allow_roaming_; }
  bool provider_requires_roaming() const { return provider_requires_roaming_; }
  bool support_network_scan() const { return support_network_scan_; }
  const std::string& technology_family() const { return technology_family_; }
  bool sim_present() const { return sim_present_; }
  const std::string& sim_lock_type() const { return sim_lock_type_; }
  int sim_retries_left() const { return sim_retries_left_; }
  bool sim_lock_enabled() const { return sim_lock_enabled_; }
  const std::string& meid() const { return meid_; }
  const std::string& imei() const { return imei_; }
  const std::string& iccid() const { return iccid_; }
  const std::string& mdn() const { return mdn_; }
  const CellularScanResults& scan_results() const { return scan_results_; }

  // |ip_configs_| is kept up to date by NetworkStateHandler.
  const base::DictionaryValue& ip_configs() const { return ip_configs_; }

  // Do not use this. It exists temporarily for internet_options_handler.cc
  // which is being deprecated.
  const base::DictionaryValue& properties() const { return properties_; }

  // Ethernet specific accessors
  bool eap_authentication_completed() const {
    return eap_authentication_completed_;
  }
  bool link_up() const { return link_up_; }
  const std::string& device_bus_type() const { return device_bus_type_; }
  const std::string& mac_address_source() const { return mac_address_source_; }

  // WiFi specific accessors
  const std::string& available_managed_network_path() const {
    return available_managed_network_path_;
  }
  void set_available_managed_network_path(
      const std::string available_managed_network_path) {
    available_managed_network_path_ = available_managed_network_path;
  }

  // Returns a human readable string for the device.
  std::string GetName() const;

  // Returns the IP Address for |type| if it exists or an empty string.
  std::string GetIpAddressByType(const std::string& type) const;

  // The following return false if the technology does not require a SIM.
  bool IsSimAbsent() const;
  bool IsSimLocked() const;

 private:
  // Common Device Properties
  std::string mac_address_;
  std::string interface_;

  // Cellular specific properties
  std::string operator_name_;
  std::string country_code_;
  bool allow_roaming_ = false;
  bool provider_requires_roaming_ = false;
  bool support_network_scan_ = false;
  bool scanning_ = false;
  std::string technology_family_;
  std::string sim_lock_type_;
  int sim_retries_left_ = 0;
  bool sim_lock_enabled_ = false;
  bool sim_present_ = true;
  std::string meid_;
  std::string imei_;
  std::string iccid_;
  std::string mdn_;
  CellularScanResults scan_results_;

  // Ethernet specific properties
  bool eap_authentication_completed_ = false;
  bool link_up_ = false;
  std::string device_bus_type_;
  std::string mac_address_source_;

  // WiFi specific properties
  std::string available_managed_network_path_;

  // Keep all Device properties in a dictionary for now. See comment above.
  base::DictionaryValue properties_;

  // Dictionary of IPConfig properties, keyed by IpConfig path.
  base::DictionaryValue ip_configs_;

  DISALLOW_COPY_AND_ASSIGN(DeviceState);
};

}  // namespace chromeos

#endif  // CHROMEOS_NETWORK_DEVICE_STATE_H_
