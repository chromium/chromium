// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/multidevice/expiring_remote_device_cache.h"

#include "base/containers/contains.h"
#include "chromeos/ash/components/multidevice/remote_device_cache.h"

namespace ash::multidevice {

ExpiringRemoteDeviceCache::ExpiringRemoteDeviceCache()
    : remote_device_cache_(RemoteDeviceCache::Factory::Create()) {}

ExpiringRemoteDeviceCache::~ExpiringRemoteDeviceCache() = default;

void ExpiringRemoteDeviceCache::SetRemoteDevicesAndInvalidateOldEntries(
    const RemoteDeviceList& remote_devices) {
  remote_device_cache_->SetRemoteDevices(remote_devices);

  legacy_device_ids_from_last_set_call_.clear();
  instance_ids_from_last_set_call_.clear();
  for (const auto& device : remote_devices)
    RememberIdsFromLastSetCall(device);
}

RemoteDeviceRefList ExpiringRemoteDeviceCache::GetNonExpiredRemoteDevices()
    const {
  // Only add to the output list if the entry is not stale.
  RemoteDeviceRefList remote_devices;
  for (auto device : remote_device_cache_->GetRemoteDevices()) {
    if ((!device.instance_id().empty() &&
         base::Contains(instance_ids_from_last_set_call_,
                        device.instance_id())) ||
        (!device.GetDeviceId().empty() &&
         base::Contains(legacy_device_ids_from_last_set_call_,
                        device.GetDeviceId()))) {
      remote_devices.push_back(device);
    }
  }

  return remote_devices;
}

void ExpiringRemoteDeviceCache::UpdateRemoteDevice(
    const RemoteDevice& remote_device) {
  remote_device_cache_->SetRemoteDevices({remote_device});
  RememberIdsFromLastSetCall(remote_device);
}

std::optional<RemoteDeviceRef> ExpiringRemoteDeviceCache::GetRemoteDevice(
    const std::optional<std::string>& instance_id,
    const std::optional<std::string>& legacy_device_id) const {
  return remote_device_cache_->GetRemoteDevice(instance_id, legacy_device_id);
}

void ExpiringRemoteDeviceCache::RememberIdsFromLastSetCall(
    const RemoteDevice& device) {
  if (!device.instance_id.empty())
    instance_ids_from_last_set_call_.insert(device.instance_id);

  if (!device.GetDeviceId().empty())
    legacy_device_ids_from_last_set_call_.insert(device.GetDeviceId());
}

}  // namespace ash::multidevice
