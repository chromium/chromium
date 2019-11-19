// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_device_info/fake_device_info_tracker.h"

#include "base/logging.h"
#include "components/sync_device_info/device_info.h"

namespace {

// static
std::unique_ptr<syncer::DeviceInfo> CloneDeviceInfo(
    const syncer::DeviceInfo& device_info) {
  return std::make_unique<syncer::DeviceInfo>(
      device_info.guid(), device_info.client_name(),
      device_info.chrome_version(), device_info.sync_user_agent(),
      device_info.device_type(), device_info.signin_scoped_device_id(),
      device_info.hardware_info(), device_info.last_updated_timestamp(),
      device_info.send_tab_to_self_receiving_enabled(),
      device_info.sharing_info());
}

}  // namespace

namespace syncer {

FakeDeviceInfoTracker::FakeDeviceInfoTracker() = default;

FakeDeviceInfoTracker::~FakeDeviceInfoTracker() = default;

bool FakeDeviceInfoTracker::IsSyncing() const {
  return !devices_.empty();
}

std::unique_ptr<DeviceInfo> FakeDeviceInfoTracker::GetDeviceInfo(
    const std::string& client_id) const {
  for (const DeviceInfo* device : devices_) {
    if (device->guid() == client_id) {
      return CloneDeviceInfo(*device);
    }
  }
  return nullptr;
}

std::vector<std::unique_ptr<DeviceInfo>>
FakeDeviceInfoTracker::GetAllDeviceInfo() const {
  std::vector<std::unique_ptr<DeviceInfo>> list;

  for (const DeviceInfo* device : devices_)
    list.push_back(CloneDeviceInfo(*device));

  return list;
}

void FakeDeviceInfoTracker::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void FakeDeviceInfoTracker::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

int FakeDeviceInfoTracker::CountActiveDevices() const {
  return active_device_count_.value_or(devices_.size());
}

void FakeDeviceInfoTracker::ForcePulseForTest() {
  NOTREACHED();
}

bool FakeDeviceInfoTracker::IsRecentLocalCacheGuid(
    const std::string& cache_guid) const {
  return local_device_cache_guid_ == cache_guid;
}

void FakeDeviceInfoTracker::Add(const DeviceInfo* device) {
  devices_.push_back(device);
  for (auto& observer : observers_)
    observer.OnDeviceInfoChange();
}

void FakeDeviceInfoTracker::OverrideActiveDeviceCount(int count) {
  active_device_count_ = count;
  for (auto& observer : observers_)
    observer.OnDeviceInfoChange();
}

void FakeDeviceInfoTracker::SetLocalCacheGuid(const std::string& cache_guid) {
  // ensure that this cache guid is present in the tracker.
  DCHECK(GetDeviceInfo(cache_guid));
  local_device_cache_guid_ = cache_guid;
}

}  // namespace syncer
