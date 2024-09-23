// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_DEVICE_INFO_FAKE_DEVICE_INFO_TRACKER_H_
#define COMPONENTS_SYNC_DEVICE_INFO_FAKE_DEVICE_INFO_TRACKER_H_

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "components/sync_device_info/device_info_tracker.h"

namespace sync_pb {
enum SharingSpecificFields_EnabledFeatures : int;
enum SyncEnums_DeviceType : int;
}  // namespace sync_pb

namespace syncer {

class DeviceInfo;

// Fake DeviceInfoTracker to be used in tests.
class FakeDeviceInfoTracker : public DeviceInfoTracker {
 public:
  FakeDeviceInfoTracker();

  FakeDeviceInfoTracker(const FakeDeviceInfoTracker&) = delete;
  FakeDeviceInfoTracker& operator=(const FakeDeviceInfoTracker&) = delete;

  ~FakeDeviceInfoTracker() override;

  // DeviceInfoTracker
  bool IsSyncing() const override;
  const DeviceInfo* GetDeviceInfo(const std::string& client_id) const override;
  std::vector<const DeviceInfo*> GetAllDeviceInfo() const override;
  std::vector<const DeviceInfo*> GetAllChromeDeviceInfo() const override;
  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;
  std::map<DeviceInfo::FormFactor, int> CountActiveDevicesByType()
      const override;
  void ForcePulseForTest() override;
  bool IsRecentLocalCacheGuid(const std::string& cache_guid) const override;

  // Adds a new DeviceInfo entry to |devices_|.
  void Add(const DeviceInfo* device);

  // Adds a vector of new DeviceInfo entries to |devices_|.
  void Add(const std::vector<const DeviceInfo*>& devices);

  // Overload that allows passing ownership.
  void Add(std::unique_ptr<DeviceInfo> device);

  // Removes a DeviceInfo entry from the device list.
  // FakeDeviceInfoTracker keeps raw pointers to previously added devices, so
  // clients should take care of removing them after those are destroyed if the
  // FakeDeviceInfoTracker may outlive them.
  void Remove(const DeviceInfo* device);

  // Replaces |old_device| with |new_device|. |old_device| must be present in
  // the tracker.
  void Replace(const DeviceInfo* old_device, const DeviceInfo* new_device);

  // Overrides the result of CountActiveDevicesByType() to |counts| instead of
  // the actual number of devices in |devices_|.
  void OverrideActiveDeviceCount(
      const std::map<DeviceInfo::FormFactor, int>& counts);

  // Marks an existing DeviceInfo entry as being on the local device.
  void SetLocalCacheGuid(const std::string& cache_guid);

 private:
  // Owned DeviceInfo instances (subset of all devices).
  std::vector<std::unique_ptr<DeviceInfo>> owned_devices_;
  // DeviceInfo stored here are not necessarily owned.
  std::vector<raw_ptr<const DeviceInfo, VectorExperimental>> devices_;
  std::string local_device_cache_guid_;
  std::optional<std::map<DeviceInfo::FormFactor, int>>
      device_count_per_type_override_;
  // Registered observers, not owned.
  base::ObserverList<Observer, true>::Unchecked observers_;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_DEVICE_INFO_FAKE_DEVICE_INFO_TRACKER_H_
