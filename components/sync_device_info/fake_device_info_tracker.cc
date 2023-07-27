// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_device_info/fake_device_info_tracker.h"

#include <map>

#include "base/check.h"
#include "base/notreached.h"
#include "base/ranges/algorithm.h"
#include "components/sync/protocol/sync_enums.pb.h"
#include "components/sync_device_info/device_info.h"

namespace {

// static
std::unique_ptr<syncer::DeviceInfo> CloneDeviceInfo(
    const syncer::DeviceInfo& device_info) {
  return std::make_unique<syncer::DeviceInfo>(
      device_info.guid(), device_info.client_name(),
      device_info.chrome_version(), device_info.sync_user_agent(),
      device_info.device_type(), device_info.os_type(),
      device_info.form_factor(), device_info.signin_scoped_device_id(),
      device_info.manufacturer_name(), device_info.model_name(),
      device_info.full_hardware_class(), device_info.last_updated_timestamp(),
      device_info.pulse_interval(),
      device_info.send_tab_to_self_receiving_enabled(),
      device_info.sharing_info(), device_info.paask_info(),
      device_info.fcm_registration_token(),
      device_info.interested_data_types());
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

  for (const DeviceInfo* device : devices_) {
    list.push_back(CloneDeviceInfo(*device));
  }

  return list;
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
  for (const auto* device : devices_) {
    count_by_type[device->form_factor()]++;
  }
  return count_by_type;
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

void FakeDeviceInfoTracker::Remove(const DeviceInfo* device) {
  const auto remove_it = base::ranges::remove(devices_, device);
  CHECK(remove_it != devices_.end());
  devices_.erase(remove_it);
}

void FakeDeviceInfoTracker::Replace(const DeviceInfo* old_device,
                                    const DeviceInfo* new_device) {
  std::vector<const DeviceInfo*>::iterator it =
      base::ranges::find(devices_, old_device);
  DCHECK(devices_.end() != it) << "Tracker doesn't contain device";
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
