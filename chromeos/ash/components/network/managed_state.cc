// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/network/managed_state.h"

#include <stdint.h>

#include <memory>

#include "base/logging.h"
#include "base/values.h"
#include "chromeos/ash/components/network/device_state.h"
#include "chromeos/ash/components/network/network_event_log.h"
#include "chromeos/ash/components/network/network_state.h"
#include "chromeos/ash/components/network/network_type_pattern.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace ash {

bool ManagedState::Matches(const NetworkTypePattern& pattern) const {
  return pattern.MatchesType(type());
}

// static
std::string ManagedState::TypeToString(ManagedType type) {
  switch (type) {
    case MANAGED_TYPE_NETWORK:
      return "Network";
    case MANAGED_TYPE_DEVICE:
      return "Device";
  }
  return "Unknown";
}

ManagedState::ManagedState(ManagedType type, const std::string& path)
    : managed_type_(type), path_(path) {}

ManagedState::~ManagedState() = default;

std::unique_ptr<ManagedState> ManagedState::Create(ManagedType type,
                                                   const std::string& path) {
  switch (type) {
    case MANAGED_TYPE_NETWORK:
      return std::make_unique<NetworkState>(path);
    case MANAGED_TYPE_DEVICE:
      return std::make_unique<DeviceState>(path);
  }
  return nullptr;
}

NetworkState* ManagedState::AsNetworkState() {
  if (managed_type() == MANAGED_TYPE_NETWORK)
    return static_cast<NetworkState*>(this);
  return nullptr;
}

const NetworkState* ManagedState::AsNetworkState() const {
  if (managed_type() == MANAGED_TYPE_NETWORK)
    return static_cast<const NetworkState*>(this);
  return nullptr;
}

DeviceState* ManagedState::AsDeviceState() {
  if (managed_type() == MANAGED_TYPE_DEVICE)
    return static_cast<DeviceState*>(this);
  return nullptr;
}

const DeviceState* ManagedState::AsDeviceState() const {
  if (managed_type() == MANAGED_TYPE_DEVICE)
    return static_cast<const DeviceState*>(this);
  return nullptr;
}

bool ManagedState::InitialPropertiesReceived(
    const base::Value::Dict& properties) {
  return false;
}

void ManagedState::GetStateProperties(base::Value::Dict* dictionary) const {
  dictionary->Set(shill::kNameProperty, name());
  dictionary->Set(shill::kTypeProperty, type());
}

bool ManagedState::ManagedStatePropertyChanged(const std::string& key,
                                               const base::Value& value) {
  if (key == shill::kNameProperty) {
    return GetStringValue(key, value, &name_);
  } else if (key == shill::kTypeProperty) {
    return GetStringValue(key, value, &type_);
  }
  return false;
}

bool ManagedState::GetBooleanValue(const std::string& key,
                                   const base::Value& value,
                                   bool* out_value) {
  if (!value.is_bool()) {
    NET_LOG(ERROR) << "Error parsing state value: " << NetworkPathId(path_)
                   << "." << key;
    return false;
  }
  bool new_value = value.GetBool();
  if (*out_value == new_value)
    return false;
  *out_value = new_value;
  return true;
}

bool ManagedState::GetIntegerValue(const std::string& key,
                                   const base::Value& value,
                                   int* out_value) {
  if (!value.is_int()) {
    NET_LOG(ERROR) << "Error parsing state value: " << NetworkPathId(path_)
                   << "." << key;
    return false;
  }
  if (*out_value == value.GetInt())
    return false;
  *out_value = value.GetInt();
  return true;
}

bool ManagedState::GetStringValue(const std::string& key,
                                  const base::Value& value,
                                  std::string* out_value) {
  if (!value.is_string()) {
    NET_LOG(ERROR) << "Error parsing state value: " << NetworkPathId(path_)
                   << "." << key;
    return false;
  }
  if (*out_value == value.GetString())
    return false;
  *out_value = value.GetString();
  return true;
}

bool ManagedState::GetUInt32Value(const std::string& key,
                                  const base::Value& value,
                                  uint32_t* out_value) {
  // base::Value restricts the number types to BOOL, INTEGER, and DOUBLE only.
  // uint32_t will automatically get converted to a double, which is why we try
  // to obtain the value as a double (see dbus/values_util.h).
  uint32_t new_value;
  double double_value = value.GetIfDouble().value_or(-1);
  if (double_value < 0) {
    NET_LOG(ERROR) << "Error parsing state value: " << NetworkPathId(path_)
                   << "." << key;
    return false;
  }
  new_value = static_cast<uint32_t>(double_value);
  if (*out_value == new_value)
    return false;
  *out_value = new_value;
  return true;
}

}  // namespace ash
