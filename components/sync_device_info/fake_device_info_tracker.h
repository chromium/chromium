// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_DEVICE_INFO_FAKE_DEVICE_INFO_TRACKER_H_
#define COMPONENTS_SYNC_DEVICE_INFO_FAKE_DEVICE_INFO_TRACKER_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/observer_list.h"
#include "components/sync_device_info/device_info_tracker.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

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
  std::unique_ptr<DeviceInfo> GetDeviceInfo(
      const std::string& client_id) const override;
  std::vector<std::unique_ptr<DeviceInfo>> GetAllDeviceInfo() const override;
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
  // DeviceInfo stored here are not owned.
  std::vector<const DeviceInfo*> devices_;
  std::string local_device_cache_guid_;
  absl::optional<std::map<DeviceInfo::FormFactor, int>>
      device_count_per_type_override_;
  // Registered observers, not owned.
  base::ObserverList<Observer, true>::Unchecked observers_;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_DEVICE_INFO_FAKE_DEVICE_INFO_TRACKER_H_
