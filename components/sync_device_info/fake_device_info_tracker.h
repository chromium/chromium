// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_DEVICE_INFO_FAKE_DEVICE_INFO_TRACKER_H_
#define COMPONENTS_SYNC_DEVICE_INFO_FAKE_DEVICE_INFO_TRACKER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/observer_list.h"
#include "base/optional.h"
#include "components/sync_device_info/device_info_tracker.h"

namespace syncer {

class DeviceInfo;

// Fake DeviceInfoTracker to be used in tests.
class FakeDeviceInfoTracker : public DeviceInfoTracker {
 public:
  FakeDeviceInfoTracker();
  ~FakeDeviceInfoTracker() override;

  // DeviceInfoTracker
  bool IsSyncing() const override;
  std::unique_ptr<DeviceInfo> GetDeviceInfo(
      const std::string& client_id) const override;
  std::vector<std::unique_ptr<DeviceInfo>> GetAllDeviceInfo() const override;
  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;
  int CountActiveDevices() const override;
  void ForcePulseForTest() override;
  bool IsRecentLocalCacheGuid(const std::string& cache_guid) const override;

  // Adds a new DeviceInfo entry to |devices_|.
  void Add(const DeviceInfo* device);

  // Overrides the result of CountActiveDevices() to |count| instead of the
  // actual number of devices in |devices_|.
  void OverrideActiveDeviceCount(int count);

  // Marks an existing DeviceInfo entry as being on the local device.
  void SetLocalCacheGuid(const std::string& cache_guid);

 private:
  // DeviceInfo stored here are not owned.
  std::vector<const DeviceInfo*> devices_;
  std::string local_device_cache_guid_;
  base::Optional<int> active_device_count_;
  // Registered observers, not owned.
  base::ObserverList<Observer, true>::Unchecked observers_;

  DISALLOW_COPY_AND_ASSIGN(FakeDeviceInfoTracker);
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_DEVICE_INFO_FAKE_DEVICE_INFO_TRACKER_H_
