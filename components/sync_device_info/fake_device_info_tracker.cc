// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_device_info/fake_device_info_tracker.h"

#include <map>

#include "base/check.h"
#include "base/memory/raw_ptr.h"
#include "base/not_fatal_until.h"
#include "base/notreached.h"
#include "base/ranges/algorithm.h"
#include "components/sync/protocol/sync_enums.pb.h"
#include "components/sync_device_info/device_info.h"

namespace syncer {

FakeDeviceInfoTracker::FakeDeviceInfoTracker() = default;

FakeDeviceInfoTracker::~FakeDeviceInfoTracker() = default;

bool FakeDeviceInfoTracker::IsSyncing() const {
  return !devices_.empty();
}

const DeviceInfo* FakeDeviceInfoTracker::GetDeviceInfo(
    const std::string& client_id) const {
  for (const DeviceInfo* device : devices_) {
    if (device->guid() == client_id) {
      return device;
    }
  }
  return nullptr;
}

std::vector<const DeviceInfo*> FakeDeviceInfoTracker::GetAllDeviceInfo() const {
  std::vector<const DeviceInfo*> devices;
  for (const DeviceInfo* device : devices_) {
    devices.push_back(device);
  }
  return devices;
}

std::vector<const DeviceInfo*> FakeDeviceInfoTracker::GetAllChromeDeviceInfo()
    const {
  return GetAllDeviceInfo();
}

void FakeDeviceInfoTracker::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void FakeDeviceInfoTracker::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

std::map<DeviceInfo::FormFactor, int>
FakeDeviceInfoTracker::CountActiveDevicesByType() const {
  if (device_count_per_type_override_) {
    return *device_count_per_type_override_;
  }

  std::map<DeviceInfo::FormFactor, int> count_by_type;
  for (const syncer::DeviceInfo* device : devices_) {
    count_by_type[device->form_factor()]++;
  }
  return count_by_type;
}

void FakeDeviceInfoTracker::ForcePulseForTest() {
  NOTREACHED_IN_MIGRATION();
}

bool FakeDeviceInfoTracker::IsRecentLocalCacheGuid(
    const std::string& cache_guid) const {
  return local_device_cache_guid_ == cache_guid;
}

void FakeDeviceInfoTracker::Add(const DeviceInfo* device) {
  devices_.push_back(device);
  for (auto& observer : observers_) {
    observer.OnDeviceInfoChange();
  }
}

void FakeDeviceInfoTracker::Add(const std::vector<const DeviceInfo*>& devices) {
  for (auto* device : devices) {
    devices_.push_back(device);
  }
  for (auto& observer : observers_) {
    observer.OnDeviceInfoChange();
  }
}

void FakeDeviceInfoTracker::Add(std::unique_ptr<DeviceInfo> device) {
  owned_devices_.push_back(std::move(device));
  Add(owned_devices_.back().get());
}

void FakeDeviceInfoTracker::Remove(const DeviceInfo* device) {
  const auto remove_it = base::ranges::remove(devices_, device);
  CHECK(remove_it != devices_.end());
  devices_.erase(remove_it);
}

void FakeDeviceInfoTracker::Replace(const DeviceInfo* old_device,
                                    const DeviceInfo* new_device) {
  auto it = base::ranges::find(devices_, old_device);
  CHECK(devices_.end() != it, base::NotFatalUntil::M130)
      << "Tracker doesn't contain device";
  *it = new_device;
  for (auto& observer : observers_) {
    observer.OnDeviceInfoChange();
  }
}

void FakeDeviceInfoTracker::OverrideActiveDeviceCount(
    const std::map<DeviceInfo::FormFactor, int>& counts) {
  device_count_per_type_override_ = counts;
  for (auto& observer : observers_) {
    observer.OnDeviceInfoChange();
  }
}

void FakeDeviceInfoTracker::SetLocalCacheGuid(const std::string& cache_guid) {
  // ensure that this cache guid is present in the tracker.
  DCHECK(GetDeviceInfo(cache_guid));
  local_device_cache_guid_ = cache_guid;
}

}  // namespace syncer
