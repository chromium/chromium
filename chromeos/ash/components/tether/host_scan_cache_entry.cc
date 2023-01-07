// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/tether/host_scan_cache_entry.h"

#include "base/check_op.h"
#include "base/memory/ptr_util.h"

namespace ash {

namespace tether {

HostScanCacheEntry::Builder::Builder() = default;

HostScanCacheEntry::Builder::~Builder() = default;

std::unique_ptr<HostScanCacheEntry> HostScanCacheEntry::Builder::Build() {
  DCHECK(!tether_network_guid_.empty());

  return base::WrapUnique(new HostScanCacheEntry(
      tether_network_guid_, device_name_, carrier_, battery_percentage_,
      signal_strength_, setup_required_));
}

HostScanCacheEntry::Builder& HostScanCacheEntry::Builder::SetTetherNetworkGuid(
    const std::string& guid) {
  tether_network_guid_ = guid;
  return *this;
}

HostScanCacheEntry::Builder& HostScanCacheEntry::Builder::SetDeviceName(
    const std::string& name) {
  device_name_ = name;
  return *this;
}

HostScanCacheEntry::Builder& HostScanCacheEntry::Builder::SetCarrier(
    const std::string& c) {
  carrier_ = c;
  return *this;
}

HostScanCacheEntry::Builder& HostScanCacheEntry::Builder::SetBatteryPercentage(
    int percentage) {
  battery_percentage_ = percentage;
  return *this;
}

HostScanCacheEntry::Builder& HostScanCacheEntry::Builder::SetSignalStrength(
    int strength) {
  signal_strength_ = strength;
  return *this;
}

HostScanCacheEntry::Builder& HostScanCacheEntry::Builder::SetSetupRequired(
    bool required) {
  setup_required_ = required;
  return *this;
}

HostScanCacheEntry::HostScanCacheEntry(const std::string& tether_network_guid,
                                       const std::string& device_name,
                                       const std::string& carrier,
                                       int battery_percentage,
                                       int signal_strength,
                                       bool setup_required)
    : tether_network_guid(tether_network_guid),
      device_name(device_name),
      carrier(carrier),
      battery_percentage(battery_percentage),
      signal_strength(signal_strength),
      setup_required(setup_required) {}

HostScanCacheEntry::HostScanCacheEntry(const HostScanCacheEntry& other) =
    default;

HostScanCacheEntry::~HostScanCacheEntry() = default;

bool HostScanCacheEntry::operator==(const HostScanCacheEntry& other) const {
  return tether_network_guid == other.tether_network_guid &&
         device_name == other.device_name && carrier == other.carrier &&
         battery_percentage == other.battery_percentage &&
         signal_strength == other.signal_strength &&
         setup_required == other.setup_required;
}

}  // namespace tether

}  // namespace ash
