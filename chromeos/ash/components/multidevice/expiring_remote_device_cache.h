// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_MULTIDEVICE_EXPIRING_REMOTE_DEVICE_CACHE_H_
#define CHROMEOS_ASH_COMPONENTS_MULTIDEVICE_EXPIRING_REMOTE_DEVICE_CACHE_H_

#include <memory>
#include <optional>
#include <string>

#include "base/containers/flat_set.h"
#include "chromeos/ash/components/multidevice/remote_device.h"
#include "chromeos/ash/components/multidevice/remote_device_ref.h"

namespace ash::multidevice {

class RemoteDeviceCache;

// A helper class around RemoteDeviceCache which keeps track of which devices
// have been removed from, or "expired" in, the cache.
//
// If the set of devices provided to SetRemoteDevicesAndInvalidateOldEntries()
// is different from the set provided to a previous call to
// SetRemoteDevicesAndInvalidateOldEntries(), then the devices in the previous
// call which are not in the new call will be marked as stale. Stale devices are
// still valid RemoteDeviceRefs (preventing clients from segfaulting), but will
// not be returned by GetNonExpiredRemoteDevices().
//
// Note: Because RemoteDeviceCache supports both Instance IDs and legacy device
// IDs, ExpiringRemoteDeviceCache does the same.
class ExpiringRemoteDeviceCache {
 public:
  ExpiringRemoteDeviceCache();

  ExpiringRemoteDeviceCache(const ExpiringRemoteDeviceCache&) = delete;
  ExpiringRemoteDeviceCache& operator=(const ExpiringRemoteDeviceCache&) =
      delete;

  virtual ~ExpiringRemoteDeviceCache();

  void SetRemoteDevicesAndInvalidateOldEntries(
      const RemoteDeviceList& remote_devices);

  RemoteDeviceRefList GetNonExpiredRemoteDevices() const;

  // Add or update a RemoteDevice without marking any other devices in the cache
  // as stale.
  void UpdateRemoteDevice(const RemoteDevice& remote_device);

  // Looks up device in cache by Instance ID, |instance_id|, and by the legacy
  // device ID from RemoteDevice::GetDeviceId(), |legacy_device_id|. Returns the
  // first device that matches either ID. Returns null if no such device exists.
  //
  // For best results, pass in both IDs when available since the device could
  // have been written to the cache with one of the IDs missing.
  std::optional<RemoteDeviceRef> GetRemoteDevice(
      const std::optional<std::string>& instance_id,
      const std::optional<std::string>& legacy_device_id) const;

 private:
  void RememberIdsFromLastSetCall(const RemoteDevice& device);

  std::unique_ptr<RemoteDeviceCache> remote_device_cache_;

  base::flat_set<std::string> legacy_device_ids_from_last_set_call_;
  base::flat_set<std::string> instance_ids_from_last_set_call_;
};

}  // namespace ash::multidevice

#endif  // CHROMEOS_ASH_COMPONENTS_MULTIDEVICE_EXPIRING_REMOTE_DEVICE_CACHE_H_
