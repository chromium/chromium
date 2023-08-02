// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/device_sync/cryptauth_device_registry.h"

#include <ostream>

#include "base/containers/contains.h"

namespace ash::device_sync {

CryptAuthDeviceRegistry::CryptAuthDeviceRegistry() = default;

CryptAuthDeviceRegistry::~CryptAuthDeviceRegistry() = default;

const CryptAuthDeviceRegistry::InstanceIdToDeviceMap&
CryptAuthDeviceRegistry::instance_id_to_device_map() const {
  return instance_id_to_device_map_;
}

const CryptAuthDevice* CryptAuthDeviceRegistry::GetDevice(
    const std::string& instance_id) const {
  auto it = instance_id_to_device_map_.find(instance_id);
  if (it == instance_id_to_device_map_.end()) {
    return nullptr;
  }

  return &it->second;
}

bool CryptAuthDeviceRegistry::AddDevice(const CryptAuthDevice& device) {
  const CryptAuthDevice* existing_device = GetDevice(device.instance_id());
  if (existing_device && device == *existing_device) {
    return false;
  }

  instance_id_to_device_map_.insert_or_assign(device.instance_id(), device);

  OnDeviceRegistryUpdated();
  return true;
}

bool CryptAuthDeviceRegistry::DeleteDevice(const std::string& instance_id) {
  if (!base::Contains(instance_id_to_device_map_, instance_id)) {
    return false;
  }

  instance_id_to_device_map_.erase(instance_id);

  OnDeviceRegistryUpdated();
  return true;
}

bool CryptAuthDeviceRegistry::SetRegistry(
    const CryptAuthDeviceRegistry::InstanceIdToDeviceMap&
        instance_id_to_device_map) {
  if (instance_id_to_device_map_ == instance_id_to_device_map) {
    return false;
  }

  instance_id_to_device_map_ = instance_id_to_device_map;

  OnDeviceRegistryUpdated();
  return true;
}

base::Value::Dict CryptAuthDeviceRegistry::AsReadableDictionary() const {
  base::Value::Dict dict;
  for (const std::pair<std::string, CryptAuthDevice>& id_device_pair :
       instance_id_to_device_map_) {
    std::string key = id_device_pair.second.device_name +
                      " (ID: " + id_device_pair.second.instance_id() + ")";
    dict.Set(key, id_device_pair.second.AsReadableDictionary());
  }

  return dict;
}

std::ostream& operator<<(std::ostream& stream,
                         const CryptAuthDeviceRegistry& registry) {
  stream << registry.AsReadableDictionary();
  return stream;
}

}  // namespace ash::device_sync
