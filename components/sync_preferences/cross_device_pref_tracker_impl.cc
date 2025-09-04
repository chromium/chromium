// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_preferences/cross_device_pref_tracker_impl.h"

#include "base/check.h"
#include "components/prefs/pref_service.h"
#include "components/sync_device_info/device_info_sync_service.h"

namespace sync_preferences {

CrossDevicePrefTrackerImpl::CrossDevicePrefTrackerImpl(
    PrefService* profile_pref_service,
    PrefService* local_pref_service,
    syncer::DeviceInfoSyncService* device_info_sync_service)
    : profile_pref_service_(profile_pref_service),
      local_pref_service_(local_pref_service),
      device_info_sync_service_(device_info_sync_service) {
  CHECK(profile_pref_service_);
  CHECK(local_pref_service_);
  CHECK(device_info_sync_service_);

  // Initialize the registrars with the corresponding `PrefService`.
  profile_pref_registrar_.Init(profile_pref_service_);
  local_pref_registrar_.Init(local_pref_service_);

  // Start observing the `DeviceInfoTracker`. This is required to map remote
  // Cache GUIDs to device metadata (OS type, form factor).
  if (syncer::DeviceInfoTracker* tracker =
          device_info_sync_service_->GetDeviceInfoTracker()) {
    device_info_observation_.Observe(tracker);
  }

  // TODO(crbug.com/441330511): Initialize tracking for specific Prefs based on
  // the static maps (will be done in a follow-up CL).
}

CrossDevicePrefTrackerImpl::~CrossDevicePrefTrackerImpl() {
  // `Shutdown()` should have been called by the `KeyedService` infrastructure.
  CHECK(!profile_pref_service_);
  CHECK(!local_pref_service_);
  CHECK(!device_info_sync_service_);
}

void CrossDevicePrefTrackerImpl::AddObserver(
    CrossDevicePrefTracker::Observer* observer) {
  observers_.AddObserver(observer);
}

void CrossDevicePrefTrackerImpl::RemoveObserver(
    CrossDevicePrefTracker::Observer* observer) {
  observers_.RemoveObserver(observer);
}

std::vector<TimestampedPrefValue> CrossDevicePrefTrackerImpl::GetValues(
    std::string_view pref_name,
    const DeviceFilter& filter) const {
  // TODO(crbug.com/441330219): Implement the Query API.

  return {};
}

std::optional<TimestampedPrefValue>
CrossDevicePrefTrackerImpl::GetMostRecentValue(
    std::string_view pref_name,
    const DeviceFilter& filter) const {
  // TODO(crbug.com/441330219): Implement the Query API.

  return std::nullopt;
}

void CrossDevicePrefTrackerImpl::Shutdown() {
  profile_pref_registrar_.RemoveAll();
  local_pref_registrar_.RemoveAll();
  device_info_observation_.Reset();

  profile_pref_service_ = nullptr;
  local_pref_service_ = nullptr;
  device_info_sync_service_ = nullptr;
}

void CrossDevicePrefTrackerImpl::OnDeviceInfoChange() {
  // `DeviceInfo` changes are relevant for two main reasons:
  // 1. Metadata (OS/FormFactor) might change, affecting query results or
  //    observer notifications.
  // 2. This is the signal to perform garbage collection of stale Cache GUIDs
  //    from the syncable dictionary Prefs (as noted in the design doc).

  // TODO(crbug.com/441330511): Implement garbage collection and handle metadata
  // updates.
}

}  // namespace sync_preferences
